//
//  SocketAddress.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# ifndef _SocketAddress_h_
# define _SocketAddress_h_

# include <arpa/inet.h>
# include <map>
# include <mutex>
# include <strings.h>
# include <string.h>
# include <sys/socket.h>
# include <stdarg.h>
# include <string>
# include <stdlib.h>
# import <netinet/in.h>
# import <arpa/inet.h>

using namespace std;

class SocketAddress
{
    public:

	SocketAddress( const char *addrStr );

    private:

	struct sockaddr_in sockaddr_in;
	void debug( void );
	string hostname;
	static std::map< std::string, struct in_addr * > hosts;
	static std::mutex hostMutex;

    friend class Service;
    friend class Connection;
};

# endif // _SocketAddress_h_

