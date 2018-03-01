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

#ifndef EI_STAT_H
#define EI_STAT_H

#include <map>
#include <typeinfo>
#include <iostream>
#include <math.h>

#include "energy_introspector.h"

namespace EI {

  #define max_trunc 1e-15 // maximum truncation error
  
  class queue_t;
  
  class queue_wrapper_t
  {
  public:
    queue_wrapper_t(queue_t *q) : queue(q) {}
    
    queue_t *queue;
    vector<class pseudo_sensor_t*> sensor;
  };
  
  template <typename T> class runtime_queue_t;
  
  class queue_t
  {
  public:
    queue_t() {}
    virtual ~queue_t() {}
    
  private:
    map<string,queue_wrapper_t> queue_wrapper;
    
  public:
    void reset(void)
    {
      queue_wrapper.clear();
    }
    
    bool is_queue(string name)
    {
      map<string,queue_wrapper_t>::iterator it = queue_wrapper.find(name);
      
      if(it == queue_wrapper.end())
        return false;
      else
        return true;
    }
    
    // regular data queue
    template <typename T>
    double begin(string name, bool disregard = false)
    {
      double time_tick = MAX_TIME;
      if(is_queue(name))
      {
        runtime_queue_t<T> *rtq = dynamic_cast<runtime_queue_t<T>* >(queue_wrapper.find(name)->second.queue);
        if(!rtq)
        {
          if(!disregard)
            EI_WARNING("queue","dynamic_cast fails when accessing the runtime queue "+name+" in begin()");
          
          return time_tick;
        }
        time_tick = rtq->head;//rtq->window.begin()->first-rtq->window.begin()->second.period;
      }
      else
        if(!disregard)
          EI_WARNING("queue","cannot find the runtime queue "+name+" in begin()");
      
      return time_tick;
    }
    
    template <typename T>
    double end(string name, bool disregard = false)
    {
      double time_tick = 0.0;
      if(is_queue(name))
      {
        runtime_queue_t<T> *rtq = dynamic_cast<runtime_queue_t<T>* >(queue_wrapper.find(name)->second.queue);
        if(!rtq)
        {
          if(!disregard)
            EI_WARNING("queue","dynamic_cast fails when accessing the runtime queue "+name+"in end()");
          
          return time_tick;
        }
        time_tick = rtq->tail;//rtq->window.rbegin()->first;
      }
      else
        if(!disregard)
          EI_WARNING("queue","cannot find the runtime queue "+name+" in end()");
      
      return time_tick;
    }
    
    template <typename T>
    bool is_synchronous(double time_tick, double period, string name, bool disregard = false)
    {
      if(is_queue(name))
      {
        runtime_queue_t<T> *rtq = dynamic_cast<runtime_queue_t<T>* >(queue_wrapper.find(name)->second.queue);
        if(!rtq)
        {
          if(!disregard)
            EI_WARNING("queue","dynamic_cast fails when accessing the runtime queue "+name+" in is_synchronous()");
          
          return false;
        }
        return rtq->is_synchronous(time_tick,period);
      }
      else
        if(!disregard)
          EI_WARNING("queue","cannot find the runtime queue "+name+" in is_synchronous()");
      
      return false;
    }
    
    void create(string option, bool disregard = false)
    {
      char parse_option[256];
      char *name, *type, *size;
      
      sprintf(parse_option,"%s",option.c_str());
      
      name = strtok(parse_option," :\t\r\n#");
      
      if(queue_wrapper.find(name) != queue_wrapper.end())
      {
        if(!disregard)
          EI_WARNING("queue","skip creating runtime queue "+string(name)+" (duplicated)");
        
        return;
      }
      
      type = strtok(NULL," :\t\r\n#");    
      
      if(!type)
        EI_ERROR("queue","incomplete queue option "+option);
      
      size = strtok(NULL," :\t\r\n#");
      if(!size)
        EI_ERROR("queue","incomplete queue option "+option);
      
      if(!strcmp(type,"power_t"))
        create<power_t>(string(name),atoi(size),disregard);
      else if(type == "energy_t")
        create<energy_t>(string(name),atoi(size),disregard);
      else if(type == "dimension_t")
        create<dimension_t>(string(name),atoi(size),disregard);
      else if(type == "double")
        create<double>(string(name),atoi(size),disregard);
      else if(type == "int")
        create<int>(string(name),atoi(size),disregard);
      else if(type == "unsigned int")
        create<unsigned int>(string(name),atoi(size),disregard);
      else if(type == "bool")
        create<bool>(string(name),atoi(size),disregard);
      else if(type == "grid_t<power_t>")
        create<grid_t<power_t> >(string(name),atoi(size),disregard);
      else if(type == "grid_t<energy_t>")
        create<grid_t<energy_t> >(string(name),atoi(size),disregard);
      else if(type == "grid_t<double>")
        create<grid_t<double> >(string(name),atoi(size),disregard);
      else if(type == "grid_t<int>")
        create<grid_t<int> >(string(name),atoi(size),disregard);
      else if(type == "grid_t<unsigned int>")
        create<grid_t<unsigned int> >(string(name),atoi(size),disregard);
      else if(type == "grid_t<bool>")
        create<grid_t<bool> >(string(name),atoi(size),disregard);
      else
        EI_ERROR("queue","unknown variable type "+string(type)+" to create a queue "+string(name));
    }
    
    template <typename T>
    void create(string name, int size = 1, bool disregard = true)
    {
      if(queue_wrapper.find(name) != queue_wrapper.end())
      {
        if(!disregard)
          EI_WARNING("queue","skip creating runtime queue "+name+" (duplicated)");
        
        return;
      }
      
      runtime_queue_t<T> *rtq = new runtime_queue_t<T>(size); // T is runtime_queue
      queue_wrapper.insert(pair<string,queue_wrapper_t>(name,queue_wrapper_t(rtq)));
    }
    
    template <typename T>
    void remove(string name, bool disregard = false)
    {
      if(queue_wrapper.find(name) != queue_wrapper.end())
      {
        map<string,queue_wrapper_t>::iterator it = queue_wrapper.find(name);
        if(it != queue_wrapper.end())
          queue_wrapper.erase(it);
        else
          if(!disregard)
            EI_WARNING("queue","skip removing a queue "+name+ " (not found)");
      }
    }
    
    template <typename T>
    void push(double time_tick, double period, string name, T data, bool disregard = false)
    {
      if(is_queue(name))
      {
        runtime_queue_t<T> *rtq = dynamic_cast<runtime_queue_t<T>* >(queue_wrapper.find(name)->second.queue);
        if(!rtq)
        {
          if(!disregard)
            EI_WARNING("queue","dynamic_cast fails when accessing the runtime queue "+name+" in push()");
          
          return;
        }
        
        if((time_tick-rtq->head > max_trunc)&&(rtq->tail-time_tick > max_trunc))
          if(!disregard)
            EI_WARNING("queue","a new data "+name+" overlaps the queue data range in push()");
          else if(period&&!is_synchronous<T>(time_tick,period,name,disregard))
            if(!disregard)
              EI_WARNING("queue","a new data "+name+" is asynchronous to existing data in push()");
        
        rtq->push(time_tick,period,data);        
      }
      else
        EI_WARNING("queue","cannot find the runtime queue "+name+" in push()");
    }
    
    template <typename T>
    T pull(double time_tick, string name, bool disregard = false)
    {
      T data;
      
      if(is_queue(name))
      {
        runtime_queue_t<T> *rtq = dynamic_cast<runtime_queue_t<T>* >(queue_wrapper.find(name)->second.queue);
        if(!rtq)
        {
          if(!disregard)
            EI_WARNING("queue","dynamic_cast fails when accessing the runtime queue "+name+" in pull()");
          
          return data;
        }
        data = rtq->pull(time_tick);
        
        if((time_tick-rtq->tail > max_trunc)||(rtq->head-time_tick > max_trunc))
          if(!disregard)
          {
            char message[256];
            sprintf(message,"pulling data %s at %lf out of the queue range [%lf:%lf]",name.c_str(),time_tick,rtq->head,rtq->tail);
            EI_WARNING("queue",string(message));
          }
      }
      else
        EI_WARNING("queue","cannot find the runtime queue "+name+" in pull()");
      
      return data;
    }
    
    template <typename T>
    void update(double time_tick, double period, string name, T data, bool disregard = false)
    {
      if(is_queue(name))
      {
        runtime_queue_t<T> *rtq = dynamic_cast<runtime_queue_t<T>* >(queue_wrapper.find(name)->second.queue);
        if(!rtq)
        {
          if(!disregard)
            EI_WARNING("queue","dynamic_cast fails when accessing the runtime queue "+name+" in update()");
          
          return;
        }
        if(rtq->is_synchronous(time_tick,period))
          rtq->update(time_tick,period,data);
        else
          EI_WARNING("queue","skip asynchronous update of data "+name+" in update()");
      }
    }
    
    // sensor data queue
    int add_sensor(string name, pseudo_sensor_t &sensor)
    {
      if(is_queue(name))
      {
        int index = 0;
        map<string,queue_wrapper_t>::iterator it = queue_wrapper.find(name);
        vector<pseudo_sensor_t*>::iterator s_it;
        for(s_it = it->second.sensor.begin(); s_it < it->second.sensor.end(); s_it++)
        {
          if(*s_it == &sensor)
            break;
          index++;
        }
        if(s_it == it->second.sensor.end())
          queue_wrapper.find(name)->second.sensor.push_back(&sensor);
        else
          EI_WARNING("queue","adding duplicated pseudo sensors to the runtime queue "+name);
        
        return index;
      }
      else
        EI_WARNING("queue","cannot fine the runtime queue "+string(name)+" to add a pseudo sensor");
    }
    
    pseudo_sensor_t* get_sensor(string name,int index)
    {
      if(is_queue(name))
      {
        map<string,queue_wrapper_t>::iterator it = queue_wrapper.find(name);
        if(it->second.sensor.size() > 0)
        {
          if((index >= 0)&&(index < (int)it->second.sensor.size()))
            return it->second.sensor[index];
        }
        return NULL;
      }
      else
        EI_WARNING("queue","cannot find the runtime queue "+name+" in get_sensor()");
    }
  };
  
  template<typename T>
  class runtime_queue_t : public queue_t
  {
   private:
    class data_t{
    public:
      double period;
      T value;
    };
    
   public:
    runtime_queue_t<T>(int size) : window_size(size), head(0.0), tail(0.0) {}
    
    int window_size;
    double head, tail;
    map<double,data_t> window;
    
    void push(double time_tick, double period, T stat)
    {
      data_t data;
      data.period = period;
      data.value = stat;
      
      // backup the window to new stat's time_tick
      for(typename map<double,data_t>::reverse_iterator it = window.rbegin();
          (it != window.rend())&&window.size(); )
      {
        if((time_tick-it->first) > max_trunc)
          break;
        else
        {
          typename map<double,data_t>::iterator dead_entry = window.find(it->first);
          it++;
          window.erase(dead_entry);
        }
      }
      
      // overlap or gap between data points - flush window
      if((window.size() > 0)&&period&&(fabs(time_tick-period-window.rbegin()->first) > max_trunc))
        window.clear();
      
      // insert new data
      window.insert(pair<double,data_t>(time_tick,data));
      
      // resize window
      while(window.size() > window_size)
        window.erase(window.begin());
      
      head = window.begin()->first-window.begin()->second.period;//-max_trunc;
      tail = window.rbegin()->first;//+max_trunc;
    }
    
    T pull(double time_tick)
    {
      T data;
      
      // search the map backward
      for(typename map<double,data_t>::reverse_iterator it = window.rbegin();
          it != window.rend(); it++)
      {
        if((time_tick-it->first) > max_trunc) // no need to search further
          break;
        else if(it->first-it->second.period == 0.0)
        {
          data = it->second.value; 
          break;
        }
        else if(it->second.period == 0.0) // best-effort return for unspecified period
        {
          ++it;
          if((it == window.rend())||((time_tick-it->first) > max_trunc))
          {
            --it;
            data = it->second.value;
            break;
          }        
        }
        else if((time_tick-it->first+it->second.period) > max_trunc) // found the time_tick within the interval
        {
          data = it->second.value;
          break;
        }
      }
      return data;
    }
    
    void update(double time_tick, double period, T stat)
    {
      if(is_synchronous(time_tick,period))
      {
        if((window.size() == 0)||((time_tick-tail) > max_trunc))
          push(time_tick,period,stat);
        else
        {
          for(typename map<double,data_t>::reverse_iterator it = window.rbegin();\
              it != window.rend(); )
          {
            if(time_tick == it->first)
            {
              it->second.value = stat;
              break;
            }
          }
        }
      }
      
      head = window.begin()->first-window.begin()->second.period;//-max_trunc;
      tail = window.rbegin()->first;//+max_trunc;
    }
    
    bool is_synchronous(double time_tick, double period)
    {
      if(window.size() > 0)
      {
        for(typename map<double,data_t>::reverse_iterator it = window.rbegin();
            it != window.rend(); it++)
        {
          if((time_tick-it->first) > max_trunc)
            return fabs((time_tick-period-it->first) <= max_trunc);
          else if((time_tick-it->first+it->second.period) > max_trunc)
            return ((fabs(time_tick-period-it->first+it->second.period) <= max_trunc)&&(fabs(time_tick-it->first) <= max_trunc));
        }
      }
      else 
        return (time_tick-period-tail-max_trunc <= max_trunc)||(time_tick-period-tail >= 0.0);
      
      return false;
    }
    
    void clear()
    {
      window.clear();
      head = tail;
    }
  };
  
} // namespace EI

#endif
