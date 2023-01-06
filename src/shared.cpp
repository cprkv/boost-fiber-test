#include "pch.hpp"

static std::size_t                           fiber_count{ 0 };
static std::mutex                            mtx_count{};
static boost::fibers::condition_variable_any cnd_count{};

using lock_type = std::unique_lock<std::mutex>;

class WorkRecording
{
  std::mutex                                   mutex_;
  std::map<std::thread::id, std::vector<char>> work_;

public:
  void add( std::thread::id id, char c )
  {
    auto lock = lock_type{ mutex_ };
    work_[id].push_back( c );
  }

  void print()
  {
    for( const auto& [id, works]: work_ )
    {
      std::cout << id << ": ";

      for( const auto& work: works )
      {
        std::cout << work;
      }

      std::cout << std::endl;
    }
  }

} work_recording;

void whatevah( char me )
{
  try
  {
    std::thread::id my_thread = std::this_thread::get_id(); // get ID of initial thread

    {
      std::ostringstream buffer;
      buffer << "fiber " << me << " started on thread " << my_thread << '\n';
      std::cout << buffer.str() << std::flush;
      work_recording.add( my_thread, me );
    }

    for( unsigned i = 0; i < 10; ++i ) // loop ten times
    {
      boost::this_fiber::yield();                              // yield to other fibers
      std::thread::id new_thread = std::this_thread::get_id(); // get ID of current thread
      work_recording.add( new_thread, me );

      //if( new_thread != my_thread ) // test if fiber was migrated to another thread
      //{
      //  my_thread = new_thread;
      //  std::ostringstream buffer;
      //  buffer << "fiber " << me << " switched to thread " << my_thread << '\n';
      //  std::cout << buffer.str() << std::flush;
      //}
    }
  }
  catch( ... )
  {
    std::cout << "exception caught\n"
              << std::flush;
  }

  lock_type lk( mtx_count );

  if( 0 == --fiber_count ) // decrement fiber counter for each completed fiber.
  {
    lk.unlock();
    cnd_count.notify_all(); // notify all fibers waiting on `cnd_count`.
  }
}


void thread( boost::fibers::detail::thread_barrier* b )
{
  std::ostringstream buffer;
  buffer << "thread started " << std::this_thread::get_id() << std::endl;
  std::cout << buffer.str() << std::flush;

  // Install the scheduling algorithm `boost::fibers::algo::shared_work` in order to join the work sharing.
  boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>();

  b->wait(); // sync with other threads: allow them to start processing

  lock_type lk( mtx_count );

  // Suspend main fiber and resume worker fibers in the meanwhile.
  // Main fiber gets resumed (e.g returns from `condition_variable_any::wait()`)
  // if all worker fibers are complete.
  cnd_count.wait( lk, []() { return 0 == fiber_count; } );

  BOOST_ASSERT( 0 == fiber_count );
}

int main( int argc, char* argv[] )
{
  std::cout << "main thread started " << std::this_thread::get_id() << std::endl;

  // Install the scheduling algorithm `boost::fibers::algo::shared_work` in the main thread
  // too, so each new fiber gets launched into the shared pool.
  boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>();

  // Launch a number of worker fibers; each worker fiber picks up a character
  // that is passed as parameter to fiber-function `whatevah`.
  // Each worker fiber gets detached.
  for( char c: std::string( "abcdefghijklmnopqrstuvwxyz" ) )
  {
    boost::fibers::fiber( [c]() { whatevah( c ); } ).detach();
    ++fiber_count; // Increment fiber counter for each new fiber.
  }

  boost::fibers::detail::thread_barrier b( 4 );

  // Launch a couple of threads that join the work sharing.
  std::thread threads[] = {
      std::thread( thread, &b ),
      std::thread( thread, &b ),
      std::thread( thread, &b ),
  };

  b.wait(); // sync with other threads: allow them to start processing

  // std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
  // boost::this_fiber::sleep_for( std::chrono::seconds( 5 ) );

  // Releasing lock of mtx_count is required before joining the threads, otherwise
  // the other threads would be blocked inside condition_variable::wait() and
  // would never return (deadlock).
  {
    lock_type lk( mtx_count ); // `lock_type` is typedef'ed as unique_lock: http://en.cppreference.com/w/cpp/thread/mutex

    // Suspend main fiber and resume worker fibers in the meanwhile.
    // Main fiber gets resumed (e.g returns from `condition_variable_any::wait()`)
    // if all worker fibers are complete.
    cnd_count.wait( lk, []() { return 0 == fiber_count; } );
  }

  BOOST_ASSERT( 0 == fiber_count );

  // wait for threads to terminate
  for( std::thread& t: threads )
  {
    t.join();
  }

  work_recording.print();

  std::cout << "done." << std::endl;
  return EXIT_SUCCESS;
}