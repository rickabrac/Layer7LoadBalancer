//
//  ProxySession.cc
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

# include "ProxySession.h"
# include "Exception.h"
# include "Log.h"
# include <fcntl.h>
# include <map>
# include <string.h>

// # define TRACE    1

ProxySessionContext :: ProxySessionContext
(
	Service *service,
	int clientSocket,
	SSL *clientSSL,
	const char *destStr,
	bool useTLS,
	const char *protocolSessionAttribute,
	const char *protocolStartHeader,
	const char *protocolAttributeSeparator,
	const char *protocolEndAttribute,
	const char *protocolEndHeader
) : SessionContext( service, clientSocket, clientSSL )
{
# if TRACE
	Log::log( "ProxySessionContext::ProxySessionContext()" );
# endif // TRACE
	this->service= service;
	this->clientSocket = clientSocket;
	this->clientSSL = clientSSL;
	this->destStr = destStr;
	this->useTLS = useTLS;
	this->protocolSessionAttribute = protocolSessionAttribute;
	this->protocolStartHeader = protocolStartHeader;
	this->protocolAttributeSeparator = protocolAttributeSeparator;
	this->protocolEndAttribute  = protocolEndAttribute;
	this->protocolEndHeader = protocolEndHeader;
	this->bufLen = service->bufLen;
	this->buf = (char *) malloc( service->bufLen );
}

ProxySessionContext :: ~ProxySessionContext()
{
# if TRACE
	Log::log( "ProxySessionContext::~ProxySessionContext()" );
# endif // TRACE
	if( buf ) 
		free( buf );
	if( proxy ) 
		delete( proxy );
}

ProxySession :: ProxySession( ProxySessionContext *context ) : Session( context )
{
# if TRACE
	Log::log( "ProxySession::ProxySession()" );
# endif // TRACE
}

ProxySession :: ~ProxySession()
{
# if TRACE
	Log::log( "ProxySession::~ProxySession()" );
# endif // TRACE
}

void
ProxySession :: _main( ProxySessionContext *context )
{
# if TRACE
	Log::log( "ProxySession::_main[ %p ] RUN", context );
# endif // TRACE
	try
	{
		context->proxy = new Connection( context->destStr, context->useTLS );
	}
	catch( const char *error )
	{
		Log::log( "ProxySession[ %p ]::_main: Connection() failed (%s)", context, error );
		delete( context );
		return;
	}

	ssize_t pending = 0;
	int loops = 0; 

	for( ;; )
	{
		fd_set fdset;
		fd_set empty_fdset;
		struct timeval timeout;
		bzero( &timeout, sizeof( timeout ) );
		timeout.tv_sec = 0; 
		timeout.tv_usec = 10000;
		int selected;

		FD_ZERO( &fdset );
		FD_ZERO( &empty_fdset );
		FD_SET( context->clientSocket, &fdset );
		FD_SET( context->proxy->socket, &fdset );

		try
		{
			if( pending )
				Exception::raise( "pending != 0" );
# if TRACE
//			Log::log( "ProxySession[ %p ]::_main: select( %d, %d )...",
//				context, context->clientSocket, context->proxy->socket );
# endif // TRACE
			if( (selected = select( FD_SETSIZE, &fdset, &empty_fdset, &empty_fdset, &timeout )) >= 0 )
			{
				if( selected == 0 )
				{
# if TRACE
//					Log::log( "SELECT() RETURNED selected=%d errno=%d (%s)", selected, errno, strerror( errno ) );
# endif // TRACE

					if( loops > 0 )
					{
# if TRACE
						if( loops % 20 == 0 )
							Log::log( "ProxySession[ %p ]::_main: *IDLE*", context );
# endif // TRACE
						++loops;
						continue;
					}
					else
					{
						if( context->clientSSL )
						{
							SSL_shutdown( context->clientSSL );
//							SSL_free( context->clientSSL );	// crashes when SSL_free() is called here
							context->clientSSL = nullptr;
						}
						delete( context );
						return;
					}
				}

				size_t loopReads = 0;

				if( loops == 0 || FD_ISSET( context->clientSocket, &fdset ) || (context->clientSSL && SSL_pending( context->clientSSL )) )
				{
					// client has data ready
					bzero( context->buf, context->bufLen );

					ssize_t len;

					if( context->clientSSL )
						len = (int) SSL_read( context->clientSSL, context->buf, (int) context->bufLen );
					else
						len = (int) recv( context->clientSocket, context->buf, context->bufLen, 0 );

					if( len > 0 )
					{
						// send to server 
						++loopReads;
						ssize_t sent, total = 0;
						size_t recvLen = len;
# if TRACE
 						context->buf[ len ] = '\0';
						Log::log( "SENDING %d BYTES TO SERVER [\n%s]", len, context->buf );
# endif // TRACE
						while( len
							&& (sent = context->proxy->write( ((char *) context->buf) + total, len )) <= len )
						{
							if( sent < 0 )
							{
# if TRACE
								if( context->clientSSL )
								{
									Log::log( "ProxySession[ %p ]::_main: context->proxy->write() failed [%d] (%s)",
										context, errno, ERR_error_string( ERR_get_error(), NULL ) ); 
								}
								else
								{
									Log::log( "ProxySession[ %p ]::_main: context->proxy->write() failed [%d] (%s)",
										context, errno, strerror( errno ) );
								}
# endif // TRACE
								delete( context );
								return;
							}
# if TRACE
							if( sent < len )
							{
								Log::log( "ProxySession[ %p ]::_main( %p ) PARTIAL SEND TO SERVER (sent = %zu)",
									context, sent );
							}
# endif // TRACE
							len -= sent;
							total += sent;
						}

						if( recvLen == context->bufLen )
						{
							context->service->bufLenMutex.lock();
							context->service->bufLen *= 2;
							context->bufLen = context->service->bufLen;
							context->service->bufLenMutex.unlock();
							free( context->buf );
							context->buf = (char *) malloc( context->bufLen );
# if TRACE
							Log::log( "ProxySession[ %p ]::_main: BUFLEN=%d", context, context->service->bufLen ); 
# endif // TRACE
						}
					}
					else if( errno != EAGAIN && len < 0 )
					{
						SSL_shutdown( context->clientSSL );
//						SSL_free( context->clientSSL );	// crashes if SSL_free() called here
						context->clientSSL = nullptr;
# if TRACE
						if( context->clientSSL )
						{
							Log::log( "ProxySession[ %p ]::_main: SSL_read() failed (%s)",
								context, ERR_error_string( ERR_get_error(), NULL ) ); 
						}
						else
						{
							Log::log( "ProxySession[ %p ]::_main: recv() failed [%d] (%s)",
								context, errno, strerror( errno ) ); 
						}
# endif // TRACE
						delete( context );
						return;
					}
				}

				if( FD_ISSET( context->proxy->socket, &fdset ) || context->proxy->pending() ) 
				{
					// server has data ready
					bzero( context->buf, context->bufLen );

					// receive from server
					if( (pending = context->proxy->read( context->buf, context->bufLen )) > 0 )
					{
# if TRACE
						Log::log( "ProxySession[ %p ]::_main: RECEIVED %d BYTES FROM SERVER", context, pending );
//						Log::log( "ProxySession[ %p ]::_main: RECEIVED FROM SERVER [%s]", context, context->buf );
# endif // TRACE
						++loopReads;
						ssize_t sent;
						ssize_t total = 0;
						size_t recvLen = pending;
						if( context->clientSSL )
							sent = SSL_write( context->clientSSL, context->buf + total, (int) pending );
						else
							sent = send( context->clientSocket, context->buf + total, (int) pending, 0 );
						while( pending && sent <= pending ) 
						{
							if( sent < 0 )
							{
# if TRACE
								Log::log( "ProxySession[ %p ]::_main: PENDING=%d SENT=%d", context, pending, sent );
# endif // TRACE
								if( context->clientSSL )
								{
# ifdef TRACE
									Log::log( "ProxySession[ %p ]::_main SSL_write() failed [%d] (%s)",
										context, errno, ERR_error_string( ERR_get_error(), NULL ) );
# endif // TRACE
								}
								else
								{
# ifdef TRACE
									Log::log( "ProxySession[ %p ]::_main: send() failed [%d] (%s)",
										context, errno, strerror( errno ) );
# endif // TRACE
								}
								delete( context );
								return;
							}
							if( pending == 0 )
							{
								Log::log( "USLEEP( 1000 )" );
								usleep( 1000 );
							}
							pending -= sent;
							total += sent;
						}
# if TRACE
//						Log::log( "ProxySession[ %p ]::_main: SENT TO CLIENT [\n%s]", context, context->buf );
# endif // TRACE
						if( strncmp( context->buf, context->protocolStartHeader, strlen( context->protocolStartHeader )  ) == 0 )
						{
							ProxySessionContext *proxySessionContext = (ProxySessionContext *) context;
							char lineBuf[ 1024 ];
							char *line = context->buf + 17;
							char *newline;
							const char *endOfRecord = context->protocolEndAttribute;
							const char *recordSeparator = context->protocolAttributeSeparator; 
							while( (newline = strstr( line, endOfRecord )) )
							{
								if( strncmp( line, endOfRecord, strlen( endOfRecord ) ) == 0 )
									break;
								bzero( lineBuf, sizeof( lineBuf ) );
								memcpy( lineBuf, line, newline - line );
								char *c1;
								for( c1 = lineBuf; *c1 && isspace( *c1 ); c1++ );
								char *c2;
								for( c2 = c1; strncmp( c2, recordSeparator, strlen( recordSeparator ) ) != 0 && strstr( c2, endOfRecord ) != c2; c2++ );
								if( strncmp( c2, recordSeparator, strlen( recordSeparator ) ) == 0 )
								{
									*c2++ = '\0';
									const char *name = c1;
									while( isspace( *c2 ) )
										++c2;
									if( strcmp( proxySessionContext->protocolSessionAttribute, name ) == 0 )
									{
# if TRACE
										Log::log( "ATTRIBUTE [%s: %s]", name, c2 );
# endif // TRACE
										string value( c2 );
										proxySessionContext->service->notifySessionProtocolAttribute( &value );
										break;
									}
								}
								line = newline + 2;
							}
						}

						if( recvLen == context->bufLen )
						{
							context->service->bufLenMutex.lock();
							context->service->bufLen *= 2;
							context->bufLen = context->service->bufLen;
							context->service->bufLenMutex.unlock();
							free( context->buf );
							context->buf = (char *) malloc( context->bufLen );
# if TRACE
							Log::log( "ProxySession[ %p ]::_main: BUFLEN=%d", context, context->service->bufLen ); 
# endif // TRACE
						}
					}
					else if( errno != EAGAIN && pending <= 0 )
					{
# if TRACE
						Log::log( "ProxySession[ % ]::_main: END SESSION (pending <= 0)" );

						if( context->clientSSL )
						{
							Log::log( "ProxySession[ %p ]::_main: SSL_read() failed [%d] (%s)",
								context, errno, ERR_error_string( ERR_get_error(), NULL ) );
						}
						else
						{
							Log::log( "ProxySession[ %p ]._main: recv() failed [%d] (%s)",
								context, errno, strerror( errno ) );
						}
# endif // TRACE
						delete( context );
						return;
					}
				}

				if( loopReads == 0 )
				{
					if( errno == 60 )
					{
# if TRACE
						Log::log( "ProxySession[ %p ]._main: ERRNO == 60", context );
# endif // TRACE
						delete( context );	
						return;
					}
# if TRACE
					Log::log( "loopReads == 0 errno=%d", errno );
# endif // TRACE
					usleep( 1000 );
				}
			}
			else
			{
# if TRACE
				Log::log( "ProxySession[ %p ]::_main: select() failed (%s) [%d]", context, strerror( errno ), errno );
# endif // TRACE
				SSL_shutdown( context->clientSSL );
//				SSL_free( context->clientSSL );
				context->clientSSL = nullptr;
				delete( context );
				return;
			}
			++loops;
		}
		catch( const char *error )
		{
			Log::log( "ProxySession[ %p ]::_main: : %s", error );
			::exit( -1 );
		}
	}
}

