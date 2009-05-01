#include "interpreter_element.h"

#include <glibmm/ustring.h>

namespace Interpreter
{

class Libraries : public Element
{
	static Glib::ustring defaultLibraryPath[] = { Glib::ustring("/lib/libc.so" )};
	public:
	Libraries( Element * parent = 0 ) : Element( parent )
	 { }

	void addLine( Glib::ustring raw_input )
	{
		if( raw_input[0] == '#' ) 
		{
			generateError( "Interperter::Bulitins: doesn't handle preprocessor macros");
		}
	}

};
}
