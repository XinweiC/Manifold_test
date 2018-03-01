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

#ifndef EI_LIB_H
#define EI_LIB_H

#include "energy_introspector.h"

namespace EI {

  class energy_library_t 
  {
  public:
    virtual ~energy_library_t() {}
    
    string name; // modeling tool name
    
    double clock_frequency; // clock frequency for power-energy conversion
    virtual energy_t get_unit_energy() = 0; // returns per-access energy
    virtual power_t get_tdp_power(void) = 0;
    virtual power_t get_runtime_power(double time_tick, double period, counters_t counters) = 0;
    virtual double get_leakage_power(double time_tick, double period) = 0;
    virtual double get_area(void) = 0; // returns area (if available)
    virtual void update_variable(string variable, void *value) = 0; // update per-access energy w.r.t the updater
    
  protected:
    class energy_introspector_t *energy_introspector; // back pointer to energy introspector
  };
  
  
  // parent class of thermal library (e.g., HotSpot, ICE)
  class thermal_library_t
  {
  public:
    virtual ~thermal_library_t() {}
    
    string name; // modeling tool name
    
    double clock_frequency;
    
    //virtual void set_partition(parameters_partition_t p_partition) = 0; // partition the processor-layer of the chip
    virtual void compute_temperature(double current_time, double period) = 0; // compute temperature
    virtual grid_t<double> get_thermal_grid(void) = 0; // returns 3D thermal grid map
    virtual double get_partition_temperature(string partition_ID, int type) = 0; // returns partition temperature
    virtual double get_point_temperature(double x, double y, int layer) = 0; // returns point temperature
    virtual int get_partition_layer_index(string partition_ID) = 0;
    virtual void deliver_power(string partition_ID, power_t partition_power) = 0; // update partition power
    
  protected:
    class energy_introspector_t *energy_introspector; // back pointer to energy introspector
  };
  
  
  // TBD
  class sensor_library_t
  {
  public:
    virtual ~sensor_library_t() {}
    
    string name; // modeling tool name
    
    double clock_frequency;
    
    virtual void read_data(double time_tick, void *data) = 0; // read and store data in the sensor queue
    
  protected:
    class energy_introspector_t *energy_introspector; // back pointer to energy introspector
  };
  
  
  // TBD
  class reliability_library_t
  {
  public:
    virtual ~reliability_library_t() {}
    
    string name; // modeling tool name
    
    double clock_frequency;
    
    virtual long double get_failure_probability(double time_tick, double period, double temperature, dimension_t dimension, double clock_frequency, double voltage, bool power_gating) = 0;
    virtual double get_threshold_voltage(double time_tick, double period, double temperature, double clock_frequency, double voltage, bool power_gating) = 0;
    
  protected:
    class energy_introspector_t *energy_introspector; // back pointer to energy introspector
  };
  
} // namespace EI

#endif
