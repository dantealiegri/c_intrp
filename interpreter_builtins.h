#include "interpreter_element.h"

namespace Interpreter
{

class Builtins : public Element
{
	public:
	Root( Element * parent = 0 ) : Element( parent )
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
