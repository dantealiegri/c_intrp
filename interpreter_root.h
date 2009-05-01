#include "interpreter_element.h"

namespace Interpeter;

class Root : public Element
{
	Libraries * libs;
	public:
	Root( Element * parent = 0 ) : Element( parent )
	{
		 libs = new Libraries( this );
	}

	void addLine( Glib::ustring raw_input )
	{
		if( raw_input[0] == '#' ) 
		{
		}
	}

	void registerPreprocessorMacro( Glib::ustring raw_ppmacro )
	{
	}
}
