//				
//  L7LBConfig.h
//  Layer7LoadBalancer
//  Created by Rick Tyler
//
//  SPDX-License-Identifier: MIT

# ifdef _L7LBConfig_h_

class SessionConfig
{
    public:

	SessionConfig( const char *destStr, bool useTLS )
	{
		this->destStr = destStr;
		this->useTLS = useTLS;
	}

    private:

	const char *destStr;
	bool useTLS;

    friend class L7LBService;
};

class ServiceConfig
{
    public:

	ServiceConfig(
		string listenStr,
		string certPath,
		string keyPath,
		string trustPath,
		string sessionCookie,
		vector< SessionConfig * > *sessionConfigs )
	{

# if TRACE
		cout << "ServiceConfig( \""
			<< listenStr << "\", \""
			<< certPath << "\", \""
			<< keyPath << "\", "
			<< trustPath << "\", "
			<< sessionCookie << ", " 
			<< sessionConfigs << endl;
# endif // TRACE
		this->listenStr = listenStr;
		this->certPath = certPath;
		this->keyPath = keyPath;
		this->trustPath = trustPath;
		this->sessionCookie = sessionCookie;
		this->sessionConfigs = sessionConfigs;
	}

	string listenStr;
	string certPath;
	string keyPath;
	string trustPath;
	string sessionCookie;
	vector< SessionConfig * > *sessionConfigs;	
};

class L7LBConfig
{
    public:

	L7LBConfig( const char *filePath )
	{
		this->filePath = filePath;
		this->configFile = new ifstream( filePath );
	}

	string *nextToken( void )
	{
		while( line == "" || line.substr( 0, 1 ) == "#" )
		{
			if( ss )
			{
				delete( ss );
				ss = nullptr;
			}
			if( !getline( *configFile, line ) )
				return nullptr;
			ss = new stringstream( line );
		}
		token = "";
		(void) getline( *ss, token, ' ' );
		while( token == "" )
		{
			if( ss )
			{
				delete( ss );
				ss = nullptr;
			}
			if( !getline( *configFile, line ) )
				return nullptr;
			ss = new stringstream( line );
			token = "";
			(void) getline( *ss, token, ' ' );
		}
		size_t start = token.find_first_not_of( " \t" );
		token = (start == std::string::npos) ? "" : token.substr( start );

		return( new string( token.c_str() ) );
	}

	void parseFile( void )
	{
		if( !configFile->is_open())
			Exception::raise( "%s not found", filePath );
		ServiceConfig *serviceConfig;
		try
		{
			while( (serviceConfig = parseServiceConfig()) )
				serviceConfigs.push_back( serviceConfig );
		}
		catch( const char *error )
		{
			cout << "LBL7Config.parseFile: " << error << endl;
			::exit(-1);
//			Exception::raise( "L7LBConfig::parseFile: %s", error ); 
		}
	}

	ServiceConfig *parseServiceConfig( void )
	{
		string *protocol = nullptr;
		string *listenStr = nullptr;
		string *keyPath = nullptr;
		string *certPath = nullptr;
		string *trustPath = nullptr;
		string *sessionCookie = nullptr;
		if( (protocol = nextToken()) == nullptr )
			return nullptr;
		if( *protocol == "#" )
			return nullptr;
		if( (listenStr = nextToken()) == nullptr )
			Exception::raise( "### expected listenStr" );
		string *s;
		if( (s = nextToken()) == NULL || *s != "{" )
			Exception::raise( "expected {" );
		string *name;
		vector<SessionConfig *> *sessionConfigs = new vector<SessionConfig *>();
		while( (name = nextToken()) != nullptr )
		{
			if( *name == "}" )
				break;
			string *value;
			if( (value = nextToken()) == nullptr ) 
				Exception::raise( "expected value" );
# if TRACE
			cout << "protocol=" << *protocol << " name=" << *name << " value=" << *value << endl;
# endif // TRACE
			if( *name == "CERTIFICATE" )
				certPath = value;
			else if( *name == "KEY" )
				keyPath = value;
			else if( *name == "TRUST" )
				trustPath = value;
			else if( *name == "SESSION-COOKIE" )
				sessionCookie = value;
			else if( *name == "TCP" || *name == "TLS" )
			{
				const char *destStr = value->c_str();
				bool useTLS = strcmp( name->c_str(), "TLS" ) == 0 ? true : false;
				sessionConfigs->push_back( new SessionConfig( destStr, useTLS ) );
			}
			else
			{
				Exception::raise( "unknown parameter: %s", name->c_str() );
			}
		}
# if TRACE
		cout << "PROTOCOL=" << *protocol << endl;
		cout << "LISTEN=" << *listenStr << endl;
# endif // TRACE
		return new ServiceConfig(
			*listenStr,
			keyPath == nullptr ? "" : *keyPath,
			certPath == nullptr ? "" : *certPath,
			trustPath == nullptr ? "" : *trustPath,
			sessionCookie == nullptr ? "" : *sessionCookie,
			sessionConfigs
		);
	}

	vector<ServiceConfig *> serviceConfigs;	
	static L7LBConfig *config;

    private:

	const char *filePath;
	ifstream *configFile;
	string line = ""; 
	string token = "";
	stringstream *ss = nullptr;
};

# endif // _L7LBConfig_h_
