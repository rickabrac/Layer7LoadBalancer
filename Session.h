//
//  Session.h
//
//  Created by Rick Tyler
//

# ifndef _Session_h_
# define _Session_h_

# include "Thread.h"
# include <openssl/ssl.h>

class Service;
class Session;

class SessionContext : public ThreadContext
{
	public:

        SessionContext( Service *daemon, int clientSocket, SSL *clientSSL = NULL );
		~SessionContext();

	protected:

        Service *service;
		Session *session;
		int clientSocket;
		SSL *clientSSL;

	friend class Session;
	friend class Service;
};

class Session : public Thread
{
	public:

		Session( SessionContext *context );
		virtual ~Session();
		void stop( void );

	private:

		SessionContext *context;

	friend class Service; 
};

# endif // _Session_h_

