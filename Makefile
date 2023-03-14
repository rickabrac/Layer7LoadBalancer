#
#  Makefile
#  Makefile for L7LB (Layer7LoadBalancer)
#  Created by Rick Tyler
#

SSL_HDR_DIR = /usr/local/include
SSL_LIB_DIR = /usr/local/lib

CXX         = g++
CXXFLAGS    = -g -pthread -O3 -fPIC -Wuninitialized -Wall -Wextra -I$(SSL_HDR_DIR) -std=c++1z -Wno-c++11-extensions
# CXXFLAGS    = -g -pthread -O3 -fPIC -Wuninitialized -Wall -Wextra -I$(SSL_HDR_DIR) -std=c++14 -Wno-c++11-extensions

SOURCES  = SocketAddress.cc Connection.cc Service.cc Session.cc ProxySession.cc

OBJECTS  = $(SOURCES:.cc=.o)

HEADERS  = $(SOURCES:.cc=.h) Thread.h Event.h Log.h Exception.h

all: testtls testtcp l7lb 

$(OBJECTS): $(HEADERS)

testtls: $(OBJECTS) TestTLS.cc
	$(CXX) $(CXXFLAGS) $(OBJECTS) -L/usr/local/lib -lssl -lcrypto TestTLS.cc -o testtls

testtcp: $(OBJECTS) TestTCP.cc
	$(CXX) $(CXXFLAGS) $(OBJECTS) -L/usr/local/lib -lssl -lcrypto TestTCP.cc -o testtcp

l7lb: $(OBJECTS) L7LB.cc L7LBConfig.cc
	$(CXX) $(CXXFLAGS) $(OBJECTS) -L/usr/local/lib -lssl -lcrypto L7LB.cc -o l7lb 

clean:
	rm -f testtls testtcp l7lb *.o
	rm -rf *.dSYM
