#include <stdio.h>
#include <stdlib.h>

#include "input.h"

int parse_args( int argc , char ** argv );
int main( int argc , char ** argv )
{
	int pret;
	if( ( pret= parse_args( argc, argv )) != 0 ) exit( pret );

	read_input();

	exit( 0 );
}

int parse_args( int argc , char ** argv )
{
	return 0;
}
