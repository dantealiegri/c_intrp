#include "util.h"
#include <stdio.h>
#include <string.h>
const char * sprint_gchar_slist( GSList * l )
{
	static gchar _buf[1024];
	int i = 0;
	int ol=0;
	while( i <g_slist_length( l ))
	{
		gchar * str = (gchar*) g_slist_nth_data( l, i );
		snprintf( _buf + ol , 1024 - ol, "%s", str );
		ol += strlen( str );
		i++;
	}

  return _buf;
}
