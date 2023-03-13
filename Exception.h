//
//  Exception.h
//
//  Created by Rick Tyler
//

# ifndef _Exception_h_
# define _Exception_h_

class Exception
{
	public:

		static void raise( const char *fmt, ... )
		{
			va_list args;
			va_start( args, fmt );
			static char buf[ 8192 ];
			vsprintf( buf, fmt, args );
			va_end( args );
			throw( (const char *) buf );
		}
};

# endif // _Exception_h_
