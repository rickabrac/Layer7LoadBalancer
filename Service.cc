//
//  Service.cc
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# include "Service.h"
# include "Exception.h"
# include "Connection.h"
# include "Event.h"
# include "Log.h"
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <poll.h>
# include <fcntl.h>
# include <unistd.h>

# include <signal.h>

// # define TRACE    1

ServiceContext :: ServiceContext( const char *listenStr, const char *certPath, const char *keyPath, const char *trustPath )
{
# if TRACE
	Log::console( "ServiceContext::ServiceContext()" );
# endif // TRACE
	this->sockAddr = new SocketAddress( listenStr );
	this->certPath = certPath;
	this->keyPath = keyPath;
	this->trustPath = trustPath;
	this->listenStr = listenStr;
}

ServiceContext :: ~ServiceContext()
{
# if TRACE
	Log::console( "ServiceContext::~ServiceContext()" );
# endif // TRACE
	if( sockAddr )
		delete( sockAddr );
}

SSL_CTX *ServiceContext :: get_SSL_CTX( void )
{
	SSL_CTX *ssl_ctx = SSL_CTX_new( SSLv23_server_method() );

	if( !ssl_ctx ) 
		Exception::raise( "Service::get_SSL_CTX: SSL_CTX_new() failed: %s",
			ERR_error_string( ERR_get_error(), NULL ) );

	SSL_CTX_set_options
	(
		ssl_ctx,
		SSL_OP_NO_SSLv2 |
		SSL_OP_NO_SSLv3 |
		SSL_OP_NO_TLSv1 |
		SSL_OP_NO_TLSv1_1 |
		SSL_OP_NO_COMPRESSION
	);

	if( SSL_CTX_use_certificate_file( ssl_ctx, certPath, SSL_FILETYPE_PEM ) != 1 )
	{
		Exception::raise( "SSL_CTX_use_certificate_file() failed: %s",
			ERR_error_string( ERR_get_error(), NULL ) );
	}

	if( SSL_CTX_use_PrivateKey_file( ssl_ctx, keyPath, SSL_FILETYPE_PEM ) != 1 )
	{
		Exception::raise( "SSL_CTX_use_PrivateKey_file() failed: %s",
			ERR_error_string( ERR_get_error(), NULL ) );
	}

	if( trustPath && SSL_CTX_load_verify_locations( ssl_ctx, NULL, trustPath) <= 0 )
	{
		Exception::raise( "SSL_CTX_load_verify_locations() failed: %s",
			ERR_error_string( ERR_get_error(), NULL ) );
	}

	return( ssl_ctx );
}

size_t Service :: bufLen = 8192;
mutex Service :: bufLenMutex;
SSL_CTX * Service :: ssl_ctx = nullptr;
mutex Service :: ssl_ctx_mutex;
set< SessionContext * > Service :: sslSessions;

Service :: Service( ServiceContext *context ) : Thread( context )
{
# if TRACE
	Log::console( "Service::Service()" );
# endif // TRACE
	this->context = context;
	context->service = this;

	try
	{
		if( context->service->isSecure() )
		{
			Service::ssl_ctx_mutex.lock();
			if( !Service::ssl_ctx )
			{
				OpenSSL_add_all_algorithms();
				SSL_load_error_strings();	
				if( SSL_library_init() < 0 )
					Exception::raise( "SSL_library_init() failed: %s", ERR_error_string( ERR_get_error(), NULL ) );
				Service::ssl_ctx = context->get_SSL_CTX();
			}
			Service::ssl_ctx_mutex.unlock();
		}

		context->socket = socket( AF_INET, SOCK_STREAM, 0 );

		if( context->socket == -1 )
			Exception::raise( "socket() failed: %s", strerror( errno ) );

//		if( fcntl( context->socket, F_SETFD, 1 ) == -1 )
//			Exception::raise( "fcntl( F_SETFD, 1 ) failed", strerror( errno ) );

		signal(SIGPIPE, SIG_IGN);

		// set SO_REUSEADDR so bind() doesn't fail after restart
		int optval = 1;
		if( setsockopt( context->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) ) < 0 )
			Exception::raise( "setsockopt( SO_REUSEADDR ) on listen socket failed (%s)", strerror( errno ) );

		if( ::bind( context->socket, (struct sockaddr *) &context->sockAddr->sockaddr_in, sizeof( struct sockaddr_in ) ) != 0 )
			Exception::raise( "bind() failed (%s) running as superuser?", strerror( errno ) );

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
	Log::console( "Service::~Service()" );
# endif // TRACE
	if( context->socket > -1 )
		(void) close( context->socket );
}

bool
Service :: isSecure()
{
	return( context->certPath && *context->certPath && context->keyPath && *context->keyPath );
}

void
Service :: sessionNotifyProtocolAttribute( string *value, void *data )
{
	if( value || data ) return;	// suppress compiler warning
	return;
}

void
Service :: endSession( SessionContext *context )
{
	if( !context->service->isSecure() )
	 	return;

	Service::ssl_ctx_mutex.lock();

	Service::sslSessions.erase( context );

	if( Service::ssl_ctx && Service::sslSessions.size() == 0 )
	{
		SSL_CTX_free( Service::ssl_ctx );
		Service::ssl_ctx = context->service->context->get_SSL_CTX();
	}
	Service::ssl_ctx_mutex.unlock();
}

ssize_t
Service :: peek( int socket, SSL *ssl, void *buf, size_t len )
{
	fd_set fdset;
	fd_set empty_fdset;
	FD_ZERO( &fdset );
	FD_SET( socket, &fdset );
	FD_ZERO( &empty_fdset );
	struct timeval timeout;
	bzero( &timeout, sizeof( timeout ) );
	timeout.tv_sec = 0; 
	timeout.tv_usec = 100000;
	if( select( FD_SETSIZE, &fdset, &empty_fdset, &empty_fdset, &timeout ) < 0 )
		return( 0 ); 
	if( ssl ) 
		return( SSL_peek( ssl, buf, (int) len ) );
	return( recv( socket, buf, len, MSG_PEEK ) );
}

void Service :: _main( ServiceContext *context )
{
# if TRACE
	Log::console( "Service::_main()" );
# endif // TRACE

	for( ;; )
	{
		try
		{
# if TRACE
			Log::console( "Service::_main: accept()..." );
# endif // TRACE
			int clientSocket;
			socklen_t socklen = sizeof( struct sockaddr_in );

			if( (clientSocket = accept( context->socket, (struct sockaddr *) &context->sockAddr->sockaddr_in, &socklen )) < 0 )
			{
				Exception::raise( "accept() failed (%s) [%d]", strerror( errno ), errno );
			}
# if TRACE
			Log::console( "Service::_main: accept() (clientSocket=%d)", clientSocket );
# endif // TRACE

			struct pollfd server_poll;
			server_poll.fd = context->socket;
			server_poll.events = POLLIN;
			SSL *clientSSL = nullptr;

			if( context->service->isSecure() )
			{
				Service::ssl_ctx_mutex.lock();

				if( !Service::ssl_ctx )
					Service::ssl_ctx = context->get_SSL_CTX();

				if( (clientSSL = SSL_new( Service::ssl_ctx )) == NULL )
					Exception::raise( "SSL_new() failed (%s)", ERR_error_string( ERR_get_error(), NULL ) );
# if TRACE
				Log::console( "Service::_main: clientSocket=%d clientSSL=<%p>", clientSocket, clientSSL );
# endif // TRACE
				Service::ssl_ctx_mutex.unlock();

				if( !SSL_set_fd( clientSSL, clientSocket ) )
					Exception::raise( "SSL_set_fd() failed (%s)", ERR_error_string( ERR_get_error(), NULL ) ); 

				int result = 0;
				if( (result = SSL_accept( clientSSL )) < 0 )
				{
					SSL_shutdown( clientSSL );
					SSL_free( clientSSL );
					(void) close( clientSocket );

					int optval = 1;
					if( setsockopt( context->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) ) < 0 )
						Exception::raise( "setsockopt( SO_REUSEADDR ) on listen socket failed (%s)", strerror( errno ) );
				}

				if( result == 0 )
					Exception::raise( "SSL_accept() == 0" );
			}
# if TRACE
			Log::console( "Service::_main: CALLING getSession( %d, <%p> )", clientSocket, clientSSL );
# endif // TRACE

			Session *session = context->service->getSession( clientSocket, clientSSL );

			if( !session )
			{
				if( context->service->isSecure() )
					SSL_shutdown( clientSSL );
				(void) close( clientSocket );
				continue;
			}

			if( context->service->isSecure() )
			{
				Service::ssl_ctx_mutex.lock();
				Service::sslSessions.insert( session->context );
				Service::ssl_ctx_mutex.unlock();
			}

			session->run();
			session->detach();
		}
		catch( const char *error )
		{
			Log::log( "Service::_main: %s", error ); 
		}
	}
}

