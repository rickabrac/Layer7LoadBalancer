//
//  Service.cc
//
//  Created by Rick Tyler
//

# include "Service.h"
# include "Exception.h"
# include "SocketAddress.h"
# include "Event.h"
# include "Log.h"
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <poll.h>
# include <fcntl.h>

// # define TRACE    1

ServiceContext :: ServiceContext( const char *listenStr, const char *certPath, const char *keyPath )
{
# if TRACE
	Log::log( "ServiceContext::ServiceContext()" );
# endif // TRACE
	this->sockAddr = new SocketAddress( listenStr );
	this->certPath = certPath;
	this->keyPath = keyPath;
	this->listenStr = listenStr;
}

ServiceContext :: ~ServiceContext()
{
# if TRACE
	Log::log( "ServiceContext::~ServiceContext()" );
# endif // TRACE
	if( sockAddr )
		delete( sockAddr );
}

SSL_CTX *ServiceContext :: ssl_ctx = NULL;
size_t Service :: bufLen = 262144;
mutex Service :: bufLenMutex;

Service :: Service( ServiceContext *context ) : Thread( context )
{
# if TRACE
	Log::log( "Service::Service()" );
# endif // TRACE
	this->context = context;
	context->service = this;
	try
	{
		if( context->service->isSecure() && !ServiceContext::ssl_ctx )
		{
			OpenSSL_add_all_algorithms();

			SSL_load_error_strings();	

			if( SSL_library_init() < 0 )
				Exception::raise( "SSL_library_init() failed: %s", ERR_error_string( ERR_get_error(), NULL ) );

			if( !(ServiceContext::ssl_ctx = SSL_CTX_new( SSLv23_server_method() )) )
				Exception::raise( "SSL_CTX_new() failed: %s", ERR_error_string( ERR_get_error(), NULL ) );

			SSL_CTX_set_options( ServiceContext::ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1
				| SSL_OP_NO_TLSv1_1 | SSL_OP_NO_COMPRESSION ); // SSL_OP_NO_SSLv2 );

			if( SSL_CTX_use_certificate_file( ServiceContext::ssl_ctx, context->certPath, SSL_FILETYPE_PEM ) != 1 )
			{
				Exception::raise( "SSL_CTX_use_certificate_file() failed: %s",
					ERR_error_string( ERR_get_error(), NULL ) );
			}

			if( SSL_CTX_use_PrivateKey_file( ServiceContext::ssl_ctx, context->keyPath, SSL_FILETYPE_PEM ) != 1 )
			{
				Exception::raise( "SSL_CTX_use_PrivateKey_file() failed: %s",
					ERR_error_string( ERR_get_error(), NULL ) );
			}
		}

		context->socket = socket( AF_INET, SOCK_STREAM, 0 );

		if( context->socket == -1 )
			Exception::raise( "socket() failed: %s", strerror( errno ) );

        if( fcntl( context->socket, F_SETFD, 1 ) == -1 )
            Exception::raise( "fcntl( F_SETFD, 1 ) failed", strerror( errno ) );

		// set SO_REUSEADDR so bind() doesn't fail after restart
		int optval = 1;
		if( setsockopt( context->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) ) < 0 )
		{
			Exception::raise( "setsockopt( SO_REUSEADDR ) on listen socket failed (%s)", strerror( errno ) );
		}

		if( ::bind( context->socket, (struct sockaddr *) &context->sockAddr->sockaddr_in,
			sizeof( struct sockaddr_in ) ) != 0 )
		{
			Exception::raise( "bind() failed (%s)", strerror( errno ) );
		}

		if( listen( context->socket, -1 ) != 0 )
			Exception::raise( "listen() failed (%s)", strerror( errno ) );
	}
	catch( const char *error )
	{
		Log::log( "Service::_main: %s", error );
		::exit( -1 );
	}
}

Service :: ~Service()
{
# if TRACE
	Log::log( "Service::~Service()" );
# endif // TRACE
	if( ServiceContext::ssl_ctx )
		SSL_CTX_free( ServiceContext::ssl_ctx );
	close( context->socket );
}

bool Service :: isSecure()
{
	return( *context->certPath && *context->keyPath );
}

void Service :: _main( ServiceContext *context )
{
# if TRACE
	Log::log( "Service::_main()" );
# endif // TRACE
	for( ;; )
	{
		try
		{
# if TRACE
			Log::log( "Service::_main: accept()..." );
# endif // TRACE

			int clientSocket;
			socklen_t socklen = sizeof( struct sockaddr_in );
			if( (clientSocket = accept( context->socket, (struct sockaddr *) &context->sockAddr->sockaddr_in, &socklen )) < 0 )
			{
				Exception::raise( "accept() failed (%s) [%d]", strerror( errno ), errno );
			}
# if TRACE
			Log::log( "Service::_main: accept() (clientSocket=%d)", clientSocket );
# endif // TRACE

           	struct pollfd server_poll;
           	server_poll.fd = context->socket;
           	server_poll.events = POLLIN;

			SSL *clientSSL = nullptr;

			if( context->service->isSecure() )
			{
				if( (clientSSL = SSL_new( ServiceContext::ssl_ctx )) == NULL )
				{
					Exception::raise( "SSL_new() failed (%s)", ERR_error_string( ERR_get_error(), NULL ) );
				}
# if TRACE
				Log::log( "Service::main: clientSocket=%d clientSSL=<%p>", clientSocket, clientSSL );
# endif // TRACE

				if( !SSL_set_fd( clientSSL, clientSocket ) )
				{
					Exception::raise( "SSL_set_fd() failed (%s)",
						ERR_error_string( ERR_get_error(), NULL ) ); 
				}

				int SSL_accepted = 0;
				if( (SSL_accepted = SSL_accept( clientSSL )) < 0 ) 
				{
					Exception::raise( "SSL_accept() failed (%s) [%d]",
						ERR_error_string( ERR_get_error(), NULL ), SSL_accepted ); 
				}
				if( SSL_accepted == 0 )
				{
					Exception::raise( "SSL_accepted == 0" );
					return;
				}
			}
# if TRACE
			Log::log( "Service::_main: CALLING getSession( %d, <%p> )", clientSocket, clientSSL );
# endif // TRACE

			Session *session = context->service->getSession( clientSocket, clientSSL );

			if( !session )
				Exception::raise( "getSession() failed" );

			session->run();
			session->detach();
		}
		catch( const char *error )
		{
			Log::log( "Service::_main: %s", error ); 
		}
	}
}

ssize_t
Service :: peek( int clientSocket, SSL *clientSSL, void *buf, size_t len )
{
	if( clientSSL ) 
		return( SSL_peek( clientSSL, buf, (int) len ) );
	return( recv( clientSocket, buf, len, MSG_PEEK ) );
}

