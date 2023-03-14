//
//  ProxySession.h
//  L7LB (Layer7LoadBalancer)
//  Created by Rick Tyler
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

	protected:

	private:

        static void _main( ProxySessionContext *context );
        ThreadMain main( void ) { return( (ThreadMain) _main ); }
		ProxySessionContext *context;

	friend class Service; 
};

# endif // _ProxySession_h_

