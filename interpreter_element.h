#include <glibmm/ustring.h>
namespace Interpeter;


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
}
