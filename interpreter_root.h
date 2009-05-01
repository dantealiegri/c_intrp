#ifndef __NS_INTERPRETER_ROOT_H__
#define __NS_INTERPRETER_ROOT_H__
#include "interpreter_element.h"
#include "interpreter_libraries.h"

namespace Interpreter
{

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
};
}
#endif // __NS_INTERPRETER_ROOT_H__
