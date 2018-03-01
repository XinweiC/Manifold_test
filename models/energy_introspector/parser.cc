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

#include "parser.h"

using namespace EI;

#define LINESIZE 1024

using namespace std;

void parser_t::parse(char *filename, parameters_t *parameters)
{
  FILE *file = fopen(filename,"r");
  if(!file)
  {
    fprintf(stdout,"EI ERROR (parser): cannot open config file %s\n",filename);
    exit(1);
  }

  char parseline[1024];
  char *option, *component, *variable, *value, *valid_line;
  unsigned int line_index = 0;

  while(1)
  {
    line_index++;
    fflush(file);
    valid_line = fgets(parseline,LINESIZE,file);
    if(/*feof(file)||*/!parseline||!valid_line)
      break;
    //cout << "parseline = " << parseline;
    if(parseline[0] != '-')
      continue;

    option = strtok(parseline," \t\r\n#");
    //cout << "option = " << option << endl;
    value = strtok(NULL," \t\r\n#");

    if(strcspn(option,".")>=strlen(option))
    {
      if(feof(file))
        break;
      else
      {
        fprintf(stdout,"EI ERROR (parser): incomponent config option (line %u): %s\n",line_index,option);
        exit(1);
      }
    }
    option = lowerstring(option);
    component = strtok(option,".");
    component = lowerstring(component);
    variable = strtok(NULL," \t\r\n#");
    variable = lowerstring(variable);
    if(!variable)
    {
      if(feof(file))
        break;
      else
      {
        fprintf(stdout,"EI ERROR (parser): incomplete config option (line %u): %s\n",line_index,parseline);
        exit(1);
      }
    }
    if(value)
    {
      //value = lowerstring(value);

      if(!stricmp(component,"-package"))
        package.add_option(variable,value);
      else if(!stricmp(component,"-partition"))
        partition.add_option(variable,value);
      else if(!stricmp(component,"-module"))
        module.add_option(variable,value);
      else if(!stricmp(component,"-sensor"))
        sensor.add_option(variable,value);
      else
      {
        fprintf(stdout,"EI ERROR (parser): unknown -component (line %u): %s\n",line_index,component);
        exit(1);
      }
    }
    else
    {
      if(stricmp(variable,"end"))
      {
        fprintf(stdout,"EI ERROR (parser): missing config value (line %u): %s.%s\n",line_index,component,variable);
		exit(1);
      }
      else
      {
        map<string,pair<string,bool> >::iterator opt_it;

        if(!stricmp(component,"-package"))
        {
          opt_it = package.options.find("name");
          if(opt_it == package.options.end())
          {
            fprintf(stdout,"EI ERROR (parser): pseudo package has no name");
            exit(1);
          }
          package.name = opt_it->second.first;
          package.type = "package";
          package.options.erase(opt_it);
          parameters->package.push_back(package);
          package.reset();
        }
        else if(!stricmp(component,"-partition"))
        {
          opt_it = partition.options.find("name");
          if(opt_it == partition.options.end())
          {
            fprintf(stdout,"EI ERROR (parser): pseudo partition has no name");
            exit(1);
          }
          partition.name = opt_it->second.first;
          partition.type = "partition";
          partition.options.erase(opt_it);
          parameters->partition.push_back(partition);
          partition.reset();
        }
        else if(!stricmp(component,"-module"))
        {
          opt_it = module.options.find("name");
          if(opt_it == module.options.end())
          {
            fprintf(stdout,"EI ERROR (parser): pseudo module has no name");
            exit(1);
          }
          module.name = opt_it->second.first;
          module.type = "module";
          module.options.erase(opt_it);
          parameters->module.push_back(module);
          module.reset();
        }
        else if(!stricmp(component,"-sensor"))
        {
          opt_it = sensor.options.find("name");
          if(opt_it == sensor.options.end())
          {
            fprintf(stdout,"EI ERROR (parser): pseudo sensor has no name");
            exit(1);
          }
          sensor.name = opt_it->second.first;
          sensor.type = "sensor";
          sensor.options.erase(opt_it);
          parameters->sensor.push_back(sensor);
          sensor.reset();
        }
        else
        {
          fprintf(stdout,"EI ERROR (parser): unknown -component (line %u): %s\n",line_index,component);
          exit(1);
        }
      }
    }
    if(feof(file))
      break;
  }
  
  fclose(file);
}
