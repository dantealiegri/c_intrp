#include <stdio.h>
#include <stdlib.h>
#include "input.h"


int parse_options( int argc, char ** argv );
int main( int argc, char ** argv )
{
	int parse_ret;
	if( (parse_ret = parse_options( argc, argv )) != 0 )
		exit( parse_ret );
	
	read_input();
}

int parse_options( int argc, char ** argv )
{
 // nooo options.
 return 0;
}
