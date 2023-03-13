//
//  NetworkService.h
//
//  Abstract network service class
//
//  Created by Rick Tyler
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

        ServiceContext( const char *listenStr, const char *certPath = nullptr, const char *keyPath = nullptr );
		~ServiceContext();
		Service *service;

	private:

		SocketAddress *sockAddr;
		const char *listenStr;
		static SSL_CTX *ssl_ctx;
		int socket;
		const char *certPath;
		const char *keyPath;

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
