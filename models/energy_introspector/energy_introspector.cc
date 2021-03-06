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

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <math.h>

#include "include.h"
#ifdef ENERGYLIB_INTSIM_I
#include "ENERGYLIB_INTSIM.h"
#endif
#ifdef ENERGYLIB_DSENT_I
#include "ENERGYLIB_DSENT.h"
#endif
#ifdef ENERGYLIB_MCPAT_I
#include "ENERGYLIB_MCPAT.h"
#endif
#ifdef ENERGYLIB_TSV_I
#include "ENERGYLIB_TSV.h"
#endif
#ifdef RELIABILITYLIB_FAILURE_I
#include "RELIABILITYLIB_FAILURE.h"
#endif
#ifdef SENSORLIB_RANDOM_I
#include "SENSORLIB_RANDOM.h"
#endif
#ifdef THERMALLIB_3DICE_I
#include "THERMALLIB_3DICE.h"
#endif
#ifdef THERMALLIB_HOTSPOT_I
#include "THERMALLIB_HOTSPOT.h"
#endif
#include "energy_introspector.h"

using namespace EI;

energy_introspector_t::energy_introspector_t(void) :
num_components(0),
file_config(NULL)
{
}

energy_introspector_t::energy_introspector_t(char *input_config) :
num_components(0),
file_config(NULL)
{
  configure(input_config,NULL);
}

energy_introspector_t::energy_introspector_t(char *input_config, char *output_config) :
num_components(0),
file_config(NULL)
{
  configure(input_config,output_config);
}


energy_introspector_t::~energy_introspector_t()
{
  package.clear();
  partition.clear();
  module.clear();
  sensor.clear();
}


void energy_introspector_t::configure(char *input_config, char *output_config)
{
  // create input parameters
  parameters_t *parameters = new parameters_t();

  if(!parameters)
    EI_ERROR("EI","cannot configure energy_introspector parameters");

  // parse the configuration file
  parameters->parse(input_config);
  
  file_config = fopen(output_config?output_config:"CONFIG/config.out","w");
  
  if(!file_config)
  {
    EI_WARNING("EI","cannot open configuration log file CONFIG/config.out\n");
    file_config = stdout;
  }

  EI_DISPLAY("\n### configuring Energy Introspector ###");

  // configure the EI
  pre_config(parameters);
  post_config(parameters);
  
  // check if there are unused options
  parameters->check_option(file_config);

  fprintf(file_config,"### CONFIGURATION SUMMARY ###\n");
  for(map<string,pseudo_package_t>::iterator package_it = package.begin(); 
      package_it != package.end(); package_it++)
  {
    fprintf(file_config,"package %s\n",package_it->first.c_str());
    fprintf(file_config,"  thermal library: ");
    if(package_it->second.thermal_library)
      fprintf(file_config,"%s\n",package_it->second.thermal_library->name.c_str());
    else
      fprintf(file_config,"n/a\n");
    fprintf(file_config,"  partition: ");
    if(package_it->second.partition.size() > 0)
    {
      fprintf(file_config,"\n");
      for(vector<string>::iterator partition_str_it = package_it->second.partition.begin();\
          partition_str_it < package_it->second.partition.end(); partition_str_it++)
        fprintf(file_config,"    %s\n",partition_str_it->c_str());
      power_t TDP_power = package_it->second.queue.pull<power_t>(0.0,"TDP");
      char print_message[128];
      if(TDP_power.baseline > 0.0)
        fprintf(file_config,"  TDP.baseline = %3.3lfW\n",TDP_power.baseline);
      if(TDP_power.read > 0.0)
        fprintf(file_config,"  TDP.read = %3.3lfW\n",TDP_power.read);
      if(TDP_power.write > 0.0)
        fprintf(file_config,"  TDP.write = %3.3lfW\n",TDP_power.write);
      if(TDP_power.search > 0.0)
        fprintf(file_config,"  TDP.search = %3.3lfW\n",TDP_power.search);
      if(TDP_power.read_tag > 0.0)
        fprintf(file_config,"  TDP.read_tag = %3.3lfW\n",TDP_power.read_tag);
      if(TDP_power.write_tag > 0.0)
        fprintf(file_config,"  TDP.write_tag = %3.3lfW\n",TDP_power.write_tag);
      if(TDP_power.get_total() > 0.0)
        fprintf(file_config,"  TDP.total = %3.3lfW (dynamic = %3.3lfW, leakage = %3.3lfW)\n",TDP_power.get_total(),TDP_power.get_total()-TDP_power.leakage,TDP_power.leakage);
    }
    else
      fprintf(file_config,"n/a\n");
  }
  
  for(map<string,pseudo_partition_t>::iterator partition_it = partition.begin();
      partition_it != partition.end(); partition_it++)
  {
    fprintf(file_config,"partition %s\n",partition_it->first.c_str());
    dimension_t dimension = pull_data<dimension_t>(0.0,"partition",partition_it->first,"dimension");
    fprintf(file_config,"  (computed) area = %6.6lfmm^2 (%3.3lfmm x %3.3lfmm) # if area != (width*length), the area is estimated from the energy library\n",dimension.get_area()*1e6,dimension.width*1e3,dimension.length*1e3);
    fprintf(file_config,"  coordinate: x_left = %3.3lfmm, y_bottom = %3.3lfmm, layer = %d\n",dimension.x_left*1e3,dimension.y_bottom*1e3,dimension.layer);
    fprintf(file_config,"  reliability_library: ");
    if(partition_it->second.reliability_library)
      fprintf(file_config,"%s\n",partition_it->second.reliability_library->name.c_str());
    else
      fprintf(file_config,"n/a\n");
    fprintf(file_config,"  package: %s\n",partition_it->second.package.c_str());
    fprintf(file_config,"  modules: ");
    if(partition_it->second.module.size() > 0)
    {
      fprintf(file_config,"\n");
      for(vector<string>::iterator module_str_it = partition_it->second.module.begin();
          module_str_it < partition_it->second.module.end(); module_str_it++)
        fprintf(file_config,"    %s\n",module_str_it->c_str());

      power_t TDP_power = partition_it->second.queue.pull<power_t>(0.0,"TDP");
      if(TDP_power.baseline > 0.0)
        fprintf(file_config,"  TDP.baseline = %3.3lfW\n",TDP_power.baseline);
      if(TDP_power.read > 0.0)
        fprintf(file_config,"  TDP.read = %3.3lfW\n",TDP_power.read);
      if(TDP_power.write > 0.0)
        fprintf(file_config,"  TDP.write = %3.3lfW\n",TDP_power.write);
      if(TDP_power.search > 0.0)
        fprintf(file_config,"  TDP.search = %3.3lfW\n",TDP_power.search);
      if(TDP_power.read_tag > 0.0)
        fprintf(file_config,"  TDP.read_tag = %3.3lfW\n",TDP_power.read_tag);
      if(TDP_power.write_tag > 0.0)
        fprintf(file_config,"  TDP.write_tag = %3.3lfW\n",TDP_power.write_tag);
      if(TDP_power.get_total() > 0.0)
        fprintf(file_config,"  TDP.total = %3.3lfW (dynamic = %3.3lfW, leakage = %3.3lfW)\n",TDP_power.total,TDP_power.total-TDP_power.leakage,TDP_power.leakage);
    }
    else
      fprintf(file_config,"n/a\n");
  }
  for(map<string,pseudo_module_t>::iterator module_it = module.begin();
      module_it != module.end(); module_it++)
  {
    fprintf(file_config,"module %s\n",module_it->first.c_str());
    fprintf(file_config,"  area = %6.6lfmm^2\n",pull_data<dimension_t>(0.0,"module",module_it->first,"dimension").get_area()*1e6);
    fprintf(file_config,"  energy_library: ");
    if(module_it->second.energy_library)
    {
      fprintf(file_config,"%s\n",module_it->second.energy_library->name.c_str());
      energy_t unit_energy = module_it->second.energy_library->get_unit_energy();
      power_t TDP_power = module_it->second.queue.pull<power_t>(0.0,"TDP");
      
      if(unit_energy.baseline > 0.0)
      {
        fprintf(file_config,"  unit_energy.baseline = %6.3lfpJ",unit_energy.baseline*1e12);
        if(TDP_power.baseline > 0.0)
          fprintf(file_config," (TDP = %6.3lfmW)",TDP_power.baseline*1e3);
        fprintf(file_config,"\n");
      }
      
      fprintf(file_config,"  unit_energy.read = %6.3lfpJ",unit_energy.read*1e12);
      if(TDP_power.read > 0.0)
        fprintf(file_config," (TDP = %6.3lfmW)",TDP_power.read*1e3);
      fprintf(file_config,"\n");
      fprintf(file_config,"  unit_energy.write = %6.3lfpJ",unit_energy.write*1e12);
      if(TDP_power.write > 0.0)
        fprintf(file_config," (TDP = %6.3lfmW)",TDP_power.write*1e3);
      fprintf(file_config,"\n");
      if(unit_energy.search > 0.0)
      {
        fprintf(file_config,"  unit_energy.search = %6.3lfpJ",unit_energy.search*1e12);
        if(TDP_power.search > 0.0)
          fprintf(file_config," (TDP = %6.3lfmW)",TDP_power.search*1e3);
        fprintf(file_config,"\n");
      }
      if((unit_energy.read_tag > 0.0)||(unit_energy.write_tag > 0.0))
      {
        fprintf(file_config,"  unit_energy.read_tag = %6.3lfpJ",unit_energy.read_tag*1e12);
        if(TDP_power.read_tag > 0.0)
          fprintf(file_config," (TDP = %6.3lfmW)",TDP_power.read_tag*1e3);
        fprintf(file_config,"\n");
        fprintf(file_config,"  unit_energy.write_tag = %6.3lfpJ",unit_energy.write_tag*1e12);
        if(TDP_power.write_tag > 0.0)
          fprintf(file_config," (TDP = %6.3lfmW)",TDP_power.write_tag*1e3);
        fprintf(file_config,"\n");
      }
      fprintf(file_config,"  unit_energy.leakage = %6.3lfpJ (%3.6lfmW)",unit_energy.leakage*1e12,unit_energy.leakage*module_it->second.energy_library->clock_frequency*1e3);
      if(stricmp(module_it->second.partition,"n/a"))
        fprintf(file_config," at %3.2lfK\n",pull_data<double>(0.0,"partition",module_it->second.partition,"temperature"));
      else
        fprintf(file_config,"\n");
      if(TDP_power.get_total() > 0.0)
        fprintf(file_config,"  TDP total = %6.3lfmW (dynamic = %6.3lfmW, leakage = %6.3lfmW)\n",TDP_power.total*1e3,(TDP_power.total-TDP_power.leakage)*1e3,TDP_power.leakage*1e3);
    }
    else
      fprintf(file_config,"n/a\n");
    fprintf(file_config,"  partition: %s\n",module_it->second.partition.c_str());
  }
  for(map<string,pseudo_sensor_t>::iterator sensor_it = sensor.begin();
      sensor_it != sensor.end(); sensor_it++)
  {
    fprintf(file_config,"sensor %s\n",sensor_it->first.c_str());
    fprintf(file_config,"  sensor_library: ");
    if(sensor_it->second.sensor_library)
    {
      fprintf(file_config,"%s\n",sensor_it->second.sensor_library->name.c_str());
      fprintf(file_config,"  sensing: %s(%s).%s\n",sensor_it->second.component_type.c_str(),sensor_it->second.component_name.c_str(),sensor_it->second.component_data.c_str());
    }
    else
      fprintf(file_config,"n/a\n");
  }

  EI_DISPLAY("\n### Energy Introspector configured ###");

  if(file_config)
  {
    EI_DISPLAY("### configuration log is available in CONFIG/config.out ###");
    fclose(file_config);
  }
}


// pre_config() creates empty pseudo components
void energy_introspector_t::pre_config(parameters_t *parameters)
{
  EI_DISPLAY("\n### pre_config process ###");

  // create empty pseudo packages
  for(vector<parameters_component_t>::iterator p_package_it = parameters->package.begin(); 
      p_package_it < parameters->package.end(); p_package_it++)
  {
    EI_DISPLAY("creating pseudo package "+p_package_it->name);

    pseudo_package_t pseudo_package;

    string queue_option = p_package_it->get_option("queue",true);
    while(stricmp(queue_option,"n/a"))
    {
      pseudo_package.queue.create(/*(string)name:type:size*/queue_option,/*skip?*/false);
      queue_option = p_package_it->get_option("queue",true);
    }

    // default queues - these queue won't be created if already manually created above
    pseudo_package.queue.create<dimension_t>("dimension",1,/*skip?*/true);
    pseudo_package.queue.create<power_t>("TDP",1,true);
    pseudo_package.queue.create<power_t>("power",1,true);
    pseudo_package.queue.create<grid_t<double> >("thermal_grid",1,true);

    // connect to partitions
    string partition_option = p_package_it->get_option("partition",true);
    while(stricmp(partition_option,"n/a"))
    {
      pseudo_package.partition.push_back(partition_option);
      partition_option = p_package_it->get_option("partition",true);
    }

    set_variable(pseudo_package.ID,*p_package_it,"ID",++num_components,false,false);

    // store the pseudo package in the EI
    if(package.find(p_package_it->name) == package.end())
      package.insert(pair<string,pseudo_package_t>(p_package_it->name,pseudo_package));
    else
      EI_ERROR("EI","duplicated pseudo package name "+p_package_it->name);
  }

  // create emtpy pseudo partitions and link to pseudo packages
  for(vector<parameters_component_t>::iterator p_partition_it = parameters->partition.begin();
      p_partition_it < parameters->partition.end(); p_partition_it++)
  {
    EI_DISPLAY("creating pseudo partition "+p_partition_it->name);

    pseudo_partition_t pseudo_partition;

    // link to a pseudo package
    string package_name;
    set_variable(package_name,*p_partition_it,"package","n/a",false,false);

    if(stricmp(package_name,"n/a"))
    {
      map<string,pseudo_package_t>::iterator package_it = package.find(package_name);
      if(package_it == package.end())
        EI_ERROR("EI","pseudo partition "+p_partition_it->name+" cannot be linked to non-existing pseudo package "+package_name);

      // connect to the package
      vector<string>::iterator partition_str_it;
      for(partition_str_it = package_it->second.partition.begin();
          partition_str_it < package_it->second.partition.end(); partition_str_it++)
        if(*partition_str_it == p_partition_it->name) // already linked
          break;
      if(partition_str_it == package_it->second.partition.end())
        package_it->second.partition.push_back(p_partition_it->name);
    }

    pseudo_partition.package = package_name;
    
    string first_option, current_option;

    // create data queues
    first_option = p_partition_it->get_option("queue");
    current_option = first_option;
    while(stricmp(current_option,"n/a"))
    {
      pseudo_partition.queue.create(/*(string)name:type:size*/current_option,/*skip?*/false);
      current_option = p_partition_it->get_option("queue");
      if(!stricmp(first_option,current_option))
        break;
    }

    set_variable(pseudo_partition.ID,*p_partition_it,"ID",++num_components,false,false);
    
    // default queues - these queue won't be created if already manually created above
    pseudo_partition.queue.create<dimension_t>("dimension",1,/*skip?*/true);
    pseudo_partition.queue.create<power_t>("power",1,true);
    pseudo_partition.queue.create<power_t>("TDP",1,true);
    pseudo_partition.queue.create<double>("temperature",1,true);
    pseudo_partition.queue.create<long double>("failure_probability",1,true);
    
    // connect to modules
    first_option = p_partition_it->get_option("module");
    current_option = first_option;
    while(stricmp(current_option,"n/a"))
    {
      pseudo_partition.module.push_back(current_option);
      current_option = p_partition_it->get_option("module");
      if(!stricmp(first_option,current_option))
        break;
    }

    // store the pseudo partition in the EI
    if(partition.find(p_partition_it->name) == partition.end())
    {
      partition.insert(pair<string,pseudo_partition_t>(p_partition_it->name,pseudo_partition));
    }
    else
      EI_ERROR("EI","duplicated pseudo partition name "+p_partition_it->name);
  }
  
  // check if pseudo package includes correct pseudo partitions, and the links are matching
  for(map<string,pseudo_package_t>::iterator package_it = package.begin();\
      package_it != package.end(); package_it++)
  {
    parameters_component_t &parameters_package = parameters->get_package(package_it->first);

    for(vector<string>::iterator partition_str_it = package_it->second.partition.begin();
        partition_str_it < package_it->second.partition.end(); partition_str_it++)
    {
      map<string,pseudo_partition_t>::iterator partition_it = partition.find(*partition_str_it);
      if(partition_it == partition.end())
        EI_ERROR("EI","pseudo package "+package_it->first+" includes non-existing pseudo partition "+(*partition_str_it));
      else
      {
        parameters_component_t &parameters_partition = parameters->get_partition(partition_it->first);
        parameters_partition.upper_level = &parameters_package;

        if(partition_it->second.package != package_it->first)
          if(stricmp(partition_it->second.package,"n/a"))
            EI_ERROR("EI","pseudo package"+package_it->first+" and pseudo partition "+partition_it->first+" have conflicting link");

        partition_it->second.package = package_it->first;
      }
    }
  }
  
  // create emtpy pseudo modules and link to pseudo partitions
  for(vector<parameters_component_t>::iterator p_module_it = parameters->module.begin();
      p_module_it < parameters->module.end(); p_module_it++)
  {
    EI_DISPLAY("creating pseudo module "+p_module_it->name);

    pseudo_module_t pseudo_module;

    // link to a pseudo partition
    string partition_name;
    set_variable(partition_name,*p_module_it,"partition","n/a",false,false);

    if(stricmp(partition_name,"n/a"))
    {
      map<string,pseudo_partition_t>::iterator partition_it = partition.find(partition_name);
      if(partition_it == partition.end())
        EI_ERROR("EI","pseudo module "+p_module_it->name+" cannot be linked to non-existing pseudo partition "+partition_name);

      // connecto to the partition
      vector<string>::iterator module_str_it;
      for(module_str_it = partition_it->second.module.begin();
          module_str_it < partition_it->second.module.end(); module_str_it++)
        if(*module_str_it == p_module_it->name) // already linked
          break;
      if(module_str_it == partition_it->second.module.end())
        partition_it->second.module.push_back(p_module_it->name);
    }

    pseudo_module.partition = partition_name;
    
    string first_option, current_option;
    
    // create data queues
    first_option = p_module_it->get_option("queue");
    current_option = first_option;
    while(stricmp(current_option,"n/a"))
    {
      pseudo_module.queue.create(current_option,false);
      current_option = p_module_it->get_option("queue");
      if(!stricmp(first_option,current_option))
        break;
    }

    set_variable(pseudo_module.ID,*p_module_it,"ID",++num_components,false,false);
    
    // default queues - these queue won't be created if already manually created above
    pseudo_module.queue.create<dimension_t>("dimension",1,true);    
    pseudo_module.queue.create<power_t>("power",1,true);    
    pseudo_module.queue.create<power_t>("TDP",1,true);    
    
    // store the pseudo module in the EI
    if(module.find(p_module_it->name) == module.end())
      module.insert(pair<string,pseudo_module_t>(p_module_it->name,pseudo_module));
    else
      EI_ERROR("EI","duplicated pseudo module name "+p_module_it->name);
  }

  // cross check pseudo modules in the pseudo partitions are correctly linked
  for(map<string,pseudo_partition_t>::iterator partition_it = partition.begin();
      partition_it != partition.end(); partition_it++)
  {
    parameters_component_t &parameters_partition = parameters->get_partition(partition_it->first);

    for(vector<string>::iterator module_str_it = partition_it->second.module.begin();
        module_str_it < partition_it->second.module.end(); module_str_it++)
    {
      map<string,pseudo_module_t>::iterator module_it = module.find(*module_str_it);
      if(module_it == module.end())
        EI_ERROR("EI","pseudo partition "+partition_it->first+" includes non-existing pseudo module "+(*module_str_it));
      else
      {
        parameters_component_t &parameters_module = parameters->get_module(module_it->first);
        parameters_module.upper_level = &parameters_partition;

        if(module_it->second.partition != partition_it->first)
          if(stricmp(module_it->second.partition,"n/a"))
            EI_ERROR("EI","pseudo partition "+partition_it->first+" and pseudo module "+module_it->first+" have conflicting link");

        module_it->second.partition = partition_it->first;
      }
    }
  }
  
  // create empty pseudo sensors
  for(vector<parameters_component_t>::iterator p_sensor_it = parameters->sensor.begin();
      p_sensor_it < parameters->sensor.end(); p_sensor_it++)
  {
    EI_DISPLAY("creating pseudo sensor "+p_sensor_it->name);

    pseudo_sensor_t pseudo_sensor;

    set_variable(pseudo_sensor.ID,*p_sensor_it,"ID",++num_components,false,false);

    if(sensor.find(p_sensor_it->name) == sensor.end())
      sensor.insert(pair<string,pseudo_sensor_t>(p_sensor_it->name,pseudo_sensor));
    else
      EI_ERROR("EI","duplicated pseudo sensor name "+p_sensor_it->name);
  }
}


// post_config() initializes the pseudo components and links them to modeling libraries
void energy_introspector_t::post_config(parameters_t *parameters)
{
  EI_DISPLAY("\n### post_config process ###");

  // initialize pseudo modules
  for(vector<parameters_component_t>::iterator p_module_it = parameters->module.begin();\
      p_module_it < parameters->module.end(); p_module_it++)
  {
    EI_DISPLAY("initializing pseudo module "+p_module_it->name);

    pseudo_module_t &pseudo_module = module.find(p_module_it->name)->second;

    // link energy library
    string energy_library;
    set_variable(energy_library,*p_module_it,"energy_library","n/a",false,false);
    
    if(stricmp(energy_library,"n/a"))
    {
      if(!stricmp(energy_library,"none"))
        continue;
      #ifdef ENERGYLIB_INTSIM_I
      else if(!stricmp(energy_library,"intsim"))
        pseudo_module.energy_library = new ENERGYLIB_INTSIM(p_module_it->name,parameters,this);
      #endif
      #ifdef ENERGYLIB_DSENT_I
      else if(!stricmp(energy_library,"dsent"))
        pseudo_module.energy_library = new ENERGYLIB_DSENT(p_module_it->name,parameters,this);
      #endif
      #ifdef ENERGYLIB_MCPAT_I
      else if(!stricmp(energy_library,"mcpat"))
        pseudo_module.energy_library = new ENERGYLIB_MCPAT(p_module_it->name,parameters,this);
      #endif
      #ifdef ENERGYLIB_TSV_I
      else if(!stricmp(energy_library,"tsv"))
        pseudo_module.energy_library = new ENERGYLIB_TSV(p_module_it->name,parameters,this);
      #endif
      else
        EI_ERROR("EI","unknown energy library "+energy_library);
      
      // compute TDP power
      counters_t counters; // dummy counters
      compute_power(MAX_TIME,MAX_TIME,p_module_it->name,counters,/*is_TDP*/true);
      
      // compute module area
      dimension_t dimension;      
      dimension.area = pseudo_module.energy_library->get_area();
      pseudo_module.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);

      // update partition area
      map<string,pseudo_partition_t>::iterator partition_it = partition.find(pseudo_module.partition);
      if(partition_it != partition.end())
      {
        dimension_t partition_dimension = partition_it->second.queue.pull<dimension_t>(0.0,"dimension");
        partition_dimension.area += dimension.get_area();
        partition_it->second.queue.update<dimension_t>(MAX_TIME,MAX_TIME,"dimension",partition_dimension);
      }
    }
  }

  // initialize pseudo partitions
  for(vector<parameters_component_t>::iterator p_partition_it = parameters->partition.begin();\
      p_partition_it < parameters->partition.end(); p_partition_it++)
  {
    EI_DISPLAY("initializing pseudo partition "+p_partition_it->name);

    pseudo_partition_t &pseudo_partition = partition.find(p_partition_it->name)->second;

    double temperature;
    set_variable(temperature,*p_partition_it,"temperature",0.0);
    pseudo_partition.queue.push<double>(0.0,MAX_TIME,"temperature",temperature);
    
    // link reliability library
    string reliability_library;
    set_variable(reliability_library,*p_partition_it,"reliability_library","n/a",false,false);

    if(stricmp(reliability_library,"n/a"))
    {
      if(!stricmp(reliability_library,"none"))
        continue;
      #ifdef RELIABILITYLIB_FAILURE_I
      if(!stricmp(reliability_library,"failure"))
        pseudo_partition.reliability_library = new RELIABILITYLIB_FAILURE(p_partition_it->name,parameters,this);
      #endif
      else
        EI_ERROR("EI","unknown reliability library "+reliability_library);
    }

    // set location and size
    dimension_t dimension = pseudo_partition.queue.pull<dimension_t>(0.0,"dimension");

    set_variable(dimension.x_left,*p_partition_it,"x_left",0.0,false,false);
    set_variable(dimension.y_bottom,*p_partition_it,"y_bottom",0.0,false,false);
    set_variable(dimension.width,*p_partition_it,"width",0.0,false,false);
    set_variable(dimension.length,*p_partition_it,"length",0.0,false,false);
    set_variable(dimension.layer,*p_partition_it,"layer",0,false,false);

    pseudo_partition.queue.update<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);

    /* //update package area
    map<string,pseudo_package_t>::iterator package_it = package.find(pseudo_partition.package);
    if(package_it != package.end())
    {
      dimension_t package_dimension = package_it->second.queue.pull<dimension_t>(0.0,"dimension");
      package_dimension.area += dimension.get_area();
      package_it->second.queue.update<dimension_t>(MAX_TIME,MAX_TIME,"dimension",package_dimension);
    }
    */
  }
  
  // initialize pseudo packages
  for(vector<parameters_component_t>::iterator p_package_it = parameters->package.begin();\
      p_package_it < parameters->package.end(); p_package_it++)
  {
    EI_DISPLAY("initializing pseudo package "+p_package_it->name);
    
    pseudo_package_t &pseudo_package = package.find(p_package_it->name)->second;
    
    // link thermal and reliability library
    string thermal_library;
    set_variable(thermal_library,*p_package_it,"thermal_library","n/a",false,false);
    
    if(stricmp(thermal_library,"n/a"))
    {
      if(!stricmp(thermal_library,"none"))
        continue;
#if 0
      #ifdef THERMALLIB_HOTSPOT_I
      if(!stricmp(thermal_library,"hotspot"))
        pseudo_package.thermal_library = new THERMALLIB_HOTSPOT(p_package_it->name,parameters,this);
      #endif
      #ifdef THERMALLIB_3DICE_I
      else if(!stricmp(thermal_library,"3dice")||!stricmp(thermal_library,"3d-ice"))
        pseudo_package.thermal_library = new THERMALLIB_3DICE(p_package_it->name,parameters,this);
      #endif
#endif

        pseudo_package.thermal_library = new THERMALLIB_3DICE(p_package_it->name,parameters,this);


      if(pseudo_package.thermal_library)
      {
        for(vector<string>::iterator partition_str_it = pseudo_package.partition.begin();\
            partition_str_it != pseudo_package.partition.end(); partition_str_it++)
        {
          pseudo_partition_t &pseudo_partition = partition.find(*partition_str_it)->second;
          dimension_t partition_dimension = pseudo_partition.queue.pull<dimension_t>(0.0,"dimension");
          partition_dimension.layer = pseudo_package.thermal_library->get_partition_layer_index(*partition_str_it);
          pseudo_partition.queue.update<dimension_t>(MAX_TIME,MAX_TIME,"dimension",partition_dimension);
        }
      }
    }
  }

  // initialize pseudo sensors
  for(vector<parameters_component_t>::iterator p_sensor_it = parameters->sensor.begin();\
      p_sensor_it < parameters->sensor.end(); p_sensor_it++)
  {
    EI_DISPLAY("initializing pseudo sensor "+p_sensor_it->name);

    pseudo_sensor_t &pseudo_sensor = sensor.find(p_sensor_it->name)->second;

    // link sensor library
    string sensor_library;
    set_variable(sensor_library,*p_sensor_it,"sensor_library","n/a",false,false);

    if(stricmp(sensor_library,"n/a"))
    {
      if(!stricmp(sensor_library,"none"))
        continue;
      #ifdef SENSORLIB_RANDOM_I
      else if(!stricmp(sensor_library,"random"))
        pseudo_sensor.sensor_library = new SENSORLIB_RANDOM(p_sensor_it->name,parameters,this);
      #endif
      else
        EI_ERROR("EI","unknown sensor library "+sensor_library);
        
      // create sensor data queue
      pseudo_sensor.queue.create(p_sensor_it->get_option("queue"),false);
              
      // check if the pseudo sensor monitors valid data
      set_variable(pseudo_sensor.component_type,*p_sensor_it,"component_type","n/a",true,false);
      set_variable(pseudo_sensor.component_name,*p_sensor_it,"component_name","n/a",true,false);
      set_variable(pseudo_sensor.component_data,*p_sensor_it,"component_data","n/a",true,false);
      
      bool valid = true;
      if(!stricmp(pseudo_sensor.component_type,"package"))
      {
        map<string,pseudo_package_t>::iterator package_it = package.find(pseudo_sensor.component_name);
        if((package_it == package.end())||(!package_it->second.queue.is_queue(pseudo_sensor.component_data)))
          valid = false;
        else
          pseudo_sensor.index = package_it->second.queue.add_sensor(pseudo_sensor.component_data,pseudo_sensor);
      }
      else if(!stricmp(pseudo_sensor.component_type,"partition"))
      {
        map<string,pseudo_partition_t>::iterator partition_it = partition.find(pseudo_sensor.component_name);
        if((partition_it == partition.end())||(!partition_it->second.queue.is_queue(pseudo_sensor.component_data)))
          valid = false;
        else
          pseudo_sensor.index = partition_it->second.queue.add_sensor(pseudo_sensor.component_data,pseudo_sensor);
      }
      else if(!stricmp(pseudo_sensor.component_type,"module"))
      {
        map<string,pseudo_module_t>::iterator module_it = module.find(pseudo_sensor.component_name);
        if((module_it == module.end())||(!module_it->second.queue.is_queue(pseudo_sensor.component_data)))
          valid = false;
        else
          pseudo_sensor.index = module_it->second.queue.add_sensor(pseudo_sensor.component_data,pseudo_sensor);
      }
      else
        valid = false;
      
      if(!valid)
        EI_ERROR("EI","pseudo sensor "+p_sensor_it->name+" monitors invalid data");
    }
  }
}

// leakagee computation
void energy_introspector_t::compute_leakage\
(double time_tick, double period, string module_name)
{
  EI_DEBUG("computing pseudo module "+module_name+" power");

  // find pseudo module
  map<string,pseudo_module_t>::iterator module_it = module.find(module_name);

  if((module_it == module.end())||(period == 0.0))
  {
    EI_WARNING("EI","cannot compute power for pseudo module "+module_name+"; pseudo module is not found or sampling period is 0");
    return; // exit without energy computation
  }

  power_t old_power = pull_data<power_t>(time_tick,"module",module_it->first,"power");
  power_t new_power = pull_data<power_t>(time_tick,"module",module_it->first,"power");


  // compute energy
  if(module_it->second.energy_library != NULL)
  {
    new_power.leakage = module_it->second.energy_library->get_leakage_power(time_tick,period);

    #ifdef ENERGY_INTROSPECTOR_DEBUG
    char debug_message[256];
    sprintf(debug_message,"    time_tick = %lfsec",time_tick);
    EI_DEBUG(debug_message);
    if(power.leakage > 0.0)
    {
      sprintf(debug_message,"    power.leakage = %lfW",new_power.leakage);
      EI_DEBUG(debug_message);
    }
    #endif

    // store computed power in the stats queue
    module_it->second.queue.push<power_t>(time_tick,period,"power",new_power);

    // update partition power
    map<string,pseudo_partition_t>::iterator partition_it = partition.find(module_it->second.partition);

    if(partition_it != partition.end())
    {
      power_t partition_power = partition_it->second.queue.pull<power_t>(time_tick,"power");
      partition_power = partition_power + new_power - old_power;
    
      // update partition power
      if(partition_it->second.queue.is_synchronous<power_t>(time_tick,period,"power"))
        partition_it->second.queue.update<power_t>(time_tick,period,"power",partition_power);
      else
        EI_WARNING("EI","pseudo module "+module_it->first+" power is asynchronous to pseudo partition "+partition_it->first+" power; partition power is not updated");

      // update package power
      map<string,pseudo_package_t>::iterator package_it = package.find(partition_it->second.package);

      if(package_it != package.end())
      {
        power_t package_power = package_it->second.queue.pull<power_t>(time_tick,"power");
        package_power = package_power + new_power - old_power;

        if(package_it->second.queue.is_synchronous<power_t>(time_tick,period,"power"))
          package_it->second.queue.update<power_t>(time_tick,period,"power",package_power);
        else
          EI_WARNING("EI","pseudo module "+module_it->first+" power is asynchronous to pseudo package "+package_it->first+" power; package power is not updated");
      }
    }
  }
}



// energy computation
void energy_introspector_t::compute_power\
(double time_tick, double period, string module_name, counters_t counters, bool is_tdp)
{
  EI_DEBUG("computing pseudo module "+module_name+" power");

  // find pseudo module
  map<string,pseudo_module_t>::iterator module_it = module.find(module_name);

  if((module_it == module.end())||((period == 0.0)&&!is_tdp))
  {
    EI_WARNING("EI","cannot compute power for pseudo module "+module_name+"; pseudo module is not found or sampling period is 0");
    return; // exit without energy computation
  }

  power_t power;

  // compute energy
  if(module_it->second.energy_library != NULL)
  {
    if(is_tdp)
    {
      power = module_it->second.energy_library->get_tdp_power();
      
      // store the computed TDP in the queue
      module_it->second.queue.push<power_t>(MAX_TIME,MAX_TIME,"TDP",power);
      
      // update partition TDP
      map<string,pseudo_partition_t>::iterator partition_it = partition.find(module_it->second.partition);
      
      if(partition_it != partition.end())
      {
        power_t partition_power = partition_it->second.queue.pull<power_t>(0.0,"TDP");
        partition_power = partition_power + power;
        partition_it->second.queue.update<power_t>(MAX_TIME,MAX_TIME,"TDP",partition_power);
       
        map<string,pseudo_package_t>::iterator package_it = package.find(partition_it->second.package);
        
        if(package_it != package.end())
        {
          power_t package_power = package_it->second.queue.pull<power_t>(0.0,"TDP");
          package_power = package_power + power;
          package_it->second.queue.update<power_t>(MAX_TIME,MAX_TIME,"TDP",package_power);
        }
      }
    }
    else
    {
      power = module_it->second.energy_library->get_runtime_power(time_tick,period,counters);

      #ifdef ENERGY_INTROSPECTOR_DEBUG
      char debug_message[256];
      sprintf(debug_message,"    time_tick = %lfsec",time_tick);
      EI_DEBUG(debug_message);
      if(power.baseline > 0.0)
      {
        sprintf(debug_message,"    power.baseline = %lfW",power.baseline);
        EI_DEBUG(debug_message);
      }
      if(power.search > 0.0)
      {
        sprintf(debug_message,"    power.search = %lfW",power.search);
        EI_DEBUG(debug_message);
      }
      if(power.read > 0.0)
      {
        sprintf(debug_message,"    power.read = %lfW",power.read);
        EI_DEBUG(debug_message);
      }
      if(power.read_tag > 0.0)
      {
        sprintf(debug_message,"    power.read_tag = %lfW",power.read_tag);
        EI_DEBUG(debug_message);
      }
      if(power.write > 0.0)
      {
        sprintf(debug_message,"    power.write = %lfW",power.write);
        EI_DEBUG(debug_message);
      }
      if(power.write_tag > 0.0)
      {
        sprintf(debug_message,"    power.write_tag = %lfW",power.write_tag);
        EI_DEBUG(debug_message);
      }
      if(power.leakage > 0.0)
      {
        sprintf(debug_message,"    power.leakage = %lfW",power.leakage);
        EI_DEBUG(debug_message);
      }
      sprintf(debug_message,"    power.total = %lfW",power.get_total());
      EI_DEBUG(debug_message);
      #endif

      // store computed power in the stats queue
      module_it->second.queue.push<power_t>(time_tick,period,"power",power);

      // update partition power
      map<string,pseudo_partition_t>::iterator partition_it = partition.find(module_it->second.partition);

      if(partition_it != partition.end())
      {
        power_t partition_power = partition_it->second.queue.pull<power_t>(time_tick,"power");
        partition_power = partition_power + power;
    
        // update partition power
        if(partition_it->second.queue.is_synchronous<power_t>(time_tick,period,"power"))
          partition_it->second.queue.update<power_t>(time_tick,period,"power",partition_power);
        else
          EI_WARNING("EI","pseudo module "+module_it->first+" power is asynchronous to pseudo partition "+partition_it->first+" power; partition power is not updated");

        // update package power
        map<string,pseudo_package_t>::iterator package_it = package.find(partition_it->second.package);

        if(package_it != package.end())
        {
          power_t package_power = package_it->second.queue.pull<power_t>(time_tick,"power");
          package_power = package_power + power;

          if(package_it->second.queue.is_synchronous<power_t>(time_tick,period,"power"))
            package_it->second.queue.update<power_t>(time_tick,period,"power",package_power);
          else
            EI_WARNING("EI","pseudo module "+module_it->first+" power is asynchronous to pseudo package "+package_it->first+" power; package power is not updated");
        }
      }
    }
  }
}

#define MAX_TEMP 0
#define MIN_TEMP 1
#define AVG_TEMP 2
#define CTR_TEMP 2
double old_temperature = 0;
// temperature computation
void energy_introspector_t::compute_temperature(double time_tick, double period, string package_name)
{
  // find pseudo package
  map<string,pseudo_package_t>::iterator package_it = package.find(package_name);

  if((period == 0.0)||(package_it == package.end()))
  {
    EI_WARNING("EI","cannot compute temperature for pseudo package "+package_name+"; pseudo package is not found or sampling period = 0");
    return; // exit without temperature computation
  }

  // collect package and partition power, and check synchronization
  if(package_it->second.thermal_library != NULL)
  {
    power_t package_power;

    for(vector<string>::iterator partition_str_it = package_it->second.partition.begin();
        partition_str_it < package_it->second.partition.end(); partition_str_it++)
    {
      EI_DEBUG("collecting pseudo partition "+(*partition_str_it)+" power");

      bool synchronized = true;

      // find pseudo partition
      map<string,pseudo_partition_t>::iterator partition_it = partition.find(*partition_str_it);

      power_t partition_power = partition_it->second.queue.pull<power_t>(time_tick,"power");

      for(vector<string>::iterator module_str_it = partition_it->second.module.begin();
          module_str_it < partition_it->second.module.end(); module_str_it++)
      {
        map<string,pseudo_module_t>::iterator module_it = module.find(*module_str_it);

        // check module power numbers are synchronous
        if(!module_it->second.queue.is_synchronous<power_t>(time_tick,period,"power"))
        {
          synchronized = false;
          break;
        }

        // no power stats exists in the module
        if(time_tick > module_it->second.queue.end<power_t>("power")+max_trunc) // time precision = 1e-15sec
        {
          EI_WARNING("EI","no power was reported to pseudo module "+module_it->first+"; leakage power is used for this module");

          counters_t counters; // zero counters
          compute_power(time_tick,period,module_it->first,counters); // compute leakage power

          partition_power = partition_power + pull_data<power_t>(time_tick,"module",module_it->first,"power");
          partition_it->second.queue.update<power_t>(time_tick,period,"power",partition_power);
        }
      }

      if(!synchronized)
      {
        exit(1);
        // empty and erase pseudo partitions
        for(partition_str_it = package_it->second.partition.begin();
            partition_str_it < package_it->second.partition.end(); partition_str_it++)
        {
          partition_it = partition.find(*partition_str_it);
          partition_it->second.queue.reset();
          partition_it->second.module.clear();

          if(partition_it->second.reliability_library)
          {
            delete(partition_it->second.reliability_library);
            partition_it->second.reliability_library = NULL;
          }

          partition.erase(partition_it);
        }

        // delete thermal library
        if(package_it->second.thermal_library)
        {
          delete(package_it->second.thermal_library);
          package_it->second.thermal_library = NULL;
        }

        // empty and erase the pseudo package
        package_it->second.queue.reset();
        package_it->second.partition.clear();
        package.erase(package_it);

        EI_ERROR("EI","terminating thermal simulation for pseudo package "+(*partition_str_it)+" due to asynchronous power stats in the pseudo components hierarchy");
      }

      // deliver partition power to thermal model floorplan
      package_it->second.thermal_library->deliver_power(partition_it->first,partition_power);
    }

    // call the thermal library to compute temperature
    EI_DEBUG("computing pseudo package "+package_name+" temperature");
    package_it->second.thermal_library->compute_temperature(time_tick,period);

    // store thermal grid in the runtime data queue
    grid_t<double> thermal_grid = package_it->second.thermal_library->get_thermal_grid();
    package_it->second.queue.push<grid_t<double> >(time_tick,period,"thermal_grid",thermal_grid);

    for(vector<string>::iterator partition_str_it = package_it->second.partition.begin();\
        partition_str_it < package_it->second.partition.end(); partition_str_it++)
    {
      map<string,pseudo_partition_t>::iterator partition_it = partition.find(*partition_str_it);
      double old_temperature = partition_it->second.queue.pull<double>(time_tick-period,"temperature");
      double new_temperature1 = package_it->second.thermal_library->get_partition_temperature(partition_it->first, CTR_TEMP); //CTR_TEMP
      double new_temperature2 = package_it->second.thermal_library->get_partition_temperature(partition_it->first, MAX_TEMP); //MAX_TEMP
      double new_temperature =  (0.75 * new_temperature1 + 0.25 * new_temperature2);
      //partition_it->second.queue.push<double>(time_tick,period,"temperature",new_temperature);
      partition_it->second.queue.update<double>(time_tick,period,"temperature",new_temperature);

      //#ifdef ENERGY_INTROSPECTOR_DEBUG
      #if 1
      char debug_message[256];
      EI_DEBUG("    partition "+partition_it->first);
      sprintf(debug_message,"      time_tick = %lf",time_tick);
      EI_DEBUG(debug_message);
      sprintf(debug_message,"      temperature = %3.2lfK",new_temperature);
      EI_DEBUG(debug_message);
      sprintf(debug_message,"      B leakage_power = %lfK",partition_it->second.queue.pull<power_t>(time_tick,"power").leakage);
      EI_DEBUG(debug_message);

      //dimension_t dimension = partition_it->second.queue.pull<dimension_t>(0.0,"dimension");
      //sprintf(debug_message,"      power density = %lfW/mm^2",partition_it->second.queue.pull<power_t>(time_tick,"power").get_total()/(dimension.width*dimension.length*1e6));
      //EI_DEBUG(debug_message);
      //sprintf(debug_message,"      power = %lfW",partition_it->second.queue.pull<power_t>(time_tick,"power").get_total());
      //EI_DEBUG(debug_message);
      //sprintf(debug_message,"      area = %lfmm^2",(dimension.width*dimension.length*1e6));
      //EI_DEBUG(debug_message);
      #endif

      if((int)old_temperature != (int)new_temperature)
      {
        for(vector<string>::iterator module_str_it = partition_it->second.module.begin();\
            module_str_it < partition_it->second.module.end(); module_str_it++)
        {
          map<string,pseudo_module_t>::iterator module_it = module.find(*module_str_it);

      EI_DEBUG("    module "+partition_it->first);
      sprintf(debug_message,"      B leakage: = %lfW, dyn: = %lfW, total: = %lfW",module_it->second.queue.pull<power_t>(time_tick,"power").leakage, module_it->second.queue.pull<power_t>(time_tick,"power").get_total() - module_it->second.queue.pull<power_t>(time_tick,"power").leakage, module_it->second.queue.pull<power_t>(time_tick,"power").get_total());
      EI_DEBUG(debug_message);

      //fprintf(stderr,"      B leakage: = %lfW, dyn: = %lfW, total: = %lfW\n",module_it->second.queue.pull<power_t>(time_tick,"power").leakage, module_it->second.queue.pull<power_t>(time_tick,"power").get_total() - module_it->second.queue.pull<power_t>(time_tick,"power").leakage, module_it->second.queue.pull<power_t>(time_tick,"power").get_total());
 
        //  cerr << "module: " << module_it->first << " partition: " << partition_it->first << " updating T: " << old_temperature << " to "<< new_temperature << endl;

          //EI_DEBUG("updating pseudo module "+module_it->first+" unit energy (leakage feedback)");
          module_it->second.energy_library->update_variable("temperature",&new_temperature);

          compute_leakage(time_tick,period,module_it->first); // compute leakage power

      //fprintf(stderr,"      A leakage: = %lfW, dyn: = %lfW, total: = %lfW\n",module_it->second.queue.pull<power_t>(time_tick,"power").leakage, module_it->second.queue.pull<power_t>(time_tick,"power").get_total() - module_it->second.queue.pull<power_t>(time_tick,"power").leakage, module_it->second.queue.pull<power_t>(time_tick,"power").get_total());
 
      sprintf(debug_message,"      A leakage: = %lfW, dyn: = %lfW, total: = %lfW",module_it->second.queue.pull<power_t>(time_tick,"power").leakage, module_it->second.queue.pull<power_t>(time_tick,"power").get_total() - module_it->second.queue.pull<power_t>(time_tick,"power").leakage, module_it->second.queue.pull<power_t>(time_tick,"power").get_total());
      EI_DEBUG(debug_message);

        }

        old_temperature = new_temperature;
      }
#if 0
      double max_temp = package_it->second.thermal_library->get_partition_temperature(partition_it->first, CTR_TEMP); //MAX_TEMP
      partition_it->second.queue.update<double>(time_tick,period,"temperature",max_temp);
      sprintf(debug_message,"     MAX temperature = %3.2lfK",max_temp);
      EI_DEBUG(debug_message);
      sprintf(debug_message,"      A leakage_power = %lfK",partition_it->second.queue.pull<power_t>(time_tick,"power").leakage);
      EI_DEBUG(debug_message);
#endif
      
    }
  }
}


void energy_introspector_t::compute_reliability(double time_tick, double period, string partition_name,\
                                                double clock_frequency, double voltage, bool power_gating)
{
  EI_DEBUG("computing pseudo partition "+partition_name+" failure probability");
    
  // find pseudo partition
  map<string,pseudo_partition_t>::iterator partition_it = partition.find(partition_name);

  if((period == 0.0)||(partition_it == partition.end()))
  {
    EI_WARNING("EI","cannot compute failure probability for pseudo partition "+partition_name+"; pseudo partition and/or package is not found or sampling period = 0");
    return; // exit without temperature computation
  }

  if(partition_it->second.reliability_library != NULL)
  {
    long double failure_probability;

    dimension_t dimension = partition_it->second.queue.pull<dimension_t>(0.0,"dimension");
    double partition_temperature = partition_it->second.queue.pull<double>(time_tick,"temperature");
    
    if(partition_temperature < 300.0)
    {
      EI_WARNING("EI","pseudo partition "+partition_it->first+" does not have valid temperature for reliability computation");
      return;
    }

    failure_probability = partition_it->second.reliability_library->get_failure_probability\
    (time_tick,period,partition_temperature,dimension,clock_frequency,voltage,power_gating);

    partition_it->second.queue.push<long double>(time_tick,period,"failure_probability",failure_probability);
  }
}
