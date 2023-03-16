//
//  Exception.h
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

# ifndef _Exception_h_
# define _Exception_h_

class Exception
{
    public:

	static void raise( const char *fmt, ... )
	{
		static mutex mutex;
		mutex.lock();
		va_list args;
		va_start( args, fmt );
		static char buf[ 8192 ];
		vsprintf( buf, fmt, args );
		va_end( args );
		throw( (const char *) buf );
		mutex.unlock();
	}
};

# endif // _Exception_h_
