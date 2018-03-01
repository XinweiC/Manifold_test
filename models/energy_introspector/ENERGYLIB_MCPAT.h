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

#ifndef ENERGYLIB_MCPAT_H
#define ENERGYLIB_MCPAT_H
#include "energy_introspector.h"

#include "ENERGYLIB_MCPAT/mcpat0.8_r274/arch_const.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/array.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/basic_components.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/core.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/globalvar.h"		
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/interconnect.h"
//#include "ENERGYLIB_MCPAT/mcpat0.8_r274/iocontrollers.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/logic.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/memoryctrl.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/noc.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/processor.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/sharedcache.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/version.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/XML_Parse.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/xmlParser.h"

#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/arbiter.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/area.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/bank.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/basic_circuit.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/cacti_interface.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/component.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/const.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/crossbar.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/decoder.h"
//#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/highradix.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/htree2.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/io.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/mat.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/nuca.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/parameter.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/router.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/subarray.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/Ucache.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/uca.h"
#include "ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/wire.h"

namespace EI {

  class ENERGYLIB_MCPAT : public energy_library_t
  {
  public:
    ENERGYLIB_MCPAT() : energy_scaling(1.0), area_scaling(1.0), scaling(1.0) {} 
    ENERGYLIB_MCPAT(string component_name, parameters_t *parameters, energy_introspector_t *ei);  
    
    virtual energy_t get_unit_energy(void);
    virtual power_t get_tdp_power(void);
    virtual power_t get_runtime_power(double time_tick, double period, counters_t counters);
    virtual double get_leakage_power(double time_tick, double period);
    virtual double get_area(void);
    virtual void update_variable(string variable, void *value);
    
  private:
    int energy_model;
    enum ENERGY_MODEL
    {
      ARRAY,
      DEPENDENCY_CHECK_LOGIC,
      FLASH_CONTROLLER,
      FUNCTIONAL_UNIT,
      INSTRUCTION_DECODER,
      INTERCONNECT,
      MEMORY_CONTROLLER,
      NETWORK,
      NIU_CONTROLLER,
      PCIE_CONTROLLER,
      PIPELINE,
      SELECTION_LOGIC,
      UNDIFFERENTIATED_CORE,
      NUM_ENERGY_MODELS,
      NO_ENERGY_MODEL      
    };

    int energy_submodel;    
    enum ENERGY_SUBMODEL
    {
      ARRAY_MEMORY, // ARRAY
      ARRAY_CACHE, // ARRAY
      ARRAY_RAM, // ARRAY
      ARRAY_CAM, // ARRAY
      FUNCTIONAL_UNIT_ALU, // FUNCTIONAL_UNIT
      FUNCTIONAL_UNIT_MUL, // FUNCTIONAL_UNIT
      FUNCTIONAL_UNIT_FPU, // FUNCTIONAL_UNIT
      MEMORY_CONTROLLER_FRONTEND, // MEMORY_CONTROLLER
      MEMORY_CONTROLLER_BACKEND, // MEMORY_CONTROLLER
      MEMORY_CONTROLLER_PHY, // MEMORY_CONTROLLER
      NETWORK_BUS, // NETWORK
      NETWORK_ROUTER, // NETWORK
      NUM_ENERGY_SUBMODELS,
      NO_ENERGY_SUBMODEL      
    };
    
    double energy_scaling;
    double area_scaling;
    double scaling;
    
    class {
    public:
      double read, write, search, read_tag, write_tag;
    }TDP_duty_cycle;
    
    // McPAT input classes
    ParseXML XML_interface;
    InputParameter input_p;
    Device_ty device_ty;
    TechnologyParameter tech_p;
    TechnologyParameter tech_ref_p;
    MCParam mc_p;
    NoCParam noc_p;
    NIUParam niu_p;
    PCIeParam pci_p;
    ProcParam proc_p;
    CacheDynParam cache_p;
    CoreDynParam core_p;
    
    // McPAT library models
    ArrayST *McPAT_ArrayST;
    dep_resource_conflict_check *McPAT_dep_resource_conflict_check;
    FlashController *McPAT_FlashController;
    FunctionalUnit *McPAT_FunctionalUnit;
    inst_decoder *McPAT_inst_decoder;
    interconnect *McPAT_interconnect;
    MemoryController *McPAT_MemoryController;
    MCFrontEnd *McPAT_MCFrontEnd;
    MCBackend *McPAT_MCBackend;
    MCPHY *McPAT_MCPHY;
    NIUController *McPAT_NIUController;
    NoC *McPAT_NoC;
    PCIeController *McPAT_PCIeController;
    Pipeline *McPAT_Pipeline;
    selection_logic *McPAT_selection_logic;
    UndiffCore *McPAT_UndiffCore;
    
    void parse_XML(Processor *McPAT_processor, parameters_component_t &parameters_module);
    void EI_partition_config(FILE *McPAT_config, string partition_name, InputParameter McPAT_input,\
                             double target_clock_frequency, string component_type, string core_type);
    void EI_module_config(FILE *McPAT_config, ENERGYLIB_MCPAT *McPAT_obj, int ref_idx, int idx,\
                          double TDP_read_cycle, double TDP_write_cycle, double TDP_search_cycle,\
                          double TDP_read_tag_cycle, double TDP_write_tag_cycle);
    
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, ArrayST *obj) { McPAT_obj->McPAT_ArrayST = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, dep_resource_conflict_check *obj) { McPAT_obj->McPAT_dep_resource_conflict_check = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, FlashController *obj) { McPAT_obj->McPAT_FlashController = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, FunctionalUnit *obj) { McPAT_obj->McPAT_FunctionalUnit = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, inst_decoder *obj) { McPAT_obj->McPAT_inst_decoder = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, interconnect *obj) { McPAT_obj->McPAT_interconnect = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, MemoryController *obj) { McPAT_obj->McPAT_MemoryController = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, MCFrontEnd *obj) { McPAT_obj->McPAT_MCFrontEnd = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, MCBackend *obj) { McPAT_obj->McPAT_MCBackend = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, MCPHY *obj) { McPAT_obj->McPAT_MCPHY = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, NIUController *obj) { McPAT_obj->McPAT_NIUController = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, NoC *obj) { McPAT_obj->McPAT_NoC = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, PCIeController *obj) { McPAT_obj->McPAT_PCIeController = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, Pipeline *obj) { McPAT_obj->McPAT_Pipeline = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, selection_logic *obj) { McPAT_obj->McPAT_selection_logic = obj; }
    template <typename T>
    void assign(ENERGYLIB_MCPAT *McPAT_obj, UndiffCore *obj) { McPAT_obj->McPAT_UndiffCore = obj; }
    
    template <typename T>
    void populate_EI(FILE *McPAT_config, int ref_idx, int idx,\
                     double target_clock_frequency, T *model_obj, InputParameter McPAT_input,\
                     double scale, double area_scale, double energy_scale, bool adjust_area, \
                     double TDP_read_cycle, double TDP_write_cycle, double TDP_search_cycle,\
                     double TDP_read_tag_cycle, double TDP_write_tag_cycle,\
                     string partition_name, string module_name, int energy_model, int energy_submodel = -1)
    {
      ENERGYLIB_MCPAT *McPAT_obj = new ENERGYLIB_MCPAT();
      
      McPAT_obj->name = "mcpat:"+module_name;
      McPAT_obj->XML_interface = XML_interface;
      McPAT_obj->clock_frequency = target_clock_frequency;
      McPAT_obj->energy_model = energy_model;
      McPAT_obj->energy_submodel = energy_submodel;
      McPAT_obj->scaling = scale;
      McPAT_obj->area_scaling = area_scale;
      McPAT_obj->energy_scaling = energy_scale;
      
      assign<T>(McPAT_obj,model_obj);
      
      init_interface(&McPAT_input);
      McPAT_obj->input_p = *g_ip;
      McPAT_obj->tech_p = g_tp;
      
      if(McPAT_config)
      {
        fprintf(McPAT_config,"-module.name\t\t%s\n",module_name.c_str());
        fprintf(McPAT_config,"-module.partition\t\t%s\n",partition_name.c_str());
        fprintf(McPAT_config,"-module.energy_library\t\tMcPAT\n");
        
        switch(energy_model)
        {
          case ARRAY:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tarray\n");
            switch(energy_submodel)
            {
              case ARRAY_MEMORY:
                fprintf(McPAT_config,"-module.energy_submodel\t\tmemory\n");
                break;
              case ARRAY_CACHE:
                fprintf(McPAT_config,"-module.energy_submodel\t\tcache\n");
                break;
              case ARRAY_RAM:
                fprintf(McPAT_config,"-module.energy_submodel\t\tram\n");
                break;
              case ARRAY_CAM:
                fprintf(McPAT_config,"-module.energy_submodel\t\tcam\n");
                break;
              default: break;
            }
            break;
          }
          case DEPENDENCY_CHECK_LOGIC:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tdependency_check_logic\n");
            break;
          }
          case FLASH_CONTROLLER:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tflash_controller\n");
            break;
          }
          case FUNCTIONAL_UNIT:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tfunctional_unit\n");
            switch(energy_submodel)
            {
              case FUNCTIONAL_UNIT_ALU:
                fprintf(McPAT_config,"-module.energy_submodel\t\talu\n");
                break;
              case FUNCTIONAL_UNIT_MUL:
                fprintf(McPAT_config,"-module.energy_submodel\t\tmul\n");
                break;
              case FUNCTIONAL_UNIT_FPU:
                fprintf(McPAT_config,"-module.energy_submodel\t\tfpu\n");
                break;
              default: break;
            }
            break;
          }
          case INSTRUCTION_DECODER:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tinstruction_decoder\n");
            break;
          }
          case INTERCONNECT:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tinterconnect\n");
            break;
          }
          case MEMORY_CONTROLLER:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tmemory_controller\n");
            switch(energy_submodel)
            {
              case MEMORY_CONTROLLER_FRONTEND:
                fprintf(McPAT_config,"-module.energy_submodel\t\tfrontend\n");
                break;
              case MEMORY_CONTROLLER_BACKEND:
                fprintf(McPAT_config,"-module.energy_submodel\t\tbackend\n");
                break;
              case MEMORY_CONTROLLER_PHY:
                fprintf(McPAT_config,"-module.energy_submodel\t\tphy\n");
                break;
              default: break;
            }
            break;
          }
          case NETWORK:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tnetwork\n");
            switch(energy_submodel)
            {
              case NETWORK_BUS:
                fprintf(McPAT_config,"-module.energy_submodel\t\tbus\n");
                break;
              case NETWORK_ROUTER:
                fprintf(McPAT_config,"-module.energy_submodel\t\trouter\n");
                break;
              default: break;
            }
            break;
          }
          case NIU_CONTROLLER:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tniu_controller\n");
            break;
          }
          case PCIE_CONTROLLER:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tpcie_controller\n");
            break;
          }
          case PIPELINE:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tpipeline\n");
            break;
          }
          case SELECTION_LOGIC:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tselection_logic\n");
            break;
          }
          case UNDIFFERENTIATED_CORE:
          {
            fprintf(McPAT_config,"-module.energy_model\t\tundifferentiated_core\n");
            break;
          }
          default: break;
        }

        EI_module_config(McPAT_config,McPAT_obj,ref_idx,idx,\
                         TDP_read_cycle,TDP_write_cycle,TDP_search_cycle,TDP_read_tag_cycle,TDP_write_tag_cycle);
        if(area_scale != 1.0)
          fprintf(McPAT_config,"-module.area_scaling\t\t%lf\n",area_scale);
        if(energy_scale != 1.0)
          fprintf(McPAT_config,"-module.energy_scaling\t\t%lf\n",energy_scale);
        if(scale != 1.0)
          fprintf(McPAT_config,"-module.scaling\t\t%lf\n",scale);      
        if(adjust_area)
          fprintf(McPAT_config,"-module.adjust_area\t\ttrue\n");
        /*
         //if(TDP_read_cycle != 1.0)
         fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_read_cycle);
         //if(TDP_write_cycle != 1.0)
         fprintf(McPAT_config,"-module.TDP_duty_cycle.write\t\t%lf\n",TDP_write_cycle);
         //if(TDP_search_cycle != 0.0)
         fprintf(McPAT_config,"-module.TDP_duty_cycle.search\t\t%lf\n",TDP_search_cycle);
         //if(TDP_read_tag_cycle != 0.0)
         fprintf(McPAT_config,"-module.TDP_duty_cycle.read_tag\t\t%lf\n",TDP_read_tag_cycle);
         //if(TDP_write_tag_cycle != 0.0)
         fprintf(McPAT_config,"-module.TDP_duty_cycle.write_tag\t\t%lf\n",TDP_write_tag_cycle);
         */
        fprintf(McPAT_config,"-module.end\n\n");
      }
      
      pseudo_module_t pseudo_module;
      pseudo_module.energy_library = (energy_library_t*)McPAT_obj;
      pseudo_module.energy_library->get_unit_energy();
      pseudo_module.partition = partition_name;
      
      // default pseudo module queues
      pseudo_module.queue.create<dimension_t>("dimension",1);
      pseudo_module.queue.create<power_t>("power",1);
      pseudo_module.queue.create<power_t>("TDP",1);
      
      // update area
      dimension_t dimension;
      dimension.area = McPAT_obj->get_area();
      pseudo_module.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
      // initialize temperature queue
      //pseudo_module.queue.push<double>(0.0,0.0,"temperature",(double)XML_interface.sys.temperature);
      
      energy_introspector->module.insert(pair<string,pseudo_module_t>(module_name,pseudo_module));
      /*
       counters_t counters; // dummy counters
       energy_introspector->compute_power(0.0,0.0,module_name,counters,true); // compute TDP power
       */
    }
  };
  
} // namespace EI

#endif
