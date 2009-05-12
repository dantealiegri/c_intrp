#ifndef __NS_INTERPRETER_ROOT_H__
#define __NS_INTERPRETER_ROOT_H__
#include "interpreter_element.h"
#include "interpreter_libraries.h"
#include "debug_channels.h"

#include <glib/gprintf.h>

namespace Interpreter
{

class Root : public Element
{
	Libraries * libs;
	public:
	Root( Element * parent = 0 ) : Element( parent )
	{
		 //Debug::addChannel("root");
		 DebugChannel * init = Debug::addChannel("init.root");
		 DebugChannel * linput = Debug::addChannel("lineinput.root");

		 init->setEnabled( true );
		 linput->setEnabled( true );

		 libs = new Libraries( this );
		 g_printf( "Interpreter::Root: created.\n");
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
