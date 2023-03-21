//
//  Service.cc
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

ServiceContext :: ServiceContext( const char *listenStr, const char *certPath, const char *keyPath, const char *trustPath )
{
# if TRACE
	Log::log( "ServiceContext::ServiceContext()" );
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
	Log::log( "ServiceContext::~ServiceContext()" );
# endif // TRACE
	if( sockAddr )
		delete( sockAddr );
}

SSL_CTX *ServiceContext :: ssl_ctx = NULL;
size_t Service :: bufLen = 4096; // 32768;
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

			if( context->trustPath != nullptr && SSL_CTX_load_verify_locations( ServiceContext::ssl_ctx, NULL, context->trustPath) <= 0 )
			{
				Exception::raise( "SSL_CTX_load_verify_locations() failed: %s",
					ERR_error_string( ERR_get_error(), NULL ) );
			}
		}

		context->socket = socket( AF_INET, SOCK_STREAM, 0 );

		if( context->socket == -1 )
			Exception::raise( "socket() failed: %s", strerror( errno ) );

		if( fcntl( context->socket, F_SETFD, 1 ) == -1 )
			Exception::raise( "fcntl( F_SETFD, 1 ) failed", strerror( errno ) );

		int set = 1;
		setsockopt( context->socket, SOL_SOCKET, SO_NOSIGPIPE, (void *) &set, sizeof( int ) );

		// set SO_REUSEADDR so bind() doesn't fail after restart
		int optval = 1;
		if( setsockopt( context->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) ) < 0 )
		{
			Exception::raise( "setsockopt( SO_REUSEADDR ) on listen socket failed (%s)", strerror( errno ) );
		}

		if( ::bind( context->socket, (struct sockaddr *) &context->sockAddr->sockaddr_in,
			sizeof( struct sockaddr_in ) ) != 0 )
		{
			Exception::raise( "bind() failed (%s) running as superuser?", strerror( errno ) );
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

	if( context->socket > -1 )
		(void) close( context->socket );
}

bool Service :: isSecure()
{
	return( context->certPath && *context->certPath && context->keyPath && *context->keyPath );
}

void
Service :: notifySessionProtocolAttribute( string *value ) 
{
	if( value ) return; // avoid unused variable warning
	return;
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
				Log::log( "Service::_main: clientSocket=%d clientSSL=<%p>", clientSocket, clientSSL );
# endif // TRACE

				if( !SSL_set_fd( clientSSL, clientSocket ) )
				{
					Exception::raise( "SSL_set_fd() failed (%s)",
						ERR_error_string( ERR_get_error(), NULL ) ); 
				}

//				SSL_set_accept_state( clientSSL );

				int result = 0;
				if( (result = SSL_accept( clientSSL )) < 0 ) 
				{
					SSL_shutdown( clientSSL );
					SSL_free( clientSSL );
					(void) close( clientSocket );

					int optval = 1;
					if( setsockopt( context->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) ) < 0 )
					{
						Exception::raise( "setsockopt( SO_REUSEADDR ) on listen socket failed (%s)", strerror( errno ) );
					}
				}

				if( result == 0 )
				{
					Exception::raise( "SSL_accept() == 0" );
					return;
				}
			}
# if TRACE
			Log::log( "Service::_main: CALLING getSession( %d, <%p> )", clientSocket, clientSSL );
# endif // TRACE

			Session *session = context->service->getSession( clientSocket, clientSSL );

			context->sessionMutex.lock();

			if( context->sessions.size() == 0 )
			{
				SSL_CTX_free( ServiceContext::ssl_ctx );

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

				if( context->trustPath != nullptr && SSL_CTX_load_verify_locations( ServiceContext::ssl_ctx, NULL, context->trustPath) <= 0 )
				{
					Exception::raise( "SSL_CTX_load_verify_locations() failed: %s",
						ERR_error_string( ERR_get_error(), NULL ) );
				}
			}

			context->sessions.insert( session);

			context->sessionMutex.unlock();

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

