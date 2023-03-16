//
//  Log.h
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
