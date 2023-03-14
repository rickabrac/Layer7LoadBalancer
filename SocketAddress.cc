//
//  SocketAddress.cc
//  L7LB (Layer7LoadBalancer)
//  Created by Rick Tyler
//

# include "SocketAddress.h"
# include "Exception.h"
# include "Log.h"
# include <netdb.h>

// # define TRACE    1

map< string, struct in_addr * > SocketAddress :: hosts;
mutex SocketAddress :: hostMutex;

SocketAddress :: SocketAddress ( const char *addrStr ) 
{
	bzero( &sockaddr_in, sizeof( struct sockaddr_in ) );
	sockaddr_in.sin_family = AF_INET;	// pipes not supported currently
	struct in_addr in_addr;
	bzero( &in_addr, sizeof( struct in_addr ) );
	if( addrStr && *addrStr )
	{
		// parse addrStr and set up sockaddr 
		char *addrBuf = (char *) malloc( strlen( addrStr ) + 1 );
		bzero( addrBuf, strlen( addrStr ) + 1 );
		memcpy( addrBuf, addrStr, strlen( addrStr ) );
		char *c;
		for( c = (char *) addrBuf; *c && *c != ':'; c++ );
		char *portStr = NULL;
		if( *c == ':' )
		{
			*c++ = '\0';
			portStr = c;
			char *hostStr = addrBuf;
			if( isdigit( *hostStr ) )
			{
				// assume numeric ip
				in_addr.s_addr = inet_addr( addrBuf );
			}
			else
			{
				// assume hostname
				hostname = hostStr;
				SocketAddress::hostMutex.lock();
				struct in_addr *_in_addr = hosts[ hostStr ]; 
				if( !_in_addr ) 
				{
					struct hostent *host = gethostbyname( hostStr );
					if( host == NULL )
					{
						SocketAddress::hostMutex.unlock();
						Exception::raise( "SocketAddress::SocketAddress( '%s' ) gethostbyname() failed (%s)",
							addrStr, strerror( errno ) );
					}
					if( host->h_addr_list[ 0 ] == NULL )
					{
						SocketAddress::hostMutex.unlock();
						Exception::raise( "SocketAddress::SocketAddress( '%s' ) host->h_addr_list[ 0 ] == NULL",
							addrStr );
					}
					in_addr = *((struct in_addr *) host->h_addr_list[ 0 ]);
					_in_addr = (struct in_addr *) malloc( sizeof( struct in_addr ) );
					*_in_addr = in_addr;
# if TRACE
					Log::log( "NEW IN_ADDR( %s ) <%p>", hostStr, _in_addr );
# endif // TRACE
					SocketAddress::hosts[ hostStr ] = _in_addr;
				}
				else
					in_addr = *_in_addr;
				SocketAddress::hostMutex.unlock();
			}
		}
		else if( *addrBuf != '/' )
		{
			// address string must contain only port number 
			if( !*c || !isdigit( *addrBuf ) )
			{
				Exception::raise( "SocketAddress::SocketAddress( '%s' ) port number not specified",
					addrStr ); 
			}
			in_addr.s_addr = htonl( INADDR_ANY ); 
			portStr = addrBuf;
		}
		else
			Exception::raise( "SocketAddress::SocketAddress( '%s' ) pipes not supported", addrStr );

		// portStr should now point to port number
		sockaddr_in.sin_addr = in_addr;
		sockaddr_in.sin_port = htons( atoi( portStr ) );
		if( sockaddr_in.sin_port == 0 )
		{
			Exception::raise( "SocketAddress::SocketAddress( '%s' ) gethostbyname() failed (%s)",
				addrStr, strerror( errno ) );
		}
		free( addrBuf );
	}
# if TRACE
	this->debug();
# endif // TRACE
}

void
SocketAddress :: debug( void )
{
	Log::log( "this=<%p>", this );
	Log::log( "sockaddr_in=<%p>", &this->sockaddr_in );
	Log::log( "sockaddr_in.sin_family=%d", this->sockaddr_in.sin_family );
	Log::log( "sockaddr_in.sin_port=%d", ntohs( this->sockaddr_in.sin_port ) );
	Log::log( "sockaddr_in.sin_addr=[%s]", inet_ntoa( this->sockaddr_in.sin_addr ) );
}

