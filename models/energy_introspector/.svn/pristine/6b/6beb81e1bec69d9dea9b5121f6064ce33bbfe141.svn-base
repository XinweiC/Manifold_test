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

#include "ENERGYLIB_INTSIM.h"
#include <iostream>
#include <math.h>

using namespace EI;

ENERGYLIB_INTSIM::ENERGYLIB_INTSIM(string component_name, parameters_t *parameters, energy_introspector_t *ei) :
energy_model(NO_ENERGY_MODEL)
{
  // energy_library_t parameters
  name = "intsim";
  energy_introspector = ei;
  
  pseudo_module_t &pseudo_module = energy_introspector->module.find(component_name)->second;
  parameters_component_t &parameters_module = parameters->get_module(component_name);

  set_variable(energy_scaling,parameters_module,"energy_scaling",1.0,/*must_be_defined?*/false,/*search upward to partition and package?*/false);
  set_variable(clock_frequency,parameters_module,"clock_frequency",0.0,true);  

  chip = new intsim_chip_t();
  param = new intsim_param_t();
  
  memset(param,0,sizeof(intsim_param_t));

  param->f = clock_frequency;

  // these parameters must be defined by user
  set_variable(param->temp,parameters_module,"temperature",0.0,true);
  set_variable(param->F,parameters_module,"feature_size",0.0,true);
  set_variable(param->A,parameters_module,"area",0.0,true);
  set_variable(param->ngates,parameters_module,"gates",0.0,true);
  
  string energy_model_option;
  set_variable(energy_model_option,parameters_module,"energy_model","all");
  if(!stricmp(energy_model_option,"all"))
    energy_model = ALL;
  else if(!stricmp(energy_model_option,"logic_gate"))
    energy_model = LOGIC_GATE;
  else if(!stricmp(energy_model_option,"repeater"))
    energy_model = REPEATER;
  else if(!stricmp(energy_model_option,"interconnect"))
    energy_model = INTERCONNECT;
  else if(!stricmp(energy_model_option,"clock"))
    energy_model = CLOCK;
  else
    EI_ERROR("IntSim","unknown energy_model "+energy_model_option);
    

  // these parameters use default values if not explicitly defined
  set_variable(param->Vdd,parameters_module,"voltage",param->Vdd);
  set_variable(param->Vt,parameters_module,"threshold_voltage",param->Vt);
  set_variable(param->ncp,parameters_module,"critical_path_depth",param->ncp);
  set_variable(param->k,parameters_module,"rents_constant_k",param->k);
  set_variable(param->p,parameters_module,"rents_constant_p",param->p);
  set_variable(param->a,parameters_module,"activity_factor",param->a);
  set_variable(param->tdp_a,parameters_module,"tdp_activity_factor",param->a);
  set_variable(param->Vdd_spec,parameters_module,"voltage_spec",param->Vdd_spec);  
  set_variable(param->Vt_spec,parameters_module,"threshold_voltage_spec",param->Vt_spec);  
  set_variable(param->tox,parameters_module,"oxide_thickness",param->tox);  
  set_variable(param->alpha,parameters_module,"alpha_power_law",param->alpha);
  set_variable(param->drive_p_div_n,parameters_module,"pn_ratio",param->drive_p_div_n);
  set_variable(param->subvtslope_spec,parameters_module,"subvtslope_spec",param->subvtslope_spec);
  set_variable(param->er,parameters_module,"dielectric_permitivity",param->er);
  set_variable(param->rho,parameters_module,"copper_resistivity",param->rho);
  set_variable(param->ar,parameters_module,"wiring_aspect_ratio",param->ar);
  set_variable(param->R_coeff,parameters_module,"reflectivity_coefficient",param->R_coeff);
  set_variable(param->p_size,parameters_module,"specularity",param->p_size);
  set_variable(param->npower_pads,parameters_module,"power_pads",param->npower_pads);
  set_variable(param->pad_to_pad_distance,parameters_module,"power_pad_distance",param->pad_to_pad_distance);
  set_variable(param->pad_length,parameters_module,"power_pad_length",param->pad_length);
  set_variable(param->ir_drop_limit,parameters_module,"ir_drop_limit",param->ir_drop_limit);
  set_variable(param->router_eff,parameters_module,"router_efficiency",param->router_eff);
  set_variable(param->rep_eff,parameters_module,"repeater_efficiency",param->rep_eff);
  set_variable(param->fo,parameters_module,"average_fanouts",param->fo);
  set_variable(param->margin,parameters_module,"clock_margin",param->margin);
  set_variable(param->D,parameters_module,"Htree_max_span",param->D);
  set_variable(param->latches_per_buffer,parameters_module,"latches_per_buffer",param->latches_per_buffer);
  set_variable(param->clock_factor,parameters_module,"clock_factor",param->clock_factor);
  set_variable(param->clock_gating_factor,parameters_module,"clock_gating_factor",param->clock_gating_factor);  
  set_variable(param->max_tier,parameters_module,"max_tier",param->max_tier);

  param->setup(); // set default values based on the user-defined variables

  /*
  // debugging
  cout << parameters_module.name << endl;
  cout << "param->Vdd = " << param->Vdd << endl;
  cout << "param->Vt = " << param->Vt << endl;
  cout << "param->tox = " << param->tox << endl;
  cout << "param->drive_p_div_n = " << param->drive_p_div_n << endl;
  cout << "param->f = " << param->f << endl;
  cout << "param->F = " << param->F << endl;
  cout << "param->A = " << param->A << endl;
  cout << "param->ngates = " << param->ngates << endl;
  cout << "param->F1 = " << param->F1 << endl;
  cout << "param->Vdd_spec = " << param->Vdd_spec << endl;
  cout << "param->Vt_spec = " << param->Vt_spec << endl;
  cout << "param->rho = " << param->rho << endl;
  cout << "param->Ileak_spec = " << param->Ileak_spec << endl;
  cout << "param->Idsat_spec = " << param->Idsat_spec << endl;
  cout << "param->ncp = " << param->ncp << endl;
  cout << "param->er = " << param->er << endl;
  cout << "param->k = " << param->k << endl;
  cout << "param->p = " << param->p << endl;
  cout << "param->a = " << param->a << endl;
  cout << "param->alpha = " << param->alpha << endl;
  cout << "param->subvtslope_spec = " << param->subvtslope_spec << endl;
  cout << "param->s = " << param->s << endl;
  cout << "param->ar = " << param->ar << endl;
  cout << "param->p_size = " << param->p_size << endl;
  cout << "param->pad_to_pad_distance = " << param->pad_to_pad_distance << endl;
  cout << "param->pad_length = " << param->pad_length << endl;
  cout << "param->ir_drop_limit = " << param->ir_drop_limit << endl;
  cout << "param->router_eff = " << param->router_eff << endl;
  cout << "param->rep_eff = " << param->rep_eff << endl;
  cout << "param->fo = " << param->fo << endl;
  cout << "param->margin = " << param->margin << endl;
  cout << "param->D = " << param->D << endl;
  cout << "param->latches_per_buffer = " << param->latches_per_buffer << endl;
  cout << "param->clock_factor = " << param->clock_factor << endl;
  cout << "param->clock_gating_factor = " << param->clock_gating_factor << endl;
  cout << "param->kp = " << param->kp << endl;
  cout << "param->kc = " << param->kc << endl;
  cout << "param->beta_clock = " << param->beta_clock << endl;
  cout << "param->ew_ground_power = " << param->ew_power_ground << endl;
  cout << "param->kai = " << param->kai << endl;
  cout << "param->alpha_wire = " << param->alpha_wire << endl;
  cout << "param->ro = " << param->ro << endl;
  cout << "param->co = " << param->co << endl;
  cout << "param->H = " << param->H << endl;
  cout << "param->W = " << param->W << endl;
  cout << "param->T = " << param->T << endl;
  cout << "param->S = " << param->S << endl;
  cout << "param->cg = " << param->cg << endl;
  cout << "param->cm = " << param->cm << endl;
  cout << "param->ew = " << param->ew << endl;
  cout << "param->H_global = " << param->H_global << endl;
  cout << "param->W_global = " << param->W_global << endl;
  cout << "param->T_global = " << param->T_global << endl;
  cout << "param->S_global = " << param->S_global << endl;
  cout << "param->c_clock = " << param->c_clock << endl;
  cout << "param->max_tier = " << param->max_tier << endl;
  */
  intsim(chip,param,(char*)"CONFIG/INTSIM/intsim.out");
}

energy_t ENERGYLIB_INTSIM::get_unit_energy(void)
{
  energy_t unit_energy;

  switch(energy_model)
  {
    case LOGIC_GATE:
      unit_energy.read = (chip->dyn_power_logic_gates)/clock_frequency;
      unit_energy.write = unit_energy.read;
      unit_energy.leakage = (chip->leakage_power_logic_gates)/clock_frequency;
      break;
    case REPEATER:
      unit_energy.read = (chip->dyn_power_repeaters)/clock_frequency;
      unit_energy.write = unit_energy.read;      
      unit_energy.leakage = (chip->leakage_power_repeaters)/clock_frequency;
      break;
    case INTERCONNECT:
      unit_energy.read = (chip->power_wires)/clock_frequency;
      unit_energy.write = unit_energy.read;      
      break;
    case CLOCK:
      unit_energy.baseline = (chip->clock_power)/clock_frequency;
      break;
    default: // ALL
      unit_energy.baseline = (chip->clock_power)/clock_frequency;
      unit_energy.read = (chip->dyn_power_logic_gates+chip->dyn_power_repeaters+chip->power_wires)/clock_frequency; 
      unit_energy.write = unit_energy.read;
      unit_energy.leakage = (chip->leakage_power_logic_gates+chip->leakage_power_repeaters)/clock_frequency;
      break;
  }

  return unit_energy;
}

power_t ENERGYLIB_INTSIM::get_tdp_power(void)
{
  double activity_factor = param->a;
  param->a = param->tdp_a;
  chip->update_energy(param);
  energy_t unit_energy = get_unit_energy();
    
  param->a = activity_factor;
  chip->update_energy(param);
  
  // IntSim model is not architecturally analyzed,
  // so it doesn't differentiate read, write, etc.
  // mask the TDP power to read access only.
  power_t tdp_power = unit_energy*clock_frequency;
  tdp_power.write = 0.0;
  return tdp_power;
}

power_t ENERGYLIB_INTSIM::get_runtime_power(double time_tick, double period, counters_t counters)
{
  energy_t unit_energy = get_unit_energy();
  energy_t runtime_energy;
  
  runtime_energy.baseline = unit_energy.baseline*(clock_frequency*period);
  runtime_energy.read = unit_energy.read*counters.read;
  runtime_energy.write = unit_energy.write*counters.write;
  runtime_energy.leakage = unit_energy.leakage*(clock_frequency*period);
  power_t runtime_power = runtime_energy*(1.0/period);

  return runtime_power;
}

double ENERGYLIB_INTSIM::get_area(void)
{
  return param->A;
}

void ENERGYLIB_INTSIM::update_variable(string variable, void *value)
{
  if(!stricmp(name,"frequency"))
  {
    param->f = clock_frequency = *(double*)value;
  }
  else if(!stricmp(name,"temperature"))
  {
    param->temp = *(double*)value;
    param->Ileak_spec = param->I_off_n[(int)param->temp-300]*param->L;
  }
  else if(!stricmp(name,"voltage"))
  {
    param->Vdd = *(double*)value;
    param->Vdd_spec = param->Vdd;
  }
  else
  {
    fprintf(stdout,"EI WARNING (IntSim): updating undefined variable %s in update_energy()\n",name.c_str());
  }

  chip->update_energy(param);
}
