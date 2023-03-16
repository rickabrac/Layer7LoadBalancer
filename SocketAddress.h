//
//  SocketAddress.h
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

