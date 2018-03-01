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

#ifndef THERMALLIB_HOTSPOT_H
#define THERMALLIB_HOTSPOT_H
#include "energy_introspector.h"

extern "C" {
#include "THERMALLIB_HOTSPOT/HotSpot-5.02/flp.h"
#include "THERMALLIB_HOTSPOT/HotSpot-5.02/npe.h"
#include "THERMALLIB_HOTSPOT/HotSpot-5.02/package.h"
#include "THERMALLIB_HOTSPOT/HotSpot-5.02/shape.h"
#include "THERMALLIB_HOTSPOT/HotSpot-5.02/temperature.h"
#include "THERMALLIB_HOTSPOT/HotSpot-5.02/temperature_block.h"
#include "THERMALLIB_HOTSPOT/HotSpot-5.02/temperature_grid.h"
#include "THERMALLIB_HOTSPOT/HotSpot-5.02/util.h"
}

namespace EI {

  class THERMALLIB_HOTSPOT : public thermal_library_t
  {
  public:
    THERMALLIB_HOTSPOT(string component_name, parameters_t *parameters, energy_introspector_t *ei);
    ~THERMALLIB_HOTSPOT() {}
    
    virtual void compute_temperature(double time_tick, double period);
    virtual grid_t<double> get_thermal_grid(void);
    virtual double get_partition_temperature(string partition_name);
    virtual double get_point_temperature(double x, double y, int layer);
    virtual int get_partition_layer_index(string partition_name);
    virtual void deliver_power(string partition_name, power_t partition_power);
    
  private:
    thermal_config_t thermal_config;	// thermal configuration
    package_config_t package_config;	// package configuration (if used)
    convection_t convection;		// airflow parameters
    RC_model_t *model;			// RC model
    flp_t *flp;				// floorplan
    
    double *temperature, *power;

    int thermal_analysis;
    enum HOTSPOT_THERMAL_ANALYSIS
    {
      TRANSIENT_ANALYSIS,
      STEADY_STATE_ANALYSIS,
      NUM_THERMAL_ANALYSES,
      NO_THERMAL_ANALYSIS      
    };
  };
  
} // namespace EI
  
#endif

