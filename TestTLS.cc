//
//  TestTLS.cc
//
//  TLS-mode test for Thread, Service, Connection, Session, ProxySession
//
//  Created by Rick Tyler
//

# include "Service.h"
# include "ProxySession.h"
# include "Connection.h"
# include "Exception.h"
# include "Event.h"
# include "Log.h"

# define TRACE    1

using namespace std;

class ProxyServiceContext : public ServiceContext 
{
	public:

		ProxyServiceContext( const char *listenStr, const char *destStr, const char *certFile, const char *keyFile ) :
			ServiceContext( listenStr, certFile, keyFile )
		{
# if TRACE
			Log::log( "TestProxyServiceContext::TestProxyServiceContext()" );
# endif // TRACE
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

		ProxySession *getSession( int clientSocket, SSL *clientSSL )
		{
			ProxySessionContext *context = new ProxySessionContext( this, clientSocket, clientSSL, "localhost:667", true );
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

// echo received data back to client 

class EchoSession : public Session 
{
	public:

		EchoSession( EchoSessionContext *context ) : Session( context ) { }

		ThreadMain main( void ) { return( (ThreadMain) _main ); }

		static void _main ( EchoSessionContext *context )
		{
# if TRACE
			Log::log( "EchoSession::_main( %p ) RUN", context );
# endif // TRACE
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
				
				(void) SSL_write( context->clientSSL, "", 0 ); 

				for( ;; )
				{
					FD_ZERO( &fdset );
					FD_ZERO( &empty_fdset );
					FD_SET( context->clientSocket, &fdset );

					if( (selected = select( FD_SETSIZE, &fdset, &empty_fdset, &empty_fdset, &timeout)) >= 0 )
					{
						if( FD_ISSET( context->clientSocket, &fdset ) )
						{
							// client has data ready
							bzero( context->buf, sizeof( context->buf ) );

							if( (context->len = (int) SSL_read( context->clientSSL,
								context->buf, sizeof( context->buf ) )) > 0 )
							{
								context->buf[ context->len - 1 ] = '\0';
# if TRACE
								if( strlen( context->buf ) )
									Log::log( "EchoSession::main( %p ) ECHO \"%s\"", context, context->buf );
# endif // TRACE
								// send back to client
								ssize_t sent, total = 0;
								while( (sent = SSL_write( context->clientSSL, ((char *) context->buf) + total, context->len )) < context->len )
								{
									if( sent < 0 )
									{
										printf( "EchoSession[ %p ]::_main: SSL_write() failed [%d] (%s)",
											context, errno, strerror( errno ) );
										return;
									}
									context->len -= sent;
									total += sent;
								}

# if TRACE
								Log::log( "EchoSession::main( %p ) SENT \"%s\"", context, context->buf );
# endif // TRACE
								if( sent == 0 )
								{
									Log::log( "EchoSession[ %p ]._main: SSL_write() FAILED? (%s)",
										context, ERR_error_string( ERR_get_error(), NULL ) );
								}
							}
							else if( context->len < 0 )
							{
# if TRACE
								Log::log( "EchoSession[ %p ]::_main: EXIT", context);
# endif // TRACE
								return;
							}
						}
					}
					else
					{
						Log::log( "EchoSession[ %p ]::_main: select() < 0", context ); 
						return;
					}
				}
			}
			catch( const char *msg )
			{
				// exception skips to terminate session below
				Log::log( "EchoSession[ %p ]::_main: %s", context, msg );
			}
		}
};

class EchoServiceContext : public ServiceContext
{
	public:

		EchoServiceContext( const char *listenStr, const char *certFile, const char *keyFile )
			: ServiceContext( listenStr, certFile, keyFile ) { }
};

class EchoService : public Service
{
	public:

		EchoService( EchoServiceContext *context ) : Service( context ) { }

        ThreadMain main( void ) { return( (ThreadMain) _main ); }

	protected:

		Session *getSession( int clientSocket, SSL *clientSSL )
		{
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
# if TRACE
			Log::log( "TestSession::~TestSession()" );
# endif // TRACE
			delete( context );
		}

        ThreadMain main( void ) { return( (ThreadMain) _main ); }

		static void _main( TestSessionContext *context )
		{
			Connection *connection = NULL;
			try
			{
# if TRACE
				Log::log( "TestSession::main( %p ) RUN", context );
# endif // TRACE
				connection = new Connection( "localhost:666" );
				int i;
				TestSession::sessionMutex.lock();
				TestSession::sessions[ context->session->tid() ] = context->session;
				TestSession::sessionMutex.unlock();
				for( i = 0; i < context->numEchos; i++ )
				{
					char outBuf[ 128 ];
					sprintf( outBuf, "TEST#%d", i );
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
				Log::log( "TestSession[ %p ]::_main: %d strings verified [session #%d].",
					context, i, TestSession::finished );
				context->session->stop();
			}
			catch( const char *error )
			{
				if( connection )
					delete( connection );
				Log::log( "TestSession[ %p ]::_main: %s", context, error );
				::exit( -1 );
			}
		}

		void stop( void )
		{
			TestSession::sessionMutex.lock();
			string tid = this->tid();
			if( TestSession::sessions.erase( tid ) == 0 )
			{
				TestSession::sessionMutex.unlock();
				Exception::raise( "sessions.erase( %s ) failed", tid.c_str() );
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
		ProxyServiceContext *proxyContext = new ProxyServiceContext(
			"localhost:666", "localhost:667", "localhost.crt", "localhost.key" );
		ProxyService *proxyService = new ProxyService( proxyContext );
		proxyService->run();
		proxyService->detach();

		// echo test service
		EchoServiceContext *echoContext = new EchoServiceContext(
			"localhost:667", "localhost.crt", "localhost.key"
		);
		EchoService *echoService = new EchoService( echoContext );
		echoService->run();
		echoService->detach();
		sleep( 1 );

		// launch echo sessions
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
		Log::log( "%s: test failed [%s]", argv[ 0 ], error ); 
		exit( -1 );
	}
	Log::log( "%s: test passed", argv[ 0 ] );
}
