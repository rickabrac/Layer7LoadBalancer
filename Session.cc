//
//  Session.cc
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
	Log::console( "SessionContext::SessionContext()" );
# endif // TRACE
	this->service= service;
	this->clientSocket = clientSocket;
	this->clientSSL = clientSSL;
}

SessionContext :: ~SessionContext()
{
# if TRACE
	Log::console( "SessionContext::~SessionContext()" );
# endif // TRACE
	if( service->isSecure() && clientSSL )
	{
		SSL_shutdown( clientSSL );
		SSL_free( clientSSL );
	}
	if( clientSocket != -1  )
	{
		(void) close( clientSocket );
	}
	if( session )
	{
		session->context->service->notifyEndOfSession( this );
		delete( session );
	}
}

Session :: Session( SessionContext *context ) : Thread( context )
{
# if TRACE
	Log::console( "Session::Session()" );
# endif // TRACE
	context->session = this;
	this->context = context;
}

Session :: ~Session()
{
# if TRACE
	Log::console( "Session::~Session()" );
# endif // TRACE
}

