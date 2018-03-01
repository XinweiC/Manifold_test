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

#ifndef ENERGYLIB_INTSIM_H
#define ENERGYLIB_INTSIM_H

#include "energy_introspector.h"

#include "ENERGYLIB_INTSIM/intsim_alpha/intsim.h"
#include "ENERGYLIB_INTSIM/intsim_alpha/chip.h"
#include "ENERGYLIB_INTSIM/intsim_alpha/parameters.h"

namespace EI {

  class ENERGYLIB_INTSIM : public energy_library_t
  {
  public:
    ENERGYLIB_INTSIM(string component_name, parameters_t *parameters, energy_introspector_t *ei);
    ~ENERGYLIB_INTSIM() {}
    
    virtual energy_t get_unit_energy(void);
    virtual power_t get_tdp_power(void);
    virtual power_t get_runtime_power(double time_tick, double period, counters_t counters);
    virtual double get_area(void);
    virtual void update_variable(string variable, void *value);
    
  private:
    double energy_scaling;
    intsim_chip_t *chip;
    intsim_param_t *param;
    
    // IntSim main algorithm
    void IntSim(intsim_chip_t *chip, intsim_param_t *param);
    
    double tdp_activity_factor;
    
    int energy_model;  
    enum INTSIM_ENERGY_MODEL
    {
      ALL,
      LOGIC_GATE,
      REPEATER,
      INTERCONNECT,
      CLOCK,
      NUM_INTSIM_ENERGY_MODELS,
      NO_ENERGY_MODEL      
    };
  };
  
} // namespace EI

#endif
