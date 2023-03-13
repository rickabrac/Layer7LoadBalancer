//
//  Session.cc
//
//  Created by Rick Tyler
//

# include "Session.h"
# include "Service.h"
# include "Exception.h"
# include "Event.h"
# include "Log.h"

// # define TRACE    1

using namespace std;

SessionContext :: SessionContext( Service *service, int clientSocket, SSL *clientSSL )
{
# if TRACE
	Log::log( "SessionContext::SessionContext()" );
# endif // TRACE
	this->service= service;
	this->clientSocket = clientSocket;
	this->clientSSL = clientSSL;
}

SessionContext :: ~SessionContext()
{
# if TRACE
	Log::log( "SessionContext::~SessionContext()" );
# endif // TRACE
	if( service->isSecure() )
	{
		SSL_shutdown( clientSSL );
		SSL_free( clientSSL );
	}
	(void) close( clientSocket );
	if( session )
		delete( session );
}

Session :: Session( SessionContext *context ) : Thread( context ) 
{
# if TRACE
	Log::log( "Session::Session()" );
# endif // TRACE
	context->session = this;
}

Session :: ~Session()
{
# if TRACE
	Log::log( "Session::~Session()" );
# endif // TRACE
}

