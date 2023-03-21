//
//  Thread.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License You may obtain a copy of the License at
//
//  https://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

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

	void sleep( int millliseconds )
	{
		std::chrono::duration<int64_t, std::ratio<1,1000>> duration( millliseconds );
		std::this_thread::sleep_for( duration );
	}

	ThreadContext *context;

    private:

	thread *t;
};

# endif // _Thread_h_

