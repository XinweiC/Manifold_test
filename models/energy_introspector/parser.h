/*
   The Energy Introspector (EI) is the simulation interface and does not possess 
   the copyrights of the following tools: 
      IntSim: Microelectronics Research Center, Georgia Tech
      DSENT: Computer Science and Artificial Intelligence Lab, MIT
      McPAT: Hewlett-Packard Labs
      3D-ICE: Embedded Systems Lab, EPFL
      HotSpot: Laboratory for Computer Architecture at Virginia, Univ. of Virginia
  
   The EI does not guarantee the exact same functionality, performance, or result
   of the above tools distributed by the original developers.
  
   The Use, modification, and distribution of the EI is subject to the policy of
   the Semiconductor Research Corporation (Task 2084.001).
  
   Copyright 2012
   William Song and Sudhakar Yalamanchili
   Georgia Institute of Technology, Atlanta, GA 30332
*/

#ifndef EI_PARSE_H
#define EI_PARSE_H

#include "parameters.h"

namespace EI {

  class parser_t
  {
  public:
    void parse(char *filename, class parameters_t *parameters);
    
  private:
    parameters_component_t package;
    parameters_component_t partition;
    parameters_component_t module;
    parameters_component_t sensor;
  };
  
} // namespace EI

#endif
