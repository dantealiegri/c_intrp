#include "input.h"
#include "interpreter_root.h"

Interpreter::Root * input_root = NULL;
Interpreter::Element * current_frame = NULL;
void read_input()
{
	g_printf("Read Input\n");
  if( current_frame == NULL )
  {
	  current_frame = input_root = new Interpreter::Root();
  }
}
