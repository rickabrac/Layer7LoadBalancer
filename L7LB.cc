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

# define TRACE    1

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

# define _L7LBConfig_h_    1
# include "L7LBConfig.h"

using namespace std;

L7LBConfig *L7LBConfig :: config = nullptr;

class L7LBServiceContext: public ServiceContext
{
	public:

		L7LBServiceContext( ServiceConfig *serviceConfig ) :
			ServiceContext( serviceConfig->listenStr.c_str(), serviceConfig->keyPath.c_str(), serviceConfig->certPath.c_str() )
		{
			this->sessionConfigs = serviceConfig->sessionConfigs;
		}

	private:

		vector<SessionConfig *> *sessionConfigs;

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

		Session *getSession( int clientSocket, SSL *clientSSL )
		{
// 			char buf[ 1024 ];
// 			bzero( buf, sizeof( buf ) );
// 			int peeked = context->service->clientPeek( clientSocket, clientSSL, buf, sizeof( buf ) );
// 			buf[ peeked ] = '\0';
// # if TRACE
// 			Log::log( "PEEK=[\n%s]", buf );
// # endif // TRACE
			L7LBServiceContext *_context = (L7LBServiceContext *) context;
			vector<SessionConfig *> sessionConfigs = *_context->sessionConfigs;
			int sessionIndex = 0;	// use first session configuration if multiple specified for now
			SessionConfig sessionConfig = *sessionConfigs[ sessionIndex ];
			const char *destStr = sessionConfig.destStr;
			bool secure = sessionConfig.secure;
			ProxySessionContext *context = new ProxySessionContext( this, clientSocket, clientSSL, destStr, secure );
			return( new ProxySession( context ) );
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
		Log::log( "%s", error );
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
			Log::log( "%s", error ); 
			exit( -1 );
		}
	}	
	Log::log( "WAITING..." );
	done.wait();
}

