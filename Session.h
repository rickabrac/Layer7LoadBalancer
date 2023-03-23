//
//  Session.h
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

# ifndef _Session_h_
# define _Session_h_

# include "Thread.h"
# include <openssl/ssl.h>
# include <set>

class Service;
class Session;

class SessionContext : public ThreadContext
{
    public:

    SessionContext( Service *daemon, int clientSocket, SSL *clientSSL = nullptr ); // , const char *protocolSessionAttribute = nullptr );
	~SessionContext();
	int clientSocket;
	SSL *clientSSL;

    protected:

    Service *service;
	Session *session = nullptr;

    friend class Session;
    friend class Service;
};

class Session : public Thread
{
    public:

	Session( SessionContext *context );
	virtual ~Session();

//    private:

	SessionContext *context;

    friend class Service; 
};

# endif // _Session_h_

