//
//  Thread.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# ifndef _Thread_h_
# define _Thread_h_

# include <thread>
# include <unistd.h>
# include <sstream>

using namespace std;

# include <mutex>
# include "Log.h"

class ThreadContext
{
    public:

	ThreadContext( void ) { }
	~ThreadContext() { }
};

typedef void (*ThreadMain)( ThreadContext * );

class Thread
{
    public:

	Thread( ThreadContext *context ) {
		this->context = context;
	}

	~Thread() {
		delete( t );
	}

	virtual ThreadMain main( void ) = 0;

	void run( void ) {
		t = new thread( main(), context );
	}

	void join( void ) {
		t->join();
	}

	void detach( void ) {
		t->detach();
	}

	void sleep( int millliseconds ) {
		std::chrono::duration<int64_t, std::ratio<1,1000>> duration( millliseconds );
		std::this_thread::sleep_for( duration );
	}

	ThreadContext *context;

    private:

	thread *t;
};

# endif // _Thread_h_

