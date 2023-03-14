//
//  Connection.h
//	L7LB (Layer7LoadBalancer)
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

# ifndef _Connection_h_
# define _Connection_h_

# include "SocketAddress.h"
# include <openssl/ssl.h>
# include <openssl/err.h>

using namespace std;

class Connection 
{
	public:

		Connection( const char *destStr, bool secure = true ); 
		~Connection();
		ssize_t write( void *data, size_t len );
		ssize_t peek( void *buf, size_t len );
		ssize_t read( void *buf, size_t len );
		ssize_t pending( void ); 
		const char *error( int errNum );
		static void debug( void );
		int socket;

	private:

		SSL *ssl = nullptr;
		static map< string, Connection * > connections;
		static mutex connectionMutex;
		static SSL_CTX *ssl_ctx;
		SocketAddress *sockAddr = nullptr;
		bool secure;
};

# endif // _Connection_h_

