//
//  Service.h
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

	ServiceContext (
		const char *listenStr,
		const char *certPath = nullptr,
		const char *keyPath = nullptr,
		const char *trustPath = nullptr
	);
	~ServiceContext();
	Service *service;

    private:

	const char *listenStr;
	const char *certPath;
	const char *trustPath;
	const char *keyPath;
	SocketAddress *sockAddr;
	int socket;
	SSL_CTX *get_SSL_CTX( void ); 
	// void notifyEndOfSession( SessionContext *sessionContext );

    friend class Service;
};

class Service : public Thread
{
    public:

	Service( ServiceContext *context );
	~Service();

    protected:

	static void _main( ServiceContext *context );

    private:

	ServiceContext *context; 
	virtual Session *getSession( int clientSocket, SSL *clientSSL = nullptr ) = 0;
	virtual void notifySessionProtocolAttribute( string *value );
	bool isSecure( void );
	void notifyEndOfSession( SessionContext *context );
	static mutex bufLenMutex;
	static size_t bufLen;
	static SSL_CTX *ssl_ctx;
	static mutex ssl_ctx_mutex;
	static set< SessionContext * > sslSessions;

    friend class Session;
    friend class SessionContext;
    friend class ProxySession;
    friend class ProxySessionContext;
};

# endif // _Service_h_

