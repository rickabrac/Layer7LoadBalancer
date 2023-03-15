//
//  Service.h
//  L7LB (Layer7LoadBalancer)
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

# ifndef _Service_h_
# define _Service_h_

# include "Thread.h"
# include "Session.h"
# include "SocketAddress.h"
# include "Event.h"

class Service;

class ServiceContext : public ThreadContext
{
    public:
        ServiceContext( const char *listenStr, const char *certPath = nullptr, const char *keyPath = nullptr, const char *trustPath = nullptr );
		~ServiceContext();
		Service *service;

	private:
		SocketAddress *sockAddr;
		const char *listenStr;
		static SSL_CTX *ssl_ctx;
		int socket;
		const char *certPath;
		const char *trustPath;
		const char *keyPath;

	friend class Service;
};

class Service : public Thread
{
	public:
		Service( ServiceContext *context );
		~Service();
		ssize_t clientPeek( int clientSocket, SSL *clientSSL, void *buf, size_t len );

	protected:
		static void _main( ServiceContext *context );

	private:
		ServiceContext *context; 
		virtual Session *getSession( int clientSocket, SSL *clientSSL = nullptr ) = 0;
		bool getCookie( const char *name ); 
		bool isSecure( void );
		map< string, Session * > sessions;
		mutex sessionMutex;
		static mutex bufLenMutex;
		static size_t bufLen;

	friend class Session;
	friend class SessionContext;
	friend class ProxySession;
	friend class ProxySessionContext;
};

# endif // _Service_h_

