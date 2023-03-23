//				
//  L7LB.cc
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  Copyright 2021 Rick Tyler
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

// * BUG * Connecting to a TLS Service breaks if non-TLS connection attempted

// # define TRACE    1

# include "Service.h"
# include "Session.h"
# include "ProxySession.h"
# include "Exception.h"
# include "Event.h"
# include "Log.h"
# include <time.h>
# include <vector>
# include <iostream>
# include <fstream>
# include <string>
# include <map>
# include <sys/time.h>

# define _L7LBConfig_h_    1
# include "L7LBConfig.h"

using namespace std;

L7LBConfig *L7LBConfig :: config = nullptr;

class L7LBServiceContext: public ServiceContext
{
	public:

		L7LBServiceContext( ServiceConfig *serviceConfig )
			: ServiceContext( serviceConfig->listenStr.c_str(), serviceConfig->keyPath.c_str(), serviceConfig->certPath.c_str() )
		{
			this->sessionConfigs = serviceConfig->sessionConfigs;
			this->sessionCookie = serviceConfig->sessionCookie;
		}

	private:

		vector< SessionConfig * > *sessionConfigs;
		string sessionCookie;

	friend class L7LBService;
};

class L7LBService : public Service
{
	public:

		L7LBService( L7LBServiceContext *context ) : Service( context )
		{
			this->context = context;
		}

		ThreadMain main( void ) { return( (ThreadMain) _main ); }

	private:

		L7LBServiceContext *context;

		map< string, string > stickySessionDestStr;
		map< string, int > stickySessionLastUsed;

		Session *getSession( int clientSocket, SSL *clientSSL )
		{
			const char *httpHeaderStart = "HTTP/1.1 200 OK\r\n";
			const char *httpCookieDelimiter = ":";
			const char *httpCookieEnd = "\r\n";
			const char *httpHeaderEnd = "\r\n";

			if( !context->sessionCookie.empty() )
			{
# if TRACE
				Log::console( "sessionCookie=%s", context->sessionCookie.c_str() );
# endif // TRACE
 				char buf[ 1024 ];
 				bzero( buf, sizeof( buf ) );
 				int peeked = context->service->peek( clientSocket, clientSSL, buf, sizeof( buf ) );
				if( peeked == sizeof( buf ) )
					Exception::raise( "L7LBService::getSession: HTTP header length exceeds static buffer length (%d)", sizeof( buf ) );
				if( peeked == 0 ) 
					return( nullptr );

				char lineBuf[ 1024 ];
				char *line = buf + 17;
				char *newline;
				map< string, string * > httpCookies;

				while( (newline = strstr( line, httpCookieEnd )) )
				{
					if( strncmp( line, httpHeaderEnd, strlen( httpHeaderEnd ) ) == 0 )
						break;
					bzero( lineBuf, sizeof( lineBuf ) );
					memcpy( lineBuf, line, newline - line );
					char *c1;
					for( c1 = lineBuf; *c1 && isspace( *c1 ); c1++ );
					char *c2;
					for( c2 = c1; strncmp( c2, httpCookieDelimiter, strlen( httpCookieDelimiter ) ) != 0 && strstr( c2, httpCookieEnd ) != c2; c2++ );
					if( strncmp( c2, httpCookieDelimiter, strlen( httpCookieDelimiter ) ) == 0 )
					{
						*c2++ = '\0';
						string name(c1);
						while( isspace( *c2 ) )
							++c2;
						string value( c2 );
						httpCookies[ name ] = &value;
					}
						line = newline + 2;
				}
# if TRACE
				// Log::console( "httpCookies.size()=%d", httpCookies.size() );
				// for( auto const& [name, value] : httpCookies )
				// 	Log::console( "%s: %s", name.c_str(), value.c_str() );
# endif // TRACE
# if TRACE
				string *sessionCookieValue = httpCookies[ context->sessionCookie ];
				if( sessionCookieValue )
					Log::console( "%s NOT FOUND", context->sessionCookie.c_str() );
# endif // TRACE
			}

			L7LBServiceContext *myServiceContext = (L7LBServiceContext *) context;
			vector<SessionConfig *> sessionConfigs = *myServiceContext->sessionConfigs;
			int sessionIndex = 0;
			// ### TO DO: RESPECT COOKIE-SESSION PERSISTENCE OR DO ROUND-ROBIN ###
			SessionConfig sessionConfig = *sessionConfigs[ sessionIndex ];
			const char *destStr = sessionConfig.destStr;
			bool useTLS = sessionConfig.useTLS;
			ProxySessionContext *context = new ProxySessionContext(
				this,
				clientSocket,
				clientSSL,
				destStr,
				useTLS,
				this->context->sessionCookie,
				httpHeaderStart,
				httpCookieDelimiter,
				httpCookieEnd,
				httpHeaderEnd
			);
			return( new ProxySession( context ) );
		}

#		define SESSION_COOKIE_TIMEOUT    1800

		// ### TO-DO: PERIODICALLY REMOVE TIMED-OUT STICKY SESSION MAP ELEMENTS ###

		void sessionNotifyProtocolAttribute( string *value, void *data )
		{
			const char *destStr = (const char *) data;
			Log::console( "notifyProxyProtocolAttribute( \"%s\" ) destStr=%s", value->c_str(), destStr );
			struct timeval tv;
			(void) gettimeofday( &tv, NULL );
			int now = tv.tv_sec;
			stickySessionDestStr[ *value ] = string( destStr );
			stickySessionLastUsed[ *value ] = now;
		}
};

int main( int argc, char **argv )
{
	if( argc != 2 )
	{
		cout << "Usage: " << argv[ 0 ] << " config_file_path" << endl;	
		::exit( -1 );
	}

	L7LBConfig::config = new L7LBConfig( argv[ 1 ] );
	try
	{
		L7LBConfig::config->parseFile(); 
	}
	catch( const char *error )
	{
		Log::console( "%s", error );
		::exit( -1 );
	}

	Event done;

	for( auto it = L7LBConfig::config->serviceConfigs.begin(); it != L7LBConfig::config->serviceConfigs.end(); it++ )
	{
		ServiceConfig *serviceConfig = *it;
# if TRACE
		cout << "SERVICE: " << serviceConfig->listenStr << endl;
		cout << "KEY: " << serviceConfig->keyPath	<< endl;
		cout << "CERTIFICATE: " << serviceConfig->certPath << endl;
		cout << "TRUST: " << serviceConfig->trustPath << endl;
# endif // TRACE
		try
		{
			L7LBServiceContext *context = new L7LBServiceContext( serviceConfig );
			L7LBService *service = new L7LBService( context );
			service->run();
			service->detach();
		}
		catch( const char *error )
		{
			Log::console( "%s", error ); 
			exit( -1 );
		}
	}	
	done.wait();
}

