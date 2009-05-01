#include "input.h"
#include "interpreter_root.h"

Interpreter::Root * input_root;
Interpreter::Element * current_frame;
void read_input()
{
  if( current_frame == NULL )
  {
	  current_frame = input_root = new Interpreter::Root();
  }
}
