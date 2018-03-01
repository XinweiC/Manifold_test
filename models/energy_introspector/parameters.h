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
 
#ifndef EI_PARAM_H
#define EI_PARAM_H

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "definitions.h"

namespace EI {

  char lowercase(char ch);
  char uppercase(char ch);
  char* lowerstring(char *str);
  string lowerstring(string str);
  char* upperstring(char *str);
  string upperstring(string str);
  int stricmp(const char *str1, const char *str2);
  int stricmp(string str1, string str2);
  
  void EI_ERROR(string location, string message);
  void EI_WARNING(string location, string message);
  void EI_DISPLAY(string message);
  void EI_DEBUG(string message);
  
  class parameters_component_t
  {
  public:
    parameters_component_t() : name("n/a"), upper_level(NULL) { reset(); }
    ~parameters_component_t() { reset(); }
    
    string name, type;
    multimap<string,pair<string,bool> > options;
    
    parameters_component_t *upper_level;
    
    void reset(void);
    void add_option(string opt, string val, bool overwrite = false);
    void remove_option(string opt, string val = "n/a"); // remove matching options
    string get_option(string opt, bool checkpoint = false);
    void check_option(void);
  };
  
  class parameters_t {
  public:
    parameters_t() { reset(); }
    ~parameters_t() { reset(); }
    
    vector<parameters_component_t> package;
    vector<parameters_component_t> partition;
    vector<parameters_component_t> module;
    vector<parameters_component_t> sensor;
    
    // parameters initialization
    void parse(char *config);
    
    void reset(void)
    {
      package.clear();
      partition.clear();
      module.clear();
      sensor.clear();
    }
    
    void check_option(FILE *fp)
    {
      for(vector<parameters_component_t>::iterator it = package.begin(); it < package.end(); it++)
        it->check_option();
      for(vector<parameters_component_t>::iterator it = partition.begin(); it < partition.end(); it++)
        it->check_option();
      for(vector<parameters_component_t>::iterator it = module.begin(); it < module.end(); it++)
        it->check_option();
      for(vector<parameters_component_t>::iterator it = sensor.begin(); it < sensor.end(); it++)
        it->check_option();
    }
    
    parameters_component_t& get_package(string name)
    {
      vector<parameters_component_t>::iterator it;
      for(it = package.begin(); it < package.end(); it++)
        if(it->name == name)
          return (*it);
      EI_ERROR("parameters","cannot find package "+name);
    }
    
    parameters_component_t& get_partition(string name)
    {
      vector<parameters_component_t>::iterator it;
      for(it = partition.begin(); it < partition.end(); it++)
        if(it->name == name)
          return (*it);
      EI_ERROR("parameters","cannot find partition "+name);
    }
    
    parameters_component_t& get_module(string name)
    {
      vector<parameters_component_t>::iterator it;
      for(it = module.begin(); it < module.end(); it++)
        if(it->name == name)
          return (*it);
      EI_ERROR("parameters","cannot find module "+name);
    }
    
    parameters_component_t& get_sensor(string name)
    {
      vector<parameters_component_t>::iterator it;
      for(it = sensor.begin(); it < sensor.end(); it++)
        if(it->name == name)
          return (*it);
      EI_ERROR("parameters","cannot find sensor "+name);
    }
  };

  /* double */
  void set_variable(double &var, parameters_component_t &param, string opt, double def, bool err = false, bool recursive = true);
  
  /* int */
  void set_variable(int &var, parameters_component_t &param, string opt, int def, bool err = false, bool recursive = true);
  
  /* long int */
  void set_variable(long int &var, parameters_component_t &param, string opt, long int def, bool err = false, bool recursive = true);
  
  /* unsigned int */
  void set_variable(unsigned int &var, parameters_component_t &param, string opt, unsigned int def, bool err = false, bool recursive = true);
  
  /* unsigned long int */
  void set_variable(unsigned long int &var, parameters_component_t &param, string opt, unsigned long int def, bool err = false, bool recursive = true);
  
  /* bool */
  void set_variable(bool &var, parameters_component_t &param, string opt, bool def, bool err = false, bool recursive = true);
  
  /* char* */
  void set_variable(char* var, parameters_component_t &param, string opt, char* def, bool err = false, bool recursive = true);
  
  /* string */
  void set_variable(string &var, parameters_component_t &param, string opt, string def, bool err = false, bool recursive = true);
  
} // namespace EI

#endif
