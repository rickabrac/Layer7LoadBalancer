//
//  Connection.h
//	L7LB (Layer7LoadBalancer)
//  Created by Rick Tyler
//

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
		const char *error( int errNum );
		static void debug( void );
		int socket;

	private:

		SSL *ssl = nullptr;
		static map< string, Connection * > connections;
		static mutex connectionMutex;
		static SSL_CTX *ssl_ctx;
		SocketAddress *sockAddr = nullptr;
		bool secure;
};

# endif // _Connection_h_

