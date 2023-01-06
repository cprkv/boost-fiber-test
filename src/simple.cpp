#include "pch.hpp"

using namespace std;

namespace fibers     = boost::fibers;
namespace this_fiber = boost::this_fiber;

inline void fn( string const& str, int n )
{
  for( int i = 0; i < n; ++i )
  {
    cout << i << ": " << str << endl;
    this_fiber::yield();
  }
}

int main()
{
  try
  {
    fibers::fiber f1{ fn, "abc", 5 };
    cerr << "f1 : " << f1.get_id() << endl;
    f1.join();
    cout << "done." << endl;

    return EXIT_SUCCESS;
  }
  catch( exception const& e )
  {
    cerr << "exception: " << e.what() << endl;
  }
  catch( ... )
  {
    cerr << "unhandled exception" << endl;
  }

  return EXIT_FAILURE;
}