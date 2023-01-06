#include "pch.hpp"

static std::size_t                           fiber_count{ 0 };
static std::mutex                            mtx_count{};
static boost::fibers::condition_variable_any cnd_count{};

using lock_type = std::unique_lock<std::mutex>;

void whatevah( char me )
{
  try
  {
    std::thread::id my_thread = std::this_thread::get_id(); // get ID of initial thread

    {
      std::ostringstream buffer;
      buffer << "fiber " << me << " started on thread " << my_thread << '\n';
      std::cout << buffer.str() << std::flush;
    }

    for( unsigned i = 0; i < 10; ++i )
    {
      boost::this_fiber::yield();                              // yield to other fibers
      std::thread::id new_thread = std::this_thread::get_id(); // get ID of current thread

      if( new_thread != my_thread ) // test if fiber was migrated to another thread
      {
        my_thread = new_thread;
        std::ostringstream buffer;
        buffer << "fiber " << me << " switched to thread " << my_thread << '\n';
        std::cout << buffer.str() << std::flush;
      }
    }
  }
  catch( ... )
  {
  }

  lock_type lk( mtx_count );

  if( 0 == --fiber_count ) // Decrement fiber counter for each completed fiber.
  {
    lk.unlock();
    cnd_count.notify_all(); // Notify all fibers waiting on `cnd_count`.
  }
}


void thread()
{
  std::ostringstream buffer;
  buffer << "thread started " << std::this_thread::get_id() << std::endl;
  std::cout << buffer.str() << std::flush;

  // Install the scheduling algorithm `boost::fibers::algo::work_stealing` in order to join the work sharing.
  boost::fibers::use_scheduling_algorithm<boost::fibers::algo::work_stealing>( 4 );

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

  // Launch a number of worker fibers; each worker fiber picks up a character
  // that is passed as parameter to fiber-function `whatevah`.
  // Each worker fiber gets detached.
  for( char c: std::string( "abcdefghijklmnopqrstuvwxyz" ) )
  {
    boost::fibers::fiber( [c]() { whatevah( c ); } ).detach();
    ++fiber_count; // Increment fiber counter for each new fiber.
  }

  // Launch a couple of threads that join the work sharing.
  std::thread threads[] = {
      std::thread{ thread },
      std::thread{ thread },
      std::thread{ thread },
  };

  // Install the scheduling algorithm `boost::fibers::algo::work_stealing` in the main thread
  // too, so each new fiber gets launched into the shared pool.
  boost::fibers::use_scheduling_algorithm<boost::fibers::algo::work_stealing>( 4 );

  // Releasing lock of mtx_count is required before joining the threads, otherwise
  // the other threads would be blocked inside condition_variable::wait() and
  // would never return (deadlock).
  {
    // `lock_type` is typedef'ed as unique_lock [@http://en.cppreference.com/w/cpp/thread/mutex]
    lock_type lk( mtx_count );

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

  std::cout << "done." << std::endl;
  return EXIT_SUCCESS;
}