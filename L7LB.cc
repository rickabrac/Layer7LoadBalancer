//				
//  L7LB.cc
//  Layer7LoadBalancer
//  Created by Rick Tyler
//

# define DEBUG 1

# define TRACE	1

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
# include "L7LBConfig.cc"

using namespace std;

class L7LBServiceContext: public ServiceContext
{
	public:

		L7LBServiceContext( ServiceConfig *serviceConfig )
			: ServiceContext( serviceConfig->listenStr.c_str(), serviceConfig->keyPath.c_str(), serviceConfig->certPath.c_str() )
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
			char buf[ 1024 ];
			bzero( buf, sizeof( buf ) );
			int peeked = context->service->peek( clientSocket, clientSSL, buf, sizeof( buf ) );
			buf[ peeked ] = '\0';
# if TRACE
			Log::log( "PEEK=[\n%s]", buf );
# endif // TRACE
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

# if DEBUG
L7LBConfig *L7LBConfig :: config = new L7LBConfig( "test.conf" );
# else // DEBUG
L7LBConfig *L7LBConfig :: config = new L7LBConfig( "l7lb.conf" );
# endif // DEBUG

int main( void )
{
	L7LBConfig::config->parseFile();
	Event done;
	for( auto it = L7LBConfig::config->serviceConfigs.begin(); it != L7LBConfig::config->serviceConfigs.end(); it++ )
	{
		ServiceConfig *serviceConfig = *it;
# if TRACE
		cout << "SERVICE: " << serviceConfig->listenStr << endl;
		cout << "KEY: " << serviceConfig->keyPath	<< endl;
		cout << "CERTIFICATE: " << serviceConfig->certPath	<< endl;
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

