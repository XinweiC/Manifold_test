#include <iostream>
#include <assert.h>

#include "../energy_introspector.h"

using namespace EI;

int main(void)
{
  energy_introspector_t *energy_introspector = new energy_introspector_t((char*)"CONFIG/XGC/Atom.config");
  
  return 0;
}
