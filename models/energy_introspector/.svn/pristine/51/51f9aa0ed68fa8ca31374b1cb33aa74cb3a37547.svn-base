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

#ifndef ENERGYLIB_TSV_H
#define ENERGYLIB_TSV_H

#include "energy_introspector.h"

#include "ENERGYLIB_TSV/tsv.h"

namespace EI {

  class ENERGYLIB_TSV : public energy_library_t
  {
  public:
    ENERGYLIB_TSV() : energy_scaling(1.0), area_scaling(1.0), scaling(1.0) {}
    ENERGYLIB_TSV(string component_name, parameters_t *parameters,energy_introspector_t *ei);
    
    virtual energy_t get_unit_energy(void);
    virtual power_t get_tdp_power(void);
    virtual power_t get_runtime_power(double time_tick, double period, counters_t counters);
    virtual double get_area(void);
    virtual void update_variable(string variable, void *value);
    
  private:
    string energy_model;
    string energy_submodel;
    
    double energy_scaling;
    double area_scaling;
    double scaling;
    
    tsv_t tsv;
  };

} // namespace EI
  
#endif
