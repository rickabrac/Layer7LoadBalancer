#
#  Makefile
#  Layer7LoadBalancer
#  Created by Rick Tyler
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License You may obtain a copy of the License at
#
#  https:#www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

SSL_HDR_DIR = /usr/local/include
SSL_LIB_DIR = /usr/local/lib

CXX         = g++
CXXFLAGS    = -g -fPIC -Wuninitialized -Wall -Wextra -I$(SSL_HDR_DIR) -I. -std=c++1z # -Wno-c++11-extensions

SOURCES  = SocketAddress.cc Connection.cc Service.cc Session.cc ProxySession.cc

OBJECTS  = $(SOURCES:.cc=.o)

HEADERS  = $(SOURCES:.cc=.h) Thread.h Event.h Log.h Exception.h L7LBConfig.h

all: l7lb testtls testtcp testl7lb

$(OBJECTS): $(HEADERS)

testtls: $(OBJECTS) TestTLS.cc
	$(CXX) $(CXXFLAGS) $(OBJECTS) -L/usr/local/lib -lssl -lcrypto TestTLS.cc -o testtls

testtcp: $(OBJECTS) TestTCP.cc
	$(CXX) $(CXXFLAGS) $(OBJECTS) -L/usr/local/lib -lssl -lcrypto TestTCP.cc -o testtcp

# testl7lb: $(OBJECTS) TestTCP.cc
#	$(CXX) $(CXXFLAGS) $(OBJECTS) -L/usr/local/lib -lssl -lcrypto TestL7LB.cc -o testl7lb

l7lb: $(OBJECTS) L7LB.cc
	$(CXX) $(CXXFLAGS) $(OBJECTS) -L/usr/local/lib -lssl -lcrypto L7LB.cc -o l7lb 

clean:
	rm -f testtls testtcp l7lb *.o
	rm -rf *.dSYM
