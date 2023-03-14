//
//  Thread.h
//  L7LB (Layer7LoadBalancer)
//  Created by Rick Tyler
//

# ifndef _Thread_h_
# define _Thread_h_

# include <thread>
# include <unistd.h>
# include <sstream>

using namespace std;

class ThreadContext
{
	public:

		ThreadContext( void ) { }
};

typedef void (*ThreadMain)( ThreadContext * );

class Thread
{
	public:

		Thread( ThreadContext *context )
		{
			this->context = context;
		}

		~Thread()
		{
			delete( t );
		}

		virtual ThreadMain main( void ) = 0;

		void run( void )
		{
			t = new thread( main(), context );
		}

		void join( void )
		{
			t->join();
		}

		void detach( void )
		{
			t->detach();
		}

		static string tid()
		{
			std::ostringstream ss;
			ss << std::this_thread::get_id();
			return ss.str();
		}

		ThreadContext *context;

	private:

		thread *t;
};

# endif // _Thread_h_

