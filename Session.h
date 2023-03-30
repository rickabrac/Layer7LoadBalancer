//
//  Session.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# ifndef _Session_h_
# define _Session_h_

# include "Thread.h"
# include <openssl/ssl.h>
# include <set>

class Service;
class Session;

class SessionContext : public ThreadContext
{
    public:

	SessionContext( Service *daemon, int clientSocket, SSL *clientSSL = nullptr );
	~SessionContext();
	int clientSocket;
	SSL *clientSSL;

    protected:

	Service *service;
	Session *session = nullptr;

    friend class Session;
    friend class Service;
};

class Session : public Thread
{
    public:

	Session( SessionContext *context );
	virtual ~Session();

    private:

	SessionContext *context;

    friend class Service; 
};

# endif // _Session_h_

