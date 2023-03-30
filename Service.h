//
//  Service.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

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
	ssize_t peek( int clientSocket, SSL *clientSSL, void *buf, size_t len );

    protected:

	static void _main( ServiceContext *context );

    private:

	ServiceContext *context; 
	virtual Session *getSession( int clientSocket, SSL *clientSSL = nullptr ) = 0;
	virtual void sessionNotifyProtocolAttribute( string *value, void *data = nullptr );
	bool isSecure( void );
	void endSession( SessionContext *context );
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

