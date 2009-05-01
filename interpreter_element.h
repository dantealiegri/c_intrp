#ifndef __NS_INTERPRETER_ELEMENT_H__
#define __NS_INTERPRETER_ELEMENT_H__
#include <glibmm/ustring.h>
namespace Interpreter
{


class Element
{
	Element * m_parent;
	public:
	Element( Element * parent = 0 ) { m_parent = parent; }

	void addLine( Glib::ustring raw_input )
	{
		if( raw_input[0] == '#' ) 
		{
		}
	}
	void generateError( Glib::ustring )
	{
		// TODO
	}
};
}
#endif // __NS_INTERPRETER_ELEMENT_H__
