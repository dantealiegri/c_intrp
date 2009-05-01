#include "interpreter_element.h"

namespace Interpeter;

class Libraries : public Element
{
	Glib::ustring defaultLibraryPath[] =
	{ "/lib/libc.so" };
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

}
