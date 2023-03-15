//
//  Connection.cc
//	L7LB (Layer7LoadBalancer)
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

# include "Connection.h"
# include "Thread.h"
# include "Exception.h"
# include "Log.h"
# include <fcntl.h>

// # define TRACE    1

using namespace std;

# define SSL_error() ERR_error_string( ERR_get_error(), NULL )

SSL_CTX * Connection :: ssl_ctx = nullptr;
map< string, Connection * > Connection :: connections;
mutex Connection :: connectionMutex;

const char *
Connection :: error ( int errNum )
{
	if( errNum == 0 )
		return( "out of resource?" );
	return( SSL_error() );
}

void
Connection :: debug ( void )
{
	Connection::connectionMutex.lock();
	Log::log( "Connection::debug() connections.size()=%d", Connection::connections.size() );
	for( auto it = Connection::connections.begin(); it != Connection::connections.end(); it++ )
	{
		string tid = (string) it->first;
		Connection *sslc = (Connection *) it->second;
		Log::log( "Connection::debug() connections[ %s ]=<%p>", tid.c_str(), sslc ); 
	}
	Connection::connectionMutex.unlock();
}

Connection :: Connection ( const char *destStr, bool secure )
{
	this->secure = secure;
	
	Connection::connectionMutex.lock();

	string tid = Thread::tid();
	if( connections.find( tid ) != connections.end() )
	{
		Connection::connectionMutex.unlock();
		Exception::raise( "Connection::Connection( \"%s\" ) thread %s already has a Connection",
			destStr, tid.c_str() );
	}

	if( secure && !Connection::ssl_ctx )
	{
  		OpenSSL_add_all_algorithms();
  		ERR_load_crypto_strings();
  		SSL_load_error_strings();

  		if( SSL_library_init() < 0 )
		{
			Connection::connectionMutex.unlock();
			Exception::raise( "Connection::Connection( \"%s\" ) SSL_library_init() failed (%s)",
				destStr, SSL_error() );
		}

		if( (Connection::ssl_ctx = SSL_CTX_new( SSLv23_client_method() )) == NULL )
		{
			Connection::connectionMutex.unlock();
			Exception::raise( "Connection::Connection( \"%s\" ) SSL_CTX_new() failed (%s)",
				destStr, SSL_error() );
		}

		SSL_CTX_set_options( Connection::ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1
			| SSL_OP_NO_TLSv1_1 | SSL_OP_NO_COMPRESSION );
	}
	
	sockAddr = new SocketAddress( destStr );

	for( ;; )
	{
		socket = ::socket( AF_INET, SOCK_STREAM, 0 );
		if( socket == -1 )
		{
			Connection::connectionMutex.unlock();
			Exception::raise( "Connection::Connection( \"%s\" ) socket() failed (%s)",
				destStr, strerror( errno ) );
		}

		// connect operation must be asynchronous to handle failures

		int flags;
		if( (flags = fcntl( socket, F_GETFL, NULL) ) < 0 )
		{ 
			Exception::raise( "Connection::Connection( \"%s\" ) fcntl( F_GETFL ) failed (%s)\n",
				destStr, strerror( errno ) );
		}
		flags |= O_NONBLOCK; 
		if( fcntl( socket, F_SETFL, flags ) < 0 )
		{ 
			Exception::raise( "Connection::Connection( \"%s\" ) fcntl( F_SETFL ) failed (sync -> async) (%s)\n",
				this, strerror( errno ) );
		} 

		int result;
		if( (result = connect( socket, (struct sockaddr *) &sockAddr->sockaddr_in, sizeof( struct sockaddr_in ) )) == -1 )
		{
# ifdef TRACE
			Log::log( "Connection::Connection( \"%s\" ) connect() failed (%s)", destStr, strerror( errno ) );
# endif // TRACE

			if( errno == EINPROGRESS )
			{
				fd_set fdset, empty_fdset;
				FD_ZERO( &fdset );
				FD_ZERO( &empty_fdset );
				FD_SET( socket, &fdset );
				struct timeval timeout;
				bzero( &timeout, sizeof( timeout ) );
				timeout.tv_sec = 0; 
				timeout.tv_usec = 100000; 

				result = select( socket, &empty_fdset, &fdset, &empty_fdset, &timeout ); 

				if( result < 0 && errno != EINTR )
				{
					Exception::raise( "Connection::Connection( \"%s\" ) select() failed (%s)", destStr, strerror( errno ) );
				} 
				else if( result > 0 )
				{
					// socket selected for write 
					socklen_t optlen = sizeof( int );
					int optval = 0;
					if( getsockopt( socket, SOL_SOCKET, SO_ERROR, (void *)(&optval), &optlen ) < 0 )
					{ 
# ifdef TRACE
						Exception::raise( "Connection::Connection( \"%s\" ) getsockopt() failed (%s)", destStr, strerror( errno ) );
# endif // TRACE
					} 
					// check the value returned... 
					if( optval )
					{ 
# ifdef TRACE
						Log::log( "Connection::Connection( \"%s\" ) getsockopt() failed (%s) [%d]", destStr, strerror( optval ), optval );
# endif // TRACE
						if( optval == 61 )
						{
							Exception::raise( "Connection::Connection( \"%s\" ) server down?", destStr, strerror( errno ) );
						}
						else
							continue;
					}
					else
					{ 
# ifdef TRACE
						Log::log( "Connection::Connection( \"%s\" ) select() timed out", destStr );
# endif // TRACE
						close( socket );
						continue;
					} 
				}
			}
			else
			{ 
# ifdef TRACE
				Exception::raise( "Connection::Connection( \"%s\" ) connect() failed (%s)", destStr, strerror( errno ) );
# endif // TRACE
			} 
		}

	 	break;
	}

	// reset socket to synchonous 

	int flags;
	if( (flags = fcntl( socket, F_GETFL, NULL) ) < 0 )
	{ 
		Exception::raise( "Connection::Connection( \"%s\" ) fcntl( F_GETFL ) failed (%s)\n",
			destStr, strerror( errno ) );
	}
	flags &= ~O_NONBLOCK; 
	if( fcntl( socket, F_SETFL, flags ) < 0 )
	{ 
		Exception::raise( "Connection::Connection( \"%s\" ) fcntl( F_SETFL ) failed (async -> sync) (%s)\n", destStr, strerror( errno ) );
	} 

	if( secure )
	{
		if( !(ssl = SSL_new( Connection::ssl_ctx )) )
		{
			(void) close( socket );
			Connection::connectionMutex.unlock();
			Exception::raise( "Connection::Connection( %s ) SSL_new() failed (%s)", destStr, SSL_error() );
		}

		if( !SSL_set_fd( ssl, socket ) )
		{
			(void) close( socket );
			Connection::connectionMutex.unlock();
			Exception::raise( "Connection::Connection( %s ) SSL_set_fd() failed (%s)", destStr, SSL_error() );
		}

		int SSL_connected;
		if( (SSL_connected = SSL_connect( ssl )) <= 0 ) 
		{
			(void) close( socket );
			Connection::connectionMutex.unlock();
			Exception::raise( "Connection::Connection( %s ) SSL_connect() failed (%s) [%d]",
				destStr, SSL_connected == -1 ? "out of resource?" : SSL_error(), SSL_connected );
		}
	}

	Connection::connections[ Thread::tid() ] = this;

	Connection::connectionMutex.unlock();

# if TRACE
	Log::log( "Connection::Connection( %s ) ssl=<%p>", destStr, ssl );
# endif // TRACE
}

ssize_t
Connection :: pending( void )
{
	if( secure )
		return SSL_pending( ssl );
	char c;
	return peek( &c, 1 );
}

ssize_t
Connection :: peek( void *buf, size_t len )
{
	if( secure )
		return( SSL_peek( ssl, buf, (int) len ) );
	return( recv( socket, buf, len, MSG_PEEK ) );
} 

ssize_t
Connection :: read( void *buf, size_t len )
{
	if( secure )
		return( SSL_read( ssl, buf, (int) len ) );
	return( recv( socket, buf, len, 0 ) );
} 

ssize_t
Connection :: write( void *data, size_t len )
{
	if( secure )
	{
		return( SSL_write( ssl, data, (int) len ) );
	}
	return( send( socket, data, len, 0 ) );
} 

Connection :: ~Connection ()
{
# if TRACE
	Log::log( "Connection::~Connection" );
# endif // TRACE
	if( ssl )
	{
# if TRACE
		Log::log( "Connection::~Connection() SSL_free( %p )", ssl );
# endif // TRACE
		SSL_free( ssl );
	}
	if( sockAddr )
		delete( sockAddr );
	if( socket > 0 )
		(void) close( socket );
	Connection::connectionMutex.lock();
	string tid = Thread::tid();
	if( Connection::connections.erase( tid ) == 0 )
	{
		Connection::connectionMutex.unlock();
		Exception::raise( "Connection::~Connection()  Connection::connections.erase( %s ) failed",
			tid.c_str() ); 
	}
	if( Connection::connections.size() == 0 )
	{
		SSL_CTX_free( Connection::ssl_ctx );
		Connection::ssl_ctx = NULL;
	}
# if TRACE
	Log::log( "Connection::~Connection() connections.size()=%d", Connection::connections.size() );
# endif // TRACE
	Connection::connectionMutex.unlock();
}

