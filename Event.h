//
//  Event.h
//
//  Created by Rick Tyler
//

# ifndef _Event_h_
# define _Event_h_

# include <mutex>

using namespace std;

class Event
{
	public:

		Event( void )
		{
			_mutex.lock();
		}

		~Event( void )
		{
			_mutex.unlock();
		}

		void signal( void )
		{
			_mutex.unlock();
		}

		void wait( void )
		{
			_mutex.lock();
		}

	private:

		mutex _mutex;
};

# endif // _Event_h_
