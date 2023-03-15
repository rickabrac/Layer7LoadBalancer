//
//  ProxySession.cc
//  L7LB (Layer7LoadBalancer)
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
# include "Event.h"
# include "Log.h"
# include <fcntl.h>

// # define TRACE    1

ProxySessionContext :: ProxySessionContext( Service *service, int clientSocket, SSL *clientSSL, const char *destStr, bool useTLS )
	: SessionContext( service, clientSocket, clientSSL )
{
# if TRACE
	Log::log( "ProxySessionContext::ProxySessionContext()" );
# endif // TRACE
	this->service= service;
	this->clientSocket = clientSocket;
	this->clientSSL = clientSSL;
	this->destStr = destStr;
	this->useTLS = useTLS;
	this->bufLen = service->bufLen;
	this->buf = (char *) malloc( service->bufLen );
}

ProxySessionContext :: ~ProxySessionContext()
{
# if TRACE
	Log::log( "ProxySessionContext::~ProxySessionContext()" );
# endif // TRACE
	if( buf != nullptr )
		free( buf );
	if( proxy != nullptr )
		delete( proxy );
}

ProxySession :: ProxySession( ProxySessionContext *context ) : Session( context )
{
# if TRACE
	Log::log( "ProxySession::ProxySession()" );
# endif // TRACE
}

ProxySession :: ~ProxySession ()
{
# if TRACE
	Log::log( "ProxySession::~ProxySession()" );
# endif // TRACE
}

void
ProxySession :: _main ( ProxySessionContext *context )
{
# if TRACE
	Log::log( "ProxySession::_main( %p ) RUN", context );
# endif // TRACE
	try
	{
		context->proxy = new Connection( context->destStr, context->useTLS );
	}
	catch( const char *error )
	{
		Log::log( "ProxySession[ %p ]::_main: Connection() failed", context );
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
		if( context->useTLS )
		{
			timeout.tv_sec = 1; 
			timeout.tv_usec = 0;
		}
		else
		{
			timeout.tv_sec = 0; 
			timeout.tv_usec = 100000;
		}
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
				if( selected == 0 && loops > 0 )
				{
# if TRACE
					Log::log( "ProxySession[ %p ]::_main: *IDLE*", context );
# endif // TRACE
					++loops;
					continue;
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
// context->buf[ len// ] = '\0';
// Log::log( "SENDING %d BYTES [\n%s]", len, context->buf );
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
# if TRACE
						Log::log( "ProxySession[ %p ]::_main: SEND TO SERVER [\n%s]",
							context, context->buf );
# endif // TRACE
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
						else
							context->service->bufLenMutex.unlock();
					}
					else if( errno != EAGAIN && len < 0 )
					{
# if TRACE
						if( context->clientSSL )
						{
							Log::log( "ProxySession[ %p ]::_main: SSL_read() failed [%d] (%s)",
								context, errno, ERR_error_string( ERR_get_error(), NULL ) );
						}
						else
						{
							Log::log( "ProxySession[ %p ]::_main: recv() failed [%d] (%s)",
								context, errno, ERR_error_string( ERR_get_error(), NULL ) );
						}
# endif // TRACE
						delete( context );
						return;
					}
// 					else if( len == 0 )
// 					{
// 						if( context->clientSSL )
// 							(void) SSL_write( context->clientSSL, (void *) "", 0 ); 
// 						else
// 							(void) send( context->clientSocket, (void *) "", 0, 0 );
// # if TRACE
// 						Log::log( "ProxySession[ %p ]::_main: WROTE NOTHING TO CLIENT", context );
// # endif // TRACE
// 					}
				}

				if( FD_ISSET( context->proxy->socket, &fdset ) || context->proxy->pending() ) 
				{
					// server has data ready
					bzero( context->buf, context->bufLen );
					// receive from server
					if( (pending = context->proxy->read( context->buf, context->bufLen )) > 0 )
					{
//						Log::log( "ProxySession[ %p ]::_main: RECEIVED %d BYTES FROM SERVER", context, pending );
# if TRACE
//						Log::log( "ProxySession[ %p ]::_main: RECEIVED FROM SERVER [%s]",
//							context, context->buf );
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
							// send to client
							if( sent < 0 )
							{
								Log::log( "SENT=%d PENDING=%d", sent, pending );
								if( context->clientSSL )
								{
									Log::log( "ProxySession[ %p ]::_main SSL_write() failed [%d] (%s)",
										context, errno, ERR_error_string( ERR_get_error(), NULL ) );
								}
								else
								{
									Log::log( "ProxySession[ %p ]::_main: send() failed [%d] (%s)",
										context, errno, strerror( errno ) );
								}
								delete( context );
								return;
							}
							if( pending == 0 )
								usleep( 250 );
							pending -= sent;
							total += sent;
						}
# if TRACE
//						Log::log( "ProxySession[ %p ]::_main: SEND TO CLIENT [\n%s]",
//							context, context->buf );
# endif // TRACE
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
					usleep( 100000 );
			}
			else
			{
				Log::log( "ProxySession[ %p ]::_main: select() failed (%s) [%d]", context, strerror( errno ), errno );
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

