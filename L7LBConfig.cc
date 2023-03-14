//				
// L7LBConfig.cc
//
// Created by Rick Tyler
//

class SessionConfig
{
	public:

		SessionConfig( const char *destStr, bool secure )
		{
			this->destStr = destStr;
			this->secure = secure;
		}

	private:

		const char *destStr;
		bool secure;

	friend class L7LBService;
};

class ServiceConfig
{
	public:

		ServiceConfig(
			string listenStr,
			string certPath,
			string keyPath,
			vector<string> *sessionCookies,
			vector<SessionConfig *> *sessionConfigs )
		{
# if TRACE
			cout << "ServiceConfig( \""
				<< listenStr << "\", \""
				<< certPath << "\", \""
				<< keyPath << "\", "
				<< sessionConfigs << ", "
				<< sessionCookies << endl;
# endif // TRACE
			this->listenStr = listenStr;
			this->sessionCookies = sessionCookies;
			this->certPath = certPath;
			this->keyPath = keyPath;
			this->sessionConfigs = sessionConfigs;
		}

		string listenStr;
		string certPath;
		string keyPath;
		vector<string> *sessionCookies;
		vector<SessionConfig *> *sessionConfigs;	
};

class L7LBConfig
{
	public:

		L7LBConfig( const char *fileName )
		{
			this->configFile = new ifstream( fileName );
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
				{
					return nullptr;
				}
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
				{
					return nullptr;
				}
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
			{
				Exception::raise( "configFile.is_open() == false" );
				return;
			}
			ServiceConfig *serviceConfig;
			try
			{
				while( (serviceConfig = parseServiceConfig()) )
				{
					serviceConfigs.push_back( serviceConfig );
				}
			}
			catch( const char *msg )
			{
				cout << "L7LBConfig::parseFile: " << msg << endl;
				::exit( -1 );
			}
		}

		ServiceConfig *parseServiceConfig( void )
		{
			string *protocol = nullptr;
			string *listenStr = nullptr;
			string *keyPath = nullptr;
			string *certPath = nullptr;
			vector<string> *sessionCookies = new vector<string>();
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
			keyPath = nullptr;
			certPath = nullptr;
			vector<SessionConfig *> *sessionConfigs = new vector<SessionConfig *>();
			while( (name = nextToken()) != nullptr ) 
			{
				if( *name == "}" )
					break;
				string *value;
				if( (value = nextToken()) == nullptr ) 
				{
					Exception::raise( "expected value" );
				}
# if TRACE
				cout << "protocol=" << *protocol << " name=" << *name << " value=" << *value << endl;
# endif // TRACE
				if( *name == "KEY" )
					keyPath = value;
				else if( *name == "CERTIFICATE" )
					certPath = value;
				else if( *name == "SESSION-COOKIE" )
				{
					sessionCookies->push_back( *value );
				}
				else if( *name == "TCP" || *name == "TLS" )
				{
					const char *destStr = value->c_str();
					bool secure = strcmp( value->c_str(), "TLS" ) == 0 ? true : false;
					sessionConfigs->push_back( new SessionConfig( destStr, secure ) );
				}
			}
# if TRACE
			cout << "PROTOCOL=" << *protocol << endl;
			cout << "LISTEN=" << *listenStr << endl;
			if( *protocol == "TLS" )
			{
				cout << "KEYPATH=" << *keyPath << endl;
				cout << "CERTPATH=" << *certPath << endl;
			}
# endif // TRACE
			return new ServiceConfig(
				*listenStr,
				keyPath == nullptr ? "" : *keyPath,
				certPath == nullptr ? "" : *certPath,
				sessionCookies,
				sessionConfigs
			);
		}

		vector<ServiceConfig *> serviceConfigs;	
		static L7LBConfig *config;

	private:

		ifstream *configFile;
		string line = ""; 
		string token = "";
		stringstream *ss = nullptr;
};

