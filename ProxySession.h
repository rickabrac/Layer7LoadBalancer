//
//  ProxySession.h
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

# ifndef _ProxySession_h_
# define _ProxySession_h_

# include "Thread.h"
# include "Service.h"
# include "Session.h"
# include "Connection.h"

class ProxySessionContext : public SessionContext
{
	public:
		ProxySessionContext( Service *service, int clientSocket, SSL *clientSSL, const char *destStr, bool useTLS );
		~ProxySessionContext();

	private:
		char *buf;
		size_t bufLen;
		const char *destStr;
		bool useTLS; 
		Connection *proxy = nullptr;

	friend class ProxySession;
};

class ProxySession : public Session 
{
	public:
		ProxySession( ProxySessionContext *context );
		virtual ~ProxySession();
		void stop( void );

	private:
		static void _main( ProxySessionContext *context );
		ThreadMain main( void ) { return( (ThreadMain) _main ); }
		ProxySessionContext *context;

	friend class Service; 
};

# endif // _ProxySession_h_

