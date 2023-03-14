//
//  SocketAddress.h
//  L7LB (Layer7LoadBalancer)
//  Created by Rick Tyler
//

# ifndef _SocketAddress_h_
# define _SocketAddress_h_

# include <arpa/inet.h>
# include <map>
# include <mutex>

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

