//
//  Connection.cc
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# include "Connection.h"
# include "Thread.h"
# include "Exception.h"
# include "Log.h"
# include <fcntl.h>

# include <signal.h>

// # define TRACE    1

using namespace std;

# define SSL_error() ERR_error_string( ERR_get_error(), NULL )

SSL_CTX * Connection :: ssl_ctx = nullptr;
mutex Connection :: mutex;

Connection :: Connection ( const char *destStr, bool useTLS )
{
	sockAddr = new SocketAddress( destStr );
	this->useTLS = useTLS;

	Connection::mutex.lock();

	if( useTLS && !Connection::ssl_ctx )
	{
  		OpenSSL_add_all_algorithms();
  		ERR_load_crypto_strings();
  		SSL_load_error_strings();

  		if( SSL_library_init() < 0 )
		{
			Connection::mutex.unlock();
			Exception::raise( "Connection::Connection( \"%s\" ) SSL_library_init() failed (%s)", destStr, SSL_error() );
		}

		if( (Connection::ssl_ctx = SSL_CTX_new( SSLv23_client_method() )) == NULL )
		{
			Connection::mutex.unlock();
			Exception::raise( "Connection::Connection( \"%s\" ) SSL_CTX_new() failed (%s)", destStr, SSL_error() );
		}

		SSL_CTX_set_options( Connection::ssl_ctx,
			SSL_OP_NO_SSLv2 |
			SSL_OP_NO_SSLv3 |
			SSL_OP_NO_TLSv1 |
			SSL_OP_NO_TLSv1_1 |
			SSL_OP_NO_COMPRESSION
		);
	}

	Connection::mutex.unlock();
	
	for( ;; )
	{
		socket = ::socket( AF_INET, SOCK_STREAM, 0 );
		if( socket == -1 )
			Exception::raise( "Connection::Connection( \"%s\" ) socket() failed (%s)", destStr, strerror( errno ) );

# ifdef FREEBSD
		int set = 1;
		setsockopt( socket, SOL_SOCKET, SO_NOSIGPIPE, (void *) &set, sizeof( int ) );
# else // FREEBSD
		signal(SIGPIPE, SIG_IGN);
# endif // FREEBSD

		// connect operation must be asynchronous to handle failures
		int flags;
		if( (flags = fcntl( socket, F_GETFL, NULL) ) < 0 )
		{ 
			(void) close( socket );
			Exception::raise( "Connection::Connection( \"%s\" ) fcntl( F_GETFL ) failed (%s)\n", destStr, strerror( errno ) );
		}
		flags |= O_NONBLOCK; 
		if( fcntl( socket, F_SETFL, flags ) < 0 )
		{ 
			(void) close( socket );
			Exception::raise( "Connection::Connection( \"%s\" ) fcntl( F_SETFL ) failed (sync -> async) (%s)\n",
				this, strerror( errno ) );
		} 

		int result;
		if( (result = connect( socket, (struct sockaddr *) &sockAddr->sockaddr_in, sizeof( struct sockaddr_in ) )) == -1 )
		{
# ifdef TRACE
			Log::console( "Connection::Connection( \"%s\" ) connect() failed (%s)", destStr, strerror( errno ) );
# endif // TRACE
			if( errno == EINPROGRESS )
			{
				fd_set fdset, empty_fdset;
				FD_ZERO( &fdset );
				FD_ZERO( &empty_fdset );
				FD_SET( socket, &fdset );
				struct timeval timeout;
				timeout.tv_sec = 0; 
				timeout.tv_usec = 100000; 

				result = select( socket, &empty_fdset, &fdset, &empty_fdset, &timeout ); 

				if( result < 0 && errno != EINTR )
				{
					(void) close( socket );
					Exception::raise( "Connection::Connection( \"%s\" ) select() failed (%s)", destStr, strerror( errno ) );
				} 
				else if( result > 0 )
				{

					// socket selected for write 
					socklen_t optlen = sizeof( int );
					int optval = 0;
					if( getsockopt( socket, SOL_SOCKET, SO_ERROR, (void *)(&optval), &optlen ) < 0 )
					{ 
						(void) close( socket );
						Exception::raise( "Connection::Connection( \"%s\" ) getsockopt() failed (%s)", destStr, strerror( errno ) );
					} 
					// check the value returned... 
					if( optval )
					{ 
# ifdef TRACE
						Log::console( "Connection::Connection( \"%s\" ) getsockopt() failed (%s) [%d]",
							destStr, strerror( optval ), optval );
# endif // TRACE
						if( optval == 61 )
						{
							(void) close( socket );
							Exception::raise( "Connection::Connection( \"%s\" ) server down?", destStr, strerror( errno ) );
						}
						else
							continue;
					}
					else
					{ 
# ifdef TRACE
						Log::console( "Connection::Connection( \"%s\" ) select() timed out", destStr );
# endif // TRACE
						(void) close( socket );
						continue;
					} 
				}
			}
			else
				Exception::raise( "Connection::Connection( \"%s\" ) connect() failed (%s)", destStr, strerror( errno ) );
		}
	 	break;
	}

	// reset socket to synchonous 

	int flags;
	if( (flags = fcntl( socket, F_GETFL, NULL) ) < 0 )
	{ 
		(void) close( socket );
		Exception::raise( "Connection::Connection( \"%s\" ) fcntl( F_GETFL ) failed (%s)\n", destStr, strerror( errno ) );
	}

	flags &= ~O_NONBLOCK; 

	if( fcntl( socket, F_SETFL, flags ) < 0 )
	{ 
		(void) close( socket );
		Exception::raise( "Connection::Connection( \"%s\" ) fcntl( F_SETFL ) failed (async -> sync) (%s)\n",
			destStr, strerror( errno ) );
	} 

	if( useTLS )
	{
		if( !(ssl = SSL_new( Connection::ssl_ctx )) )
		{
			(void) close( socket );
			Exception::raise( "Connection::Connection( %s ) SSL_new() failed (%s)", destStr, SSL_error() );
		}

		if( !SSL_set_fd( ssl, socket ) )
		{
			(void) close( socket );
			Exception::raise( "Connection::Connection( %s ) SSL_set_fd() failed (%s)", destStr, SSL_error() );
		}

		int SSL_connected;
		if( (SSL_connected = SSL_connect( ssl )) <= 0 )
		{
			(void) close( socket );
			Exception::raise( "Connection::Connection( %s ) SSL_connect() failed (%s) [%d]",
				destStr, SSL_connected == -1 ? "out of resource?" : SSL_error(), SSL_connected );
		}
	}

# if TRACE
	Log::console( "Connection::Connection( %s ) ssl=<%p>", destStr, ssl );
# endif // TRACE
}

ssize_t
Connection :: pending( void )
{
	if( useTLS )
		return SSL_pending( ssl );
	char c;
	return peek( &c, 1 );
}

ssize_t
Connection :: peek( void *buf, size_t len )
{
	if( useTLS )
		return( SSL_peek( ssl, buf, (int) len ) );
	return( recv( socket, buf, len, MSG_PEEK ) );
} 

ssize_t
Connection :: read( void *buf, size_t len ) {
	if( useTLS )
		return( SSL_read( ssl, buf, (int) len ) );
	return( recv( socket, buf, len, 0 ) );
} 

ssize_t
Connection :: write( void *data, size_t len )
{
	if( useTLS )
		return( SSL_write( ssl, data, (int) len ) );
	return( send( socket, data, len, 0 ) );
} 

Connection :: ~Connection ()
{
# if TRACE
	Log::console( "Connection::~Connection" );
# endif // TRACE
	if( ssl )
	{
		SSL_shutdown( ssl );
		SSL_free( ssl );
	}
	if( sockAddr )
		delete( sockAddr );
	if( socket > -1 )
	{
		(void) close( socket );
	}
}

