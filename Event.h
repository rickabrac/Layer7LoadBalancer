//
//  Event.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# ifndef _Event_h_
# define _Event_h_

# include <pthread.h>

class Event
{
    public:

	Event ( void )
	{
		int error;
		if( (error = pthread_cond_init( &condition, NULL )) != 0 )
		{
			Log::console( "Event::Event: pthread_conf_init() failed (%d)", error );
			::exit( -1 );
		}
		if( (error = pthread_mutex_init( &mutex, NULL )) != 0 )
		{
			Log::console( "Event::Event: pthread_mutex_init() failed (%d)", error );
			::exit( -1 );
		}
	}

	~Event ( void )
	{
		int error;
		if( (error = pthread_cond_destroy( &condition )) != 0 )
		{
			Log::console( "Event::Event: pthread_cond_destroy() failed (%d)", error );
			::exit( -1 );
		}
		if( (error = pthread_mutex_destroy( &mutex )) != 0 )
		{
			Log::console( "Event::Event: pthread_mutex_destroy() failed (%d)", error );
			::exit( -1 );
		}
	}

	void
	signal ( void )
	{
		int error;
		if( (error = pthread_cond_signal( &condition )) != 0 )
		{
			Log::console( "Event::Event: pthread_cond_signal() failed (%d)", error );
			::exit( -1 );
		}
	}

	void
	wait ( void )
	{
		int error;
		if( (error = pthread_mutex_lock( &mutex )) != 0 )
		{
			Log::console( "Event::Event: pthread_mutex_lock() failed (%d)", error );
			::exit( -1 );
		}
		if( (error = pthread_cond_wait( &condition, &mutex )) != 0 )
		{
			Log::console( "Event::Event: pthread_cond_wait() failed (%d)", error );
			::exit( -1 );
		}
		if( (error = pthread_mutex_unlock( &mutex )) != 0 )
		{
			Log::console( "Event::Event: pthread_mutex_unlock() failed (%d)", error );
			::exit( -1 );
		}
	}

    protected:

        pthread_cond_t condition;
        pthread_mutex_t mutex;
};

/*
# include <mutex>

using namespace std;

class Event
{
	public:

	Event( void ) {
		_mutex.lock();
	}

	~Event( void ) {
		_mutex.unlock();
	}

	void signal( void ) {
		_mutex.unlock();
	}

	void wait( void ) {
		_mutex.lock();
	}

	private:

	mutex _mutex;
};
*/

# endif // _Event_h_
