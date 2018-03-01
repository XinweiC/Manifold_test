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

#include "parameters.h"
#include "parser.h"

using namespace EI;

void parameters_component_t::reset(void)
{
  options.clear();
}

void parameters_component_t::add_option(string opt, string val, bool overwrite)
{
  if(!overwrite)
    options.insert(pair<string,pair<string,bool> >(opt,pair<string,bool>(val,false)));    
  else
  {
    // find equal range of options in the multimap
    pair<multimap<string,pair<string,bool> >::iterator,multimap<string,pair<string,bool> >::iterator> options_rng = options.equal_range(opt);
      
    if(options_rng.first == options_rng.second)
    {
      // there is no such option yet, add option
      if(options_rng.first == options.end())
        add_option(opt,val);
      else // overwrite one matching option
        options_rng.first->second.first = val;
    }
    else // multiple matching options - which one to overwrite?
      EI_ERROR("parameters","cannot overwrite a parameter "+opt+" in "+name+" because there are many of them");
  }
}

void parameters_component_t::remove_option(string opt, string val) // remove matching options
{
  // find matching option range
  pair<multimap<string,pair<string,bool> >::iterator,multimap<string,pair<string,bool> >::iterator> options_rng = options.equal_range(opt);
  
  // remove matching <opt,val> pair - if val == "n/a", remove all matching options
  for(multimap<string,pair<string,bool> >::iterator options_it = options_rng.first;\
      options_it != options_rng.second; options_it++)
  {
    if(!stricmp(val,"n/a")||!stricmp(options_it->second.first,val))
      options.erase(options_it);
  }
}

string parameters_component_t::get_option(string opt, bool checkpoint)
{
  string option = lowerstring(opt);
  string val;
  
  // find matching option range
  pair<multimap<string,pair<string,bool> >::iterator,multimap<string,pair<string,bool> >::iterator> options_rng = options.equal_range(option);

  if(options_rng.first != options_rng.second)
  {
    multimap<string,pair<string,bool> >::iterator first_option = options_rng.first;
    multimap<string,pair<string,bool> >::iterator last_option = options_rng.second;
    --last_option;

    val = first_option->second.first; // return the first found entry

    // when there are multiple matching options, this function must be called multiple times 
    // to return all matching options and once more to return "n/a" to indicate it is done. 
    // an extra option is added to be used as a check point.
    if(checkpoint)
    {
      if(!stricmp(val,"checkpoint"))
      {
        remove_option(option,val);   
        return "n/a";
      }
      else
      {
        bool add_checkpoint = true;
        for(multimap<string,pair<string,bool> >::iterator it = ++options_rng.first;\
            it != options_rng.second; it++)
          if(first_option->second.second != it->second.second)
            add_checkpoint = false;

        if(add_checkpoint)
          options.insert(pair<string,pair<string,bool> >(option,pair<string,bool>("checkpoint",false)));
      }
    }

    first_option->second.second = true; // mark as a used option
       
    // re-order the multimap
    if(checkpoint||(first_option != last_option)) // multiple matching options
    {
      // remove the first entry and re-insert to the map to place it at the end
      remove_option(option,val);
      options.insert(pair<string,pair<string,bool> >(option,pair<string,bool>(val,true)));
    }
  }
  else 
    val = "n/a";

  return val;
}

void parameters_component_t::check_option(void)
{
  for(multimap<string,pair<string,bool> >::iterator options_it = options.begin();
      options_it != options.end(); options_it++)
  {
    if((!options_it->second.second)&&(stricmp(options_it->second.first,"multioption_end"))) // unused option
      EI_WARNING("parameters","option <"+options_it->first+","+options_it->second.first+"> is unused in "+type+" "+name);
  }
}

void parameters_t::parse(char *config)
{
  parser_t *parser = new parser_t();
  parser->parse(config,this);
}

namespace EI {

  char lowercase(char ch)
  {
    int alphabets = 'a'-'A';
    if((ch >= 'A') && (ch <= 'Z'))
      return ch+alphabets;
    else
      return ch;
  }
  
  char uppercase(char ch)
  {
    int alphabets = 'a'-'A';
    if((ch >= 'a') && (ch <= 'z'))
      return ch-alphabets;
    else
      return ch;
  }
  
  char* lowerstring(char *str)
  {
    for(int i = 0; i < strlen(str); i++)
      if((str[i] >= 'A')||(str[i] <= 'Z'))
        str[i] = lowercase(str[i]);
    return str;
  }
  
  string lowerstring(string str)
  {
    return (string)lowerstring((char*)str.c_str());
  }
  
  char* upperstring(char *str)
  {
    for(int i = 0; i < strlen(str); i++)
      if((str[i] >= 'a')||(str[i] <= 'z'))
        str[i] = uppercase(str[i]);
    return str;
  }
  
  string upperstring(string str)
  {
    return (string)upperstring((char*)str.c_str());
  }
  
  int stricmp(const char *str1, const char *str2)
  {
    unsigned char ch1, ch2;
    
    for (;;)
    {
      ch1 = (unsigned char)*str1++; ch1 = lowercase(ch1);
      ch2 = (unsigned char)*str2++; ch2 = lowercase(ch2);
      
      if (ch1 != ch2)
        return ch1 - ch2;
      if (ch1 == '\0')
        return 0;
    }
  }
  
  int stricmp(string str1, string str2)
  {
    return stricmp(str1.c_str(),str2.c_str());
  }
  
  void EI_ERROR(string location, string message)
  {
    fprintf(stderr,"EI ERROR (%s): %s\n",location.c_str(),message.c_str());
    exit(1);
  }
  
  void EI_WARNING(string location, string message)
  {
    fprintf(stdout,"EI WARNING (%s): %s\n",location.c_str(),message.c_str());
  }
  
  void EI_DISPLAY(string message)
  {
    fprintf(stdout,"%s\n",message.c_str());
  }
  
  void EI_DEBUG(string message)
  {
#ifdef ENERGY_INTROSPECTOR_DEBUG
    fprintf(stderr,"EI_DEBUG: %s\n",message.c_str());
#endif
  }
  
  /* double */
  void set_variable(double &var, parameters_component_t &param, string opt, double def, bool err, bool recursive)
  {
    string option = lowerstring(opt);
    string val = param.get_option(option);
    
    if(recursive)
    {
      parameters_component_t *parameters_component = &param;
      while((val == "n/a")&&parameters_component->upper_level)
      {
        parameters_component = parameters_component->upper_level;
        val = parameters_component->get_option(option);
      }
    }
    
    if(val == "n/a")
    {
      if(err)
        EI_ERROR("parameters",opt+" is not defined in "+param.name);
      else
        var = def;
    }
    else
      var = (double)atof(val.c_str());
  }
  
  /* int */
  void set_variable(int &var, parameters_component_t &param, string opt, int def, bool err, bool recursive)
  {
    string option = lowerstring(opt);
    string val = param.get_option(option);
    
    if(recursive)
    {
      parameters_component_t *parameters_component = &param;
      while((val == "n/a")&&parameters_component->upper_level)
      {
        parameters_component = parameters_component->upper_level;
        val = parameters_component->get_option(option);
      }
    }
    
    if(val == "n/a")
    {
      if(err)
        EI_ERROR("parameters",opt+" is not defined in "+param.name);
      else
        var = def;
    }
    else
      var = (int)atoi(val.c_str());
  }
  
  /* long int */
  void set_variable(long int &var, parameters_component_t &param, string opt, long int def, bool err, bool recursive)
  {
    string option = lowerstring(opt);
    string val = param.get_option(option);
    
    if(recursive)
    {
      parameters_component_t *parameters_component = &param;
      while((val == "n/a")&&parameters_component->upper_level)
      {
        parameters_component = parameters_component->upper_level;
        val = parameters_component->get_option(option);
      }
    }
    
    if(val == "n/a")
    {
      if(err)
        EI_ERROR("parameters",opt+" is not defined in "+param.name);
      else
        var = def;
    }
    else
      var = (long int)atol(val.c_str());
  }
  
  /* unsigned int */
  void set_variable(unsigned int &var, parameters_component_t &param, string opt, unsigned int def, bool err, bool recursive)
  {
    string option = lowerstring(opt);
    string val = param.get_option(option);
    
    if(recursive)
    {
      parameters_component_t *parameters_component = &param;
      while((val == "n/a")&&parameters_component->upper_level)
      {
        parameters_component = parameters_component->upper_level;
        val = parameters_component->get_option(option);
      }
    }
    
    if(val == "n/a")
    {
      if(err)
        EI_ERROR("parameters",opt+" is not defined in "+param.name);
      else
        var = def;
    }
    else
      var = (unsigned int)atol(val.c_str());
  }
  
  /* unsigned long int */
  void set_variable(unsigned long int &var, parameters_component_t &param, string opt, unsigned long int def, bool err, bool recursive)
  {
    string option = lowerstring(opt);
    string val = param.get_option(option);
    
    if(recursive)
    {
      parameters_component_t *parameters_component = &param;
      while((val == "n/a")&&parameters_component->upper_level)
      {
        parameters_component = parameters_component->upper_level;
        val = parameters_component->get_option(option);
      }
    }
    
    if(val == "n/a")
    {
      if(err)
        EI_ERROR("parameters",opt+" is not defined in "+param.name);
      else
        var = def;
    }
    else
      var = (unsigned long int)strtoul(val.c_str(),NULL,10);
  }
  
  /* bool */
  void set_variable(bool &var, parameters_component_t &param, string opt, bool def, bool err, bool recursive)
  {
    string option = lowerstring(opt);
    string val = param.get_option(option);
    
    if(recursive)
    {
      parameters_component_t *parameters_component = &param;
      while((val == "n/a")&&parameters_component->upper_level)
      {
        parameters_component = parameters_component->upper_level;
        val = parameters_component->get_option(option);
      }
    }
    
    if(val == "n/a")
    {
      if(err)
        EI_ERROR("parameters",opt+" is not defined in "+param.name);
      else
        var = def;
    }
    else
    {
      if(stricmp(val,"true")&&stricmp(val,"false")&&stricmp(val,"1")&&stricmp(val,"0"))
        EI_ERROR("parameters","option "+opt+" in "+param.name+" should be true or false");
      var = (bool)!(stricmp(val,"true")&&stricmp(val,"1"));
    }
  }
  
  /* char* */
  void set_variable(char* var, parameters_component_t &param, string opt, char* def, bool err, bool recursive)
  {
    string option = lowerstring(opt);
    string val = param.get_option(option);
    
    if(recursive)
    {
      parameters_component_t *parameters_component = &param;
      while((val == "n/a")&&parameters_component->upper_level)
      {
        parameters_component = parameters_component->upper_level;
        val = parameters_component->get_option(option);
      }
    }
    
    if(val == "n/a")
    {
      if(err)
        EI_ERROR("parameters",opt+" is not defined in "+param.name);
      else
        var = def;
    }
    else
    {
      if(!(stricmp(def,"true")&&stricmp(def,"false"))&&\
         stricmp(val,"true")&&stricmp(val,"false")&&stricmp(val,"1")&&stricmp(val,"0"))
        EI_ERROR("parameters","option "+opt+" in "+param.name+" should be true or false");
      var = (char*)val.c_str();
    }
  }
  
  /* string */
  void set_variable(string &var, parameters_component_t &param, string opt, string def, bool err, bool recursive)
  {
    string option = lowerstring(opt);
    string val = param.get_option(option);
    
    if(recursive)
    {
      parameters_component_t *parameters_component = &param;
      while((val == "n/a")&&parameters_component->upper_level)
      {
        parameters_component = parameters_component->upper_level;
        val = parameters_component->get_option(option);
      }
    }
    
    if(val == "n/a")
    {
      if(err)
        EI_ERROR("parameters",opt+" is not defined in "+param.name);
      else
        var = def;
    }
    else
    {
      if(!(stricmp(def,"true")&&stricmp(def,"false"))&&\
         stricmp(val,"true")&&stricmp(val,"false")&&stricmp(val,"1")&&stricmp(val,"0"))
        EI_ERROR("parameters","option "+opt+" in "+param.name+" should be true or false");
      var = (string)val;
    }
  }
  
} // namespace EI
