//
//  Connection.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# ifndef _Connection_h_
# define _Connection_h_

# include "SocketAddress.h"
# include <openssl/ssl.h>
# include <openssl/err.h>

using namespace std;

class Connection
{
    public:

	Connection( const char *destStr, bool secure = true ); 
	~Connection();
	ssize_t write( void *data, size_t len );
	ssize_t peek( void *buf, size_t len );
	ssize_t read( void *buf, size_t len );
	ssize_t pending( void ); 
	int socket;
	static SSL_CTX *ssl_ctx;

    private:

	SocketAddress *sockAddr = nullptr;
	SSL *ssl = nullptr;
	bool useTLS;
	static mutex mutex;
};

# endif // _Connection_h_

