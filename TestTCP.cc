//
//  TestTCP.cc
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
//
//  TCP-mode test for Service, Connection, Session, and ProxySession.
//

# include "Service.h"
# include "ProxySession.h"
# include "Connection.h"
# include "Exception.h"
# include "Event.h"
# include "Log.h"

// # define TRACE    1

using namespace std;

class ProxyServiceContext : public ServiceContext 
{
	public:
		ProxyServiceContext( const char *listenStr, const char *destStr ) :
			ServiceContext( listenStr )
		{
			this->destStr = destStr;
		}

	private:
		const char *destStr;
};

class ProxyService : public Service 
{
	public:
		ProxyService( ServiceContext *context ) : Service( context ) { }

		ThreadMain main( void ) { return( (ThreadMain) _main ); }

	protected:
		ProxySession *getSession( int clientSocket, SSL *clientSSL = NULL )
		{
			ProxySessionContext *context = new ProxySessionContext( this, clientSocket, clientSSL, "localhost:667", false );
			return( new ProxySession( context ) );
		}
};

class EchoSessionContext : public SessionContext 
{
	public:
		EchoSessionContext( Service *service, int clientSocket, SSL *clientSSL )
			: SessionContext( service, clientSocket, clientSSL ) { }

	private:
		char buf[ 1024 ];
		int len;

	friend class EchoSession;
};

class EchoSession : public Session
{
	public:

		EchoSession( EchoSessionContext *context ) : Session( context ) { }

		ThreadMain main( void ) { return( (ThreadMain) _main ); }

		static void _main ( EchoSessionContext *context )
		{
# if TRACE
			Log::console( "EchoSession::main( %p ) RUN", context );
# endif // TRACE

			context->session->sleep( rand() % 1000 );

			try
			{
				fd_set fdset;
				fd_set empty_fdset;
				struct timeval timeout;
				bzero( (void *) &timeout, (size_t) sizeof( timeout ) );
				timeout.tv_sec = 1;
				timeout.tv_usec = 0; 
				int selected;
				context->len = 0;
				
				for( ;; )
				{
					FD_ZERO( &fdset );
					FD_ZERO( &empty_fdset );
					FD_SET( context->clientSocket, &fdset );
# if TRACE
					Log::console( "EchoSession[ %p ]::_main: loop clientSocket=%d", context, context->clientSocket );
# endif // TRACE
					if( (selected = select( FD_SETSIZE, &fdset, &empty_fdset, &empty_fdset, &timeout)) >= 0 )
					{
						if( FD_ISSET( context->clientSocket, &fdset ) )
						{
							// client has data ready
							bzero( context->buf, sizeof( context->buf ) );

							if( (context->len = (int) recv( context->clientSocket, context->buf, sizeof( context->buf ), 0 )) > 0 )
							{
# if TRACE
								context->buf[ context->len - 1 ] = '\0';
								if( strlen( context->buf ) )
									Log::console( "EchoSession::main( %p ) ECHO \"%s\"", context, context->buf );
# endif // TRACE 
								// send back to client
								ssize_t sent, total = 0;
								while( (sent = send( context->clientSocket, context->buf + total, context->len, 0 )) < context->len )
								{
									if( sent < 0 )
									{
										Log::console( "EchoSession[ %p ]::_main: send() failed [%d] (%s)",
											context, errno, strerror( errno ) );
										return;
									}
									context->len -= sent;
									total += sent;
								}
# if TRACE
								Log::console( "EchoSession::main( %p ) SENT \"%s\"", context, context->buf );
# endif // TRACE
								if( sent == 0 )
								{
									Log::console( "EchoSession[ %p ]._main: send() failed [%d] (%s)",
										context, errno, strerror( errno ) );
								}
							}
							else if( context->len < 0 )
							{
								Log::console( "EchoSession[ %p ]::_main: len <= 0", context);
								return;
							}
						}
					}
					else
					{
						Log::console( "EchoSession[ %p ]::_main: select() < 0", context ); 
						return;
					}
				}
			}
			catch( const char *msg )
			{
				// exception skips to terminate session below
				Log::console( "EchoSession[ %p ]::_main: %s", context, msg );
			}
		}
};

class EchoServiceContext : public ServiceContext
{
	public:

		EchoServiceContext( const char *listenStr ) : ServiceContext( listenStr ) { }

	protected:

		const char *destStr;
};

class EchoService : public Service
{
	public:

		EchoService( EchoServiceContext *context ) : Service( context ) { }

		ThreadMain main( void ) { return( (ThreadMain) _main ); }

	protected:

		EchoSession *getSession( int clientSocket, SSL *clientSSL )
		{
# if TRACE
			Log::console( "EchoService[ %p ]::getSession()" );
# endif // TRACE
			EchoSessionContext *context = new EchoSessionContext( this, clientSocket, clientSSL );
			return( new EchoSession( context ) );
		}
};

class TestSession;

class TestSessionContext : public ThreadContext 
{
	public:

		TestSessionContext( int numEchos )
		{
			this->numEchos = numEchos;
		}

		TestSession *session;
		int numEchos;
};

class TestSession : public Thread 
{
	public:

		TestSession( TestSessionContext *context ) : Thread( context )
		{
			context->session = this;
		}

		virtual ~TestSession()
		{
			Log::console( "TestSession::~TestSession()" );
			delete( context );
		}

		ThreadMain main( void ) { return( (ThreadMain) _main ); }

		static void _main( TestSessionContext *context )
		{
			Connection *connection = NULL;
			try
			{
# if TRACE
				Log::console( "TestSession::main( %p ) RUN", context );
# endif // TRACE
				connection = new Connection( "localhost:666", false );
				int i;
				char sessionIdBuf[ 32 ];
				sprintf( sessionIdBuf, "%p", context->session );
				string sessionId( sessionIdBuf );
				TestSession::sessionMutex.lock();
				TestSession::sessions[ sessionId ] = context->session;
				TestSession::sessionMutex.unlock();
				for( i = 0; i < context->numEchos; i++ )
				{
					context->session->sleep( rand() % 250 );
					char outBuf[ 128 ];
					sprintf( outBuf, "TEST<%p>#%d", context, i );
					connection->write( outBuf, strlen( outBuf ) + 1 );
					char inBuf[ 128 ];
					bzero( inBuf, sizeof( inBuf ) );
					int n;
					size_t total = 0;
					while( (n = connection->read( inBuf + total, sizeof( inBuf ) )) > 0 
						&& inBuf[ total - 1 ] != '\0' )
					{
						total += n;	
					}
					if( total && strcmp( inBuf, outBuf ) != 0 ) 
						Exception::raise( "test failed (input mismatch)" ); 
				}
				delete( connection );
				++TestSession::finished;
				Log::console( "TestSession[ %p ]::_main: %d strings verified [session #%d].",
					context, i, TestSession::finished );
				context->session->stop();
			}
			catch( const char *error )
			{
				if( connection )
					delete( connection );
				Log::console( "TestSession[ %p ]::_main: %s", context, error );
				::exit( -1 );
			}
		}

		void stop( void )
		{
			char sessionIdBuf[ 32 ];
			sprintf( sessionIdBuf, "%p", this );
			string sessionId( sessionIdBuf );
			TestSession::sessionMutex.lock();
			if( TestSession::sessions.erase( sessionId ) == 0 )
			{
				TestSession::sessionMutex.unlock();
				Exception::raise( "sessions.erase( %s ) failed", sessionId.c_str() );
			}
			if( TestSession::sessions.size() == 0 )
				TestSession::done.signal();
			TestSession::sessionMutex.unlock();
		}

		static Event done;
		static size_t finished;

	private:

		static map< std::string, TestSession * > sessions;
		static mutex sessionMutex;
};

map< std::string, TestSession * > TestSession :: sessions;
mutex TestSession :: sessionMutex;
Event TestSession :: done;
size_t TestSession :: finished = 0;

int
main( int argc, char **argv )
{
	try
	{
		if( argc != 3 )
			Exception::raise( "Usage: %s num_sessions num_echos", argv[ 0 ] );

		// proxy test service
		ProxyServiceContext *proxyContext = new ProxyServiceContext( "localhost:666", "localhost:667" );
		ProxyService *proxyService = new ProxyService( proxyContext );
		proxyService->run();
		proxyService->detach();

		// echo test service
		EchoServiceContext *context = new EchoServiceContext( "localhost:667" );
		EchoService *service = new EchoService( context );
		service->run();
		service->detach();

		sleep( 1 );

		// run test client session
		int numSessions = atoi( argv[ 1 ] );
		int numEchos = atoi( argv[ 2 ] );
		for( int i = 0; i < numSessions; i++ )
		{
			TestSessionContext *context = new TestSessionContext( numEchos );
			TestSession *test = new TestSession( context );	
			test->run();
			test->detach();
		}
		TestSession::done.wait();
	}
	catch( const char *error )
	{
		Log::console( "%s: test failed [%s]", argv[ 0 ], error ); 
		exit( -1 );
	}
	Log::console( "%s: test passed", argv[ 0 ] );
}

