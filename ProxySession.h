//
//  ProxySession.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# ifndef _ProxySession_h_
# define _ProxySession_h_

# include "Thread.h"
# include "Service.h"
# include "Session.h"
# include "Connection.h"

class ProxySessionContext : public SessionContext
{
  public:

	ProxySessionContext
	(
		Service *service,
		int clientSocket,
		SSL *clientSSL,
		const char *destStr,
		bool useTLS,
		string protocolAttribute = "",
		const char *protocolHeaderStart = "HTTP/1.1 200 OK\r\n",
		const char *protocolAttributeDelimiter = ":",
		const char *protocolAttributeEnd = "\r\n",
		const char *protocolHeaderEnd = "\r\n"
	);
	~ProxySessionContext();

  private:

	const char *destStr;
	bool useTLS; 
	char *buf;
	size_t bufLen;
	string protocolAttribute;
	const char *protocolHeaderStart;
	const char *protocolAttributeDelimiter;
	const char *protocolAttributeEnd;
	const char *protocolHeaderEnd;
	Connection *proxy = nullptr;
	bool clientDataReady( void ); 

  friend class ProxySession;
};

class ProxySession : public Session
{
	public:

		ProxySession( ProxySessionContext *context );
		~ProxySession();

	private:

		static void _main( ProxySessionContext *context );
		ThreadMain main( void ) { return( (ThreadMain) _main ); }
		ProxySessionContext *context;

	friend class Service; 
};

# endif // _ProxySession_h_

