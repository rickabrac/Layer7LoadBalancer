//
//  Log.h
//
//  Created by Rick Tyler
//

# ifndef _Log_h_
# define _Log_h_

class Log
{
	public:

		static void log( const char *msgFormat, ... )
		{
			static mutex _mutex;
			_mutex.lock();
			static char msgBuf[ 1024 ];
			va_list msgArgs;
			va_start( msgArgs, msgFormat );
			vsprintf( msgBuf, msgFormat, msgArgs );
			va_end( msgArgs );
			printf( "%s\n", msgBuf );
			fflush( stdout );
			_mutex.unlock();
		}

	private:

		static mutex _mutex;
};

# endif // _Log_h_
