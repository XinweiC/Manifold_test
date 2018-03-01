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

#include "ENERGYLIB_TSV.h"
#include <math.h>
#include <iostream>

using namespace EI;

ENERGYLIB_TSV::ENERGYLIB_TSV(string component_name, parameters_t *parameters, energy_introspector_t *ei)
{
  name = "3Dinterconnect";
  energy_introspector = ei;
  
  pseudo_module_t &pseudo_module = energy_introspector->module.find(component_name)->second;
  parameters_component_t &parameters_module = parameters->get_module(component_name);
  
  string option;

  set_variable(clock_frequency,parameters_module,"clock_frequency",0.0,true);
  set_variable(tsv.dielectric_constant,parameters_module,"dielectric_constant",8.85e-12);
  set_variable(tsv.relative_dielectric_constant,parameters_module,"relative_dielectric_constant",3.7);
  set_variable(tsv.tsv_height,parameters_module,"tsv_height",50e-6,true);
  set_variable(tsv.tsv_radius,parameters_module,"tsv_radius",5e-6,true);
  set_variable(tsv.sidewall_thickness,parameters_module,"sidewall_thickness",0.15e-6,true);
  set_variable(tsv.voltage,parameters_module,"voltage",1.0,true);
  set_variable(scaling,parameters_module,"scaling",1.0,false,false);
  set_variable(energy_scaling,parameters_module,"energy_scaling",1.0,false,false);
  set_variable(area_scaling,parameters_module,"area_scaling",1.0,false,false);

  double data_width;
  set_variable(data_width,parameters_module,"data_width",0.0,true,false);
  
  area_scaling *= data_width;
  energy_scaling *= data_width;
}

energy_t ENERGYLIB_TSV::get_unit_energy(void)
{
  energy_t unit_energy;
  
  unit_energy.read = tsv.compute_capacitance()*pow(tsv.voltage,2.0);
  unit_energy.write = unit_energy.read;
  
  /* TODO */
  //unit_energy.leakage;
  
  unit_energy.read *= energy_scaling;
  unit_energy.write *= energy_scaling;
  unit_energy.leakage *= (energy_scaling*scaling);
  
  return unit_energy;
}

power_t ENERGYLIB_TSV::get_tdp_power(void)
{

}

power_t ENERGYLIB_TSV::get_runtime_power(double time_tick, double period, counters_t counters)
{


}

double ENERGYLIB_TSV::get_area(void)
{
  return tsv.compute_area()*scaling*area_scaling;
}

void ENERGYLIB_TSV::update_variable(string variable, void *value)
{

}
