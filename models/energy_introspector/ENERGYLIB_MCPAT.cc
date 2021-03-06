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

#include "ENERGYLIB_MCPAT.h"
#include <math.h>
#include <iostream>

using namespace EI;

ENERGYLIB_MCPAT::ENERGYLIB_MCPAT(string component_name, parameters_t *parameters, energy_introspector_t *ei) :
  McPAT_ArrayST(NULL),
  McPAT_dep_resource_conflict_check(NULL),
  McPAT_FlashController(NULL),
  McPAT_FunctionalUnit(NULL), 
  McPAT_inst_decoder(NULL),
  McPAT_interconnect(NULL), 
  McPAT_MemoryController(NULL),
  McPAT_MCFrontEnd(NULL),
  McPAT_MCBackend(NULL),
  McPAT_MCPHY(NULL),
  McPAT_NIUController(NULL),
  McPAT_NoC(NULL),  
  McPAT_PCIeController(NULL),
  McPAT_Pipeline(NULL),
  McPAT_selection_logic(NULL),
  McPAT_UndiffCore(NULL),
  energy_model(NO_ENERGY_MODEL),
  energy_submodel(NO_ENERGY_SUBMODEL)
{
  // energy_library_t parameters
  name = "mcpat";
  energy_introspector = ei;
  
  pseudo_module_t &pseudo_module = energy_introspector->module.find(component_name)->second;
  parameters_component_t &parameters_module = parameters->get_module(component_name);

  string option;
  
  set_variable(option,parameters_module,"opt_for_clk","true",/*must_be_defined?*/false,/*search upward to partition and package*/true);
  opt_for_clk = opt_for_clk&(bool)stricmp(option,"false");
  
  set_variable(option,parameters_module,"parse","none",false,false);
  if(stricmp(option,"none"))
  {
    XML_interface.parse((char*)option.c_str());
    Processor *McPAT_processor = new Processor(&XML_interface);
    McPAT_processor->displayEnergy(2,5);
    parse_XML(McPAT_processor,parameters_module);
    free(McPAT_processor);
  }
  else
  {
    set_variable(clock_frequency,parameters_module,"clock_frequency",0.0,true);

    // ENERGYLIB_MCPAT parameters
    set_variable(area_scaling,parameters_module,"area_scaling",1.0,false,false);
    set_variable(energy_scaling,parameters_module,"energy_scaling",1.0,false);
    //set_variable(energy_scaling,parameters_module,"energy_scaling",2.4,false);
    set_variable(scaling,parameters_module,"scaling",1.0,false,false);
    set_variable(TDP_duty_cycle.read,parameters_module,"TDP_duty_cycle.read",1.0,false,false);
    set_variable(TDP_duty_cycle.write,parameters_module,"TDP_duty_cycle.write",1.0,false,false);
    set_variable(TDP_duty_cycle.search,parameters_module,"TDP_duty_cycle.search",1.0,false,false);
    set_variable(TDP_duty_cycle.read_tag,parameters_module,"TDP_duty_cycle.read_tag",0.0,false,false);
    set_variable(TDP_duty_cycle.write_tag,parameters_module,"TDP_duty_cycle.write_tag",0.0,false,false);
    
    set_variable(option,parameters_module,"energy_model","n/a",true,false);
    if(!stricmp(option,"array"))
    {
      energy_model = ARRAY;
      set_variable(option,parameters_module,"energy_submodel","n/a",true,false);
      if(!stricmp(option,"memory"))
        energy_submodel = ARRAY_MEMORY;
      else if(!stricmp(option,"cache"))
        energy_submodel = ARRAY_CACHE;
      else if(!stricmp(option,"ram"))
        energy_submodel = ARRAY_RAM;
      else if(!stricmp(option,"cam"))
        energy_submodel = ARRAY_CAM;
      else
        EI_ERROR("McPAT","unknown energy_submodel "+option+" of array");
    }
    else if(!stricmp(option,"dependency_check_logic"))
      energy_model = DEPENDENCY_CHECK_LOGIC;
    else if(!stricmp(option,"flash_controller"))
      energy_model = FLASH_CONTROLLER;
    else if(!stricmp(option,"functional_unit"))
    {
      energy_model = FUNCTIONAL_UNIT;
      set_variable(option,parameters_module,"energy_submodel","n/a",true,false);
      if(!stricmp(option,"alu"))
        energy_submodel = FUNCTIONAL_UNIT_ALU;
      else if(!stricmp(option,"mul"))
        energy_submodel = FUNCTIONAL_UNIT_MUL;
      else if(!stricmp(option,"fpu"))
        energy_submodel = FUNCTIONAL_UNIT_FPU;
      else
        EI_ERROR("McPAT","unknown energy_submodel "+option+" of functional_unit");
    }
    else if(!stricmp(option,"instruction_decoder"))
      energy_model = INSTRUCTION_DECODER;
    else if(!stricmp(option,"interconnect"))
      energy_model = INTERCONNECT;
    else if(!stricmp(option,"memory_controller"))
    {
      energy_model = MEMORY_CONTROLLER;
      set_variable(option,parameters_module,"energy_submodel","n/a",true,false);
      if(!stricmp(option,"frontend"))
        energy_submodel = MEMORY_CONTROLLER_FRONTEND;
      else if(!stricmp(option,"backend"))
        energy_submodel = MEMORY_CONTROLLER_BACKEND;
      else if(!stricmp(option,"phy"))
        energy_submodel = MEMORY_CONTROLLER_PHY;
      else
        EI_ERROR("McPAT","unknown energy_submodel "+option+" of memory_controller");
    }
    else if(!stricmp(option,"network"))
    {
      energy_model = NETWORK;
      set_variable(option,parameters_module,"energy_submodel","n/a",true,false);
      if(!stricmp(option,"bus"))
        energy_submodel = NETWORK_BUS;
      else if(!stricmp(option,"router"))
        energy_submodel = NETWORK_ROUTER;
      else
        EI_ERROR("McPAT","unknown energy_submodel "+option+" of network");
    }
    else if(!stricmp(option,"niu_controller"))
      energy_model = NIU_CONTROLLER;
    else if(!stricmp(option,"pcie_controller"))
      energy_model = PCIE_CONTROLLER;
    else if(!stricmp(option,"pipeline"))
      energy_model = PIPELINE;
    else if(!stricmp(option,"selection_logic"))
      energy_model = SELECTION_LOGIC;
    else if(!stricmp(option,"undifferentiated_core"))
      energy_model = UNDIFFERENTIATED_CORE;
    else
      EI_ERROR("McPAT","unknown energy_model "+option);

    // general input parsing
    set_variable(input_p.temp,parameters_module,"temperature",0,true);

    set_variable(input_p.obj_func_dyn_energy,parameters_module,"obj_func_dyn_energy",0);
    set_variable(input_p.obj_func_dyn_power,parameters_module,"obj_func_dyn_power",0);
    set_variable(input_p.obj_func_leak_power,parameters_module,"obj_func_leak_power",0);
    set_variable(input_p.obj_func_cycle_t,parameters_module,"obj_func_cycle_t",1);
    set_variable(input_p.delay_wt,parameters_module,"delay_wt",100);
    set_variable(input_p.area_wt,parameters_module,"area_wt",0);
    set_variable(input_p.dynamic_power_wt,parameters_module,"dynamic_power_wt",100);
    set_variable(input_p.leakage_power_wt,parameters_module,"leakage_power_wt",0);
    set_variable(input_p.cycle_time_wt,parameters_module,"cycle_time_wt",0);
    set_variable(input_p.delay_dev,parameters_module,"delay_dev",10000);
    set_variable(input_p.area_dev,parameters_module,"area_dev",10000);
    set_variable(input_p.dynamic_power_dev,parameters_module,"dynamic_power_dev",10000);
    set_variable(input_p.leakage_power_dev,parameters_module,"leakage_power_dev",10000);
    set_variable(input_p.cycle_time_dev,parameters_module,"cycle_time_dev",10000);
    set_variable(input_p.ed,parameters_module,"ed",2);
    set_variable(input_p.nuca,parameters_module,"nuca_size",0);
    set_variable(input_p.nuca_bank_count,parameters_module,"nuca_bank_count",0);
    set_variable(input_p.delay_wt_nuca,parameters_module,"delay_wt_nuca",0);
    set_variable(input_p.area_wt_nuca,parameters_module,"area_wt_nuca",0);
    set_variable(input_p.dynamic_power_wt_nuca,parameters_module,"dynamic_power_wt_nuca",0);
    set_variable(input_p.leakage_power_wt_nuca,parameters_module,"leakage_power_wt_nuca",0);
    set_variable(input_p.cycle_time_wt_nuca,parameters_module,"cycle_time_wt_nuca",0);
    set_variable(input_p.delay_dev_nuca,parameters_module,"delay_dev_nuca",10000);
    set_variable(input_p.area_dev_nuca,parameters_module,"area_dev_nuca",10000);
    set_variable(input_p.dynamic_power_dev_nuca,parameters_module,"dynamic_power_dev_nuca",10);
    set_variable(input_p.leakage_power_dev_nuca,parameters_module,"leakage_power_dev_nuca",10000);
    set_variable(input_p.cycle_time_dev_nuca,parameters_module,"cycle_time_dev_nuca",10000);

    set_variable(option,parameters_module,"force_wire_type","false");
    input_p.force_wiretype = stricmp(option,"false");

    set_variable(option,parameters_module,"repeaters_in_htree","true");
    input_p.rpters_in_htree = stricmp(option,"false");

    set_variable(option,parameters_module,"with_clock_grid","false");
    input_p.with_clock_grid = stricmp(option,"false");

    set_variable(option,parameters_module,"force_cache_config","false");
    input_p.force_cache_config = stricmp(option,"false");

    if(input_p.force_cache_config)
    {
      set_variable(input_p.ndbl,parameters_module,"ndbl",0,true);
      set_variable(input_p.ndwl,parameters_module,"ndwl",0,true);
      set_variable(input_p.nspd,parameters_module,"nspd",0,true);
      set_variable(input_p.ndsam1,parameters_module,"ndsam1",0,true);
      set_variable(input_p.ndsam2,parameters_module,"ndsam2",0,true);
      set_variable(input_p.ndcm,parameters_module,"ndcm",0,true);
    }

    // don't cares
    input_p.fast_access = false; // don't care
    input_p.is_seq_acc = false; // don't care
    input_p.fully_assoc = false; // don't care
    input_p.print_input_args = false; // don't care
    input_p.burst_len = 1; // don't care
    input_p.int_prefetch_w = 1; // dont' care
    input_p.page_sz_bits = 0; // don't care
    input_p.ram_cell_tech_type = 0; // don't care
    input_p.peri_global_tech_type = 0; // don't care
    input_p.block_sz = 0; // don't care
    input_p.tag_assoc = 1; // don't care
    input_p.data_assoc = 1; // don't care
    input_p.cores = 0; // don't care
    input_p.print_detail = 5; // don't care
    input_p.cache_level = 0; // don't care

    // set default values -- should be modified by each energy model
    input_p.line_sz = 8;
    input_p.out_w = 64;
    input_p.assoc = 1;
    input_p.nbanks = 1;
    input_p.cache_sz = 64;
    input_p.tag_w = 0;
    input_p.num_rw_ports = 1;
    input_p.num_rd_ports = 0;
    input_p.num_wr_ports = 0;
    input_p.num_se_rd_ports = 0;
    input_p.num_search_ports = 0;
    input_p.throughput = 1.0/clock_frequency;
    input_p.latency = 1.0/clock_frequency;
    input_p.is_main_mem = false;
    input_p.is_cache = false;
    input_p.pure_ram = false;
    input_p.pure_cam = false;
    input_p.add_ecc_b_ = true;

    set_variable(option,parameters_module,"access_mode","normal");
    if(!stricmp(option,"normal"))
      input_p.access_mode = 0;
    else if(!stricmp(option,"sequential"))
      input_p.access_mode = 1;
    else if(!stricmp(option,"fast"))
      input_p.access_mode = 2;
    else
      EI_ERROR("McPAT","unknown access_mode "+option);

    set_variable(input_p.F_sz_nm,parameters_module,"feature_size",0.0,true);
    input_p.F_sz_nm *= 1e9;
    input_p.F_sz_um = input_p.F_sz_nm*0.001;
    
    // wire type
    set_variable(option,parameters_module,"wire_type","global");
    if(!stricmp(option,"global"))
      input_p.wt = (Wire_type)Global;
    else if(!stricmp(option,"global_5"))
      input_p.wt = (Wire_type)Global_5;
    else if(!stricmp(option,"global_10"))
      input_p.wt = (Wire_type)Global_10;
    else if(!stricmp(option,"goobal_20"))
      input_p.wt = (Wire_type)Global_20;
    else if(!stricmp(option,"global_30"))
      input_p.wt = (Wire_type)Global_30;
    else if(!stricmp(option,"low_swing"))
      input_p.wt = (Wire_type)Low_swing;
    else if(!stricmp(option,"semi_global"))
      input_p.wt = (Wire_type)Semi_global;
    else if(!stricmp(option,"transmission"))
      input_p.wt = (Wire_type)Transmission;
    else if(!stricmp(option,"optical"))
      input_p.wt = (Wire_type)Optical;
    else
      EI_ERROR("McPAT","unknown wire_type "+option);

    // general device type
    set_variable(option,parameters_module,"device_type","hp");
    if(!stricmp(option,"hp"))
     input_p.data_arr_ram_cell_tech_type = input_p.data_arr_peri_global_tech_type
      = input_p.tag_arr_ram_cell_tech_type = input_p.tag_arr_peri_global_tech_type = 0;
    else if(!stricmp(option,"lstp"))
      input_p.data_arr_ram_cell_tech_type = input_p.data_arr_peri_global_tech_type
      = input_p.tag_arr_ram_cell_tech_type = input_p.tag_arr_peri_global_tech_type = 1;
    else if(!stricmp(option,"lop"))
     input_p.data_arr_ram_cell_tech_type = input_p.data_arr_peri_global_tech_type
      = input_p.tag_arr_ram_cell_tech_type = input_p.tag_arr_peri_global_tech_type = 2;
    else if(!stricmp(option,"lp_dram"))
      input_p.data_arr_ram_cell_tech_type = input_p.data_arr_peri_global_tech_type
      = input_p.tag_arr_ram_cell_tech_type = input_p.tag_arr_peri_global_tech_type = 3;
    else if(!stricmp(option,"comm_dram"))
      input_p.data_arr_ram_cell_tech_type = input_p.data_arr_peri_global_tech_type
      = input_p.tag_arr_ram_cell_tech_type = input_p.tag_arr_peri_global_tech_type = 4;
    else
      EI_ERROR("McPAT","unknown device_type "+option);

    // overwrite the device type if specifically assigned
    set_variable(option,parameters_module,"device_type.data_arr_ram_cell_tech_type","n/a");
    if(stricmp(option,"n/a"))
    {
      if(!stricmp(option,"hp"))
        input_p.data_arr_ram_cell_tech_type = 0;
      else if(!stricmp(option,"lstp"))
        input_p.data_arr_ram_cell_tech_type = 1;
      else if(!stricmp(option,"lop"))
        input_p.data_arr_ram_cell_tech_type = 2;
      else if(!stricmp(option,"lp_dram"))
        input_p.data_arr_ram_cell_tech_type = 3;
      else if(!stricmp(option,"comm_dram"))
        input_p.data_arr_ram_cell_tech_type = 4;
      else
        EI_ERROR("McPAT","unknown device_type.data_arr_ram_cell_tech_type "+option);
    }
    set_variable(option,parameters_module,"device_type.data_arr_peri_global_tech_type","n/a");
    if(stricmp(option,"n/a"))
    {
      if(!stricmp(option,"hp"))
        input_p.data_arr_peri_global_tech_type = 0;
      else if(!stricmp(option,"lstp"))
        input_p.data_arr_peri_global_tech_type = 1;
      else if(!stricmp(option,"lop"))
        input_p.data_arr_peri_global_tech_type = 2;
      else if(!stricmp(option,"lp_dram"))
        input_p.data_arr_peri_global_tech_type = 3;
      else if(!stricmp(option,"comm_dram"))
        input_p.data_arr_peri_global_tech_type = 4;
      else
        EI_ERROR("McPAT","unknown device_type.data_arr_peri_global_tech_type "+option);
    }
    // overwrite the device type if specifically assigned
    set_variable(option,parameters_module,"device_type.tag_arr_ram_cell_tech_type","n/a");
    if(stricmp(option,"n/a"))
    {
      if(!stricmp(option,"hp"))
        input_p.tag_arr_ram_cell_tech_type = 0;
      else if(!stricmp(option,"lstp"))
        input_p.tag_arr_ram_cell_tech_type = 1;
      else if(!stricmp(option,"lop"))
        input_p.tag_arr_ram_cell_tech_type = 2;
      else if(!stricmp(option,"lp_dram"))
        input_p.tag_arr_ram_cell_tech_type = 3;
      else if(!stricmp(option,"comm_dram"))
        input_p.tag_arr_ram_cell_tech_type = 4;
      else
        EI_ERROR("McPAT","unknown device_type.tag_arr_ram_cell_tech_type "+option);
    }
    set_variable(option,parameters_module,"device_type.tag_arr_peri_global_tech_type","n/a");
    if(stricmp(option,"n/a"))
    {
      if(!stricmp(option,"hp"))
        input_p.tag_arr_peri_global_tech_type = 0;
      else if(!stricmp(option,"lstp"))
        input_p.tag_arr_peri_global_tech_type = 1;
      else if(!stricmp(option,"lop"))
        input_p.tag_arr_peri_global_tech_type = 2;
      else if(!stricmp(option,"lp_dram"))
        input_p.tag_arr_peri_global_tech_type = 3;
      else if(!stricmp(option,"comm_dram"))
        input_p.tag_arr_peri_global_tech_type = 4;
      else
        EI_ERROR("McPAT","unknown device_type.tag_arr_peri_global_tech_type "+option);
    }

    set_variable(option,parameters_module,"interconnect_projection","aggressive");
    if(!stricmp(option,"aggressive"))
      input_p.ic_proj_type = 0;
    else if(!stricmp(option,"conservative"))
      input_p.ic_proj_type = 1;
    else
      EI_ERROR("McPAT","unknown interconnect_projection "+option);

    // general wiring type
    input_p.ver_htree_wires_over_array = 0; // fixed
    input_p.broadcast_addr_din_over_ver_htrees = 0; // fixed

    set_variable(option,parameters_module,"wiring_type","global");
    if(!stricmp(option,"local"))
      input_p.wire_is_mat_type = input_p.wire_os_mat_type = 0;
    else if(!stricmp(option,"semi_global"))
      input_p.wire_is_mat_type = input_p.wire_os_mat_type = 1;
    else if(!stricmp(option,"global"))
      input_p.wire_is_mat_type = input_p.wire_os_mat_type = 2;
    else if(!stricmp(option,"dram"))
      input_p.wire_is_mat_type = input_p.wire_os_mat_type = 3;
    else
      EI_ERROR("McPAT","unknown wiring_type "+option);

    // overwrite the wiring type if specifically assigned
    set_variable(option,parameters_module,"wiring_type.wire_is_mat_type","n/a");
    if(stricmp(option,"n/a"))
    {
      if(!stricmp(option,"local"))
        input_p.wire_is_mat_type = 0;
      else if(!stricmp(option,"semi_global"))
        input_p.wire_is_mat_type = 1;
      else if(!stricmp(option,"global"))
        input_p.wire_is_mat_type = 2;
      else if(!stricmp(option,"dram"))
        input_p.wire_is_mat_type = 3;
      else
        EI_ERROR("McPAT","unknown wiring_type.wire_is_mat_type "+option);
    }
    set_variable(option,parameters_module,"wiring_type.wire_os_mat_type","n/a");
    if(stricmp(option,"n/a"))
    {
      if(!stricmp(option,"local"))
        input_p.wire_os_mat_type = 0;
      else if(!stricmp(option,"semi_global"))
        input_p.wire_os_mat_type = 1;
      else if(!stricmp(option,"global"))
        input_p.wire_os_mat_type = 2;
      else if(!stricmp(option,"dram"))
        input_p.wire_os_mat_type = 3;
      else
        EI_ERROR("McPAT","unknown wiring_type.wire_os_mat_type "+option);
    }
    set_variable(option,parameters_module,"wiring_type.ver_htree_wires_over_array","n/a");
    if(stricmp(option,"n/a"))
    {
      if(!stricmp(option,"local"))
        input_p.ver_htree_wires_over_array = 0;
      else if(!stricmp(option,"semi_global"))
        input_p.ver_htree_wires_over_array = 1;
      else if(!stricmp(option,"global"))
        input_p.ver_htree_wires_over_array = 2;
      else if(!stricmp(option,"dram"))
        input_p.ver_htree_wires_over_array = 3;
      else
        EI_ERROR("McPAT","unknown wiring_type.ver_htree_wires_over_array "+option);
    }
    set_variable(option,parameters_module,"broadcast_addr_din_over_ver_htrees","n/a");
    if(stricmp(option,"n/a"))
    {
      if(!stricmp(option,"local"))
        input_p.broadcast_addr_din_over_ver_htrees = 0;
      else if(!stricmp(option,"semi_global"))
        input_p.broadcast_addr_din_over_ver_htrees = 1;
      else if(!stricmp(option,"global"))
        input_p.broadcast_addr_din_over_ver_htrees = 2;
      else if(!stricmp(option,"dram"))
        input_p.broadcast_addr_din_over_ver_htrees = 3;
      else
        EI_ERROR("McPAT","unknown broadcast_addr_din_over_ver_htrees "+option);
    }

    // component types
    set_variable(option,parameters_module,"component_type","n/a",true);
    if(!stricmp(option,"core"))
      device_ty = Core_device;
    else if(!stricmp(option,"llc"))
      device_ty = LLC_device;
    else if(!stricmp(option,"uncore"))
      device_ty = Uncore_device;
    else
      EI_ERROR("McPAT","unknown component_type "+option);

    core_p.clockRate = clock_frequency;

    if(device_ty == Core_device)
    {
      set_variable(option,parameters_module,"opt_local","false");
      core_p.opt_local = stricmp(option,"false");

      set_variable(option,parameters_module,"core_type","inorder",true);
      if(!stricmp(option,"ooo"))
        core_p.core_ty = OOO;
      else if(!stricmp(option,"inorder"))
        core_p.core_ty = Inorder;
      else
        EI_ERROR("McPAT","unknown core_type "+option);
    }

    set_variable(option,parameters_module,"embedded","false");
    XML_interface.sys.Embedded = stricmp(option,"false");

    set_variable(option,parameters_module,"longer_channel_device","false");
    XML_interface.sys.longer_channel_device = stricmp(option,"false");

    switch(energy_model)
    {
      case ARRAY:
      {
        set_variable(input_p.line_sz,parameters_module,"line_width",0,true);
        set_variable(input_p.out_w,parameters_module,"output_width",0);
        set_variable(input_p.assoc,parameters_module,"assoc",1);
        set_variable(input_p.nbanks,parameters_module,"banks",1);
        set_variable(input_p.nsets,parameters_module,"sets",0);
        set_variable(input_p.cache_sz,parameters_module,"size",0);
        set_variable(input_p.tag_w,parameters_module,"tag_width",0);
        set_variable(input_p.num_rw_ports,parameters_module,"rw_ports",0);
        set_variable(input_p.num_rd_ports,parameters_module,"rd_ports",0);
        set_variable(input_p.num_wr_ports,parameters_module,"wr_ports",0);
        set_variable(input_p.num_se_rd_ports,parameters_module,"se_rd_ports",0);
        set_variable(input_p.num_search_ports,parameters_module,"search_ports",0);
        set_variable(input_p.throughput,parameters_module,"cycle_time",1.0);
        set_variable(input_p.latency,parameters_module,"access_time",1.0);
        
        set_variable(option,parameters_module,"add_ecc","true");
        input_p.add_ecc_b_ = stricmp(option,"false");
        
        switch(energy_submodel)
        {
          case ARRAY_MEMORY:
            input_p.is_main_mem = true;
            break;
          case ARRAY_CACHE:
            input_p.is_cache = true;
            break;
          case ARRAY_RAM:
            input_p.pure_ram = true;
            break;
          case ARRAY_CAM:
            input_p.pure_cam = true;
            break;
        }
        
        if(!(input_p.is_main_mem||input_p.is_cache||input_p.pure_ram||input_p.pure_cam))
          EI_ERROR("McPAT","unknown array type -- memory/cache/ram/cam");
        
        if(input_p.out_w == 0)
          input_p.out_w = input_p.line_sz*8;
        
        if(!(input_p.nsets||input_p.cache_sz))
          EI_ERROR("McPAT","unknown array size -- both sets and size are not defined");
        
        if(input_p.cache_sz == 0)
          input_p.cache_sz = input_p.line_sz*input_p.nsets*(input_p.assoc>0?input_p.assoc:1);
        else
          input_p.nsets = input_p.cache_sz/(input_p.line_sz*(input_p.assoc>0?input_p.assoc:1));
        input_p.specific_tag = (input_p.tag_w > 0);
        input_p.throughput /= clock_frequency;
        input_p.latency /= clock_frequency;
        
        // scale up the array size to the minimum that McPAT can handle
        /*
         if((input_p.assoc>0?input_p.assoc*input_p.nsets:input_p.nsets) < 16)
         {
         #ifdef EI_CONFIG_FILEOUT
         fprintf(energy_introspector->file_config,"EI WARNING (McPAT): %s array size is smaller than the minimum\n",parameters_module.name.c_str());
         #endif
         input_p.cache_sz = input_p.line_sz*16;
         }
         
         if(input_p.cache_sz < 64)
         {
         #ifdef EI_CONFIG_FILEOUT
         fprintf(energy_introspector->file_config,"EI WARNING (McPAT): %s array size is smaller than the minimum\n",parameters_module.name.c_str());
         #endif
         input_p.cache_sz = 64;
         }
         */
        
        McPAT_ArrayST = new ArrayST(&input_p,"array",device_ty,core_p.opt_local,core_p.core_ty);
        
        if(!McPAT_ArrayST)
          EI_ERROR("McPAT","unable to create an array "+parameters_module.name);
        
        bool adjust_area;
        set_variable(adjust_area,parameters_module,"adjust_area",false);
        
        if(adjust_area)
          McPAT_ArrayST->local_result.adjust_area();
          
        break;
      }
      case DEPENDENCY_CHECK_LOGIC:
      {
        int compare_bits;
        set_variable(compare_bits,parameters_module,"compare_bits",0,true);
        set_variable(core_p.decodeW,parameters_module,"decode_width",1,true); // to compute # of comparators
        
        McPAT_dep_resource_conflict_check = new dep_resource_conflict_check(&input_p,core_p,compare_bits,true);
        
        if(!McPAT_dep_resource_conflict_check)
          EI_ERROR("McPAT","unable to create a dependency_check_logic "+parameters_module.name);
        
        break;
      }
      case FLASH_CONTROLLER:
      {
        XML_interface.sys.flashc.mc_clock = clock_frequency * 1e-6; // in MHz
        
        XML_interface.sys.flashc.number_mcs = 1; // number of controller is set by scaling option
        
        set_variable(option,parameters_module,"type","high_performance",true);
        if(!stricmp(option,"high_performance"))
          XML_interface.sys.flashc.type = 0;
        else if(!stricmp(option,"low_power"))
          XML_interface.sys.flashc.type = 1;
        else 
          EI_ERROR("McPAT","unknown flash_controller type "+option);
        
        set_variable(XML_interface.sys.flashc.total_load_perc,parameters_module,"load_percentage",1.0); // load to BW ratio
        set_variable(XML_interface.sys.flashc.peak_transfer_rate,parameters_module,"peak_transfer_rate",200,true); // in MBps
        
        set_variable(option,parameters_module,"withPHY","true",true);
        XML_interface.sys.flashc.withPHY = stricmp(option,"false");
        
        McPAT_FlashController = new FlashController(&XML_interface,&input_p);
        
        if(!McPAT_FlashController)
          EI_ERROR("McPAT","unable to create a flash_controller "+parameters_module.name);
          
        break;
      }
      case FUNCTIONAL_UNIT:
      {
        int num_fu;
        set_variable(num_fu,parameters_module,"num_fu",1);
        
        core_p.num_alus = core_p.num_fpus = core_p.num_muls = num_fu;
        
        /*
         double duty_cycle;
         set_variable(duty_cycle,parameters_module,"duty_cycle",1);
         core_p.ALU_duty_cycle = core_p.ALU_duty_cycle = core_p.MUL_duty_cycle = duty_cycle;
         */
        
        switch(energy_submodel)
        {
          case FUNCTIONAL_UNIT_ALU:
            McPAT_FunctionalUnit = new FunctionalUnit(&XML_interface,0,&input_p,core_p,ALU);
            break;
          case FUNCTIONAL_UNIT_MUL:
            McPAT_FunctionalUnit = new FunctionalUnit(&XML_interface,0,&input_p,core_p,MUL);
            break;
          case FUNCTIONAL_UNIT_FPU:
            McPAT_FunctionalUnit = new FunctionalUnit(&XML_interface,0,&input_p,core_p,FPU);
            break;
        }
        
        if(!McPAT_FunctionalUnit)
          EI_ERROR("McPAT","unable to crate a functional_unit "+parameters_module.name);
          
        break;
      }
      case INSTRUCTION_DECODER:
      {
        set_variable(option,parameters_module,"x86","false");
        core_p.x86 = stricmp(option,"false");
        
        /* This part works different from the McPAT. */
        /* McPAT doesn't scale the leakage of the instruction decoder even if the decodeW changes. */
        if(core_p.x86)
        {
          area_scaling = scaling;
          scaling = 1.0;
        }
        
        int opcode;
        set_variable(opcode,parameters_module,"opcode",0,true); // opcode length
        
        McPAT_inst_decoder = new inst_decoder(true,&input_p,opcode,1,core_p.x86,device_ty,core_p.core_ty);
        
        if(!McPAT_inst_decoder)
          EI_ERROR("McPAT","unable to create an instruction_decoder "+parameters_module.name);
          
        break;
      }
      case INTERCONNECT:
      {
        if(XML_interface.sys.Embedded)
        {
          input_p.wire_is_mat_type = 0;
          input_p.wire_os_mat_type = 0;
          input_p.wt = Global_30;
        }
        else
        {
          input_p.wire_is_mat_type = 2;
          input_p.wire_os_mat_type = 2;
          input_p.wt = Global;
        }
        input_p.throughput = 1.0/clock_frequency;
        input_p.latency = 1.0/clock_frequency;
        
        set_variable(option,parameters_module,"pipelinable","false");
        input_p.pipelinable = stricmp(option,"false");
        
        double routing;
        if(input_p.pipelinable)
          set_variable(routing,parameters_module,"routing_over_percentage",0.5,true);
        
        bool opt_local;
        if(device_ty == Core_device)
          opt_local = core_p.opt_local;
        else
        {
          set_variable(option,parameters_module,"opt_local","n/a");
          if(!stricmp(option,"n/a"))
            set_variable(option,parameters_module,"opt_local","false");
          opt_local = stricmp(option,"false");
        }
        
        // overwrite the wire_type
        set_variable(option,parameters_module,"wire_type","global");
        if(!stricmp(option,"global"))
          input_p.wt = (Wire_type)Global;
        else if(!stricmp(option,"global_5"))
          input_p.wt = (Wire_type)Global_5;
        else if(!stricmp(option,"global_10"))
          input_p.wt = (Wire_type)Global_10;
        else if(!stricmp(option,"global_20"))
          input_p.wt = (Wire_type)Global_20;
        else if(!stricmp(option,"global_30"))
          input_p.wt = (Wire_type)Global_30;
        else if(!stricmp(option,"low_swing"))
          input_p.wt = (Wire_type)Low_swing;
        else if(!stricmp(option,"semi_global"))
          input_p.wt = (Wire_type)Semi_global;
        else if(!stricmp(option,"transmission"))
          input_p.wt = (Wire_type)Transmission;
        else if(!stricmp(option,"optical"))
          input_p.wt = (Wire_type)Optical;
        else
          EI_ERROR("McPAT","unknown wire_type "+option);
        
        // data width
        int data;
        set_variable(data,parameters_module,"data",0,true);
        
        // wire length
        double wire_length;
        set_variable(wire_length,parameters_module,"wire_length",0.0);
        
        if(wire_length <= 0.0)
        {
          wire_length = 0.0;
          
          string first_module = parameters_module.get_option("connect");
          string current_module = first_module;
          while(stricmp(current_module,"n/a"))
          {
            double increment = sqrt(energy_introspector->pull_data<dimension_t>(0.0,"module",current_module,"dimension").area);
            if(!increment)
              EI_WARNING("McPAT","interconnect "+parameters_module.name+" connects to "+current_module+" which has zero area");
            wire_length += increment;
            
            current_module = parameters_module.get_option("connect");
            if(!stricmp(first_module,current_module))
              break;
          }      
        }
        
        if((wire_length <= 0.0)||(wire_length >= 1.0)) // wire_length might be given as um
          EI_ERROR("McPAT","invalid interconnect wire_length");
        else
          wire_length *= 1e6; // um
        
        McPAT_interconnect = new interconnect("interconnect",device_ty,1,1,data,wire_length,&input_p,\
                                              3,input_p.pipelinable,routing,opt_local,core_p.core_ty,input_p.wt);
        
        if(!McPAT_interconnect)
          EI_ERROR("McPAT","unable to create an interconnect "+parameters_module.name);
          
        break;
      }
      case MEMORY_CONTROLLER:
      {
        // correction
        input_p.wire_is_mat_type = 2;
        input_p.wire_os_mat_type = 2;
        input_p.wt = (Wire_type)Global;
        
        XML_interface.sys.mc.mc_clock = clock_frequency*1e-6; // in MHz
        clock_frequency *= 2; // DDR
        
        XML_interface.sys.mc.number_mcs = 1; // scaled up by "scaling" option
        
        set_variable(XML_interface.sys.mc.llc_line_length,parameters_module,"line_width",0,true);      
        set_variable(XML_interface.sys.mc.databus_width,parameters_module,"databus_width",0,true);
        set_variable(XML_interface.sys.mc.addressbus_width,parameters_module,"addressbus_width",0,true);
        set_variable(XML_interface.sys.mc.memory_channels_per_mc,parameters_module,"memory_channels",0,true);
        set_variable(XML_interface.sys.mc.peak_transfer_rate,parameters_module,"transfer_rate",0,true);
        set_variable(XML_interface.sys.mc.number_ranks,parameters_module,"memory_ranks",0,true);
        
        set_variable(option,parameters_module,"lvds","true");
        XML_interface.sys.mc.LVDS = stricmp(option,"false"); 
        set_variable(option,parameters_module,"with_phy","false");
        XML_interface.sys.mc.withPHY = stricmp(option,"false");
        set_variable(option,parameters_module,"model","low_power",true);
        if(!stricmp(option,"high_performance"))
          XML_interface.sys.mc.type = 0;
        else if(!stricmp(option,"low_power"))
          XML_interface.sys.mc.type = 1;
        else
          EI_ERROR("McPAT","unknown memory_controller model "+option);
        
        MemoryCtrl_type memoryctrl_type;
        memoryctrl_type = MC;
        
        // the following lines are part of MemoryController::set_mc_param()
        mc_p.clockRate = XML_interface.sys.mc.mc_clock*2; // DDR
        mc_p.clockRate *= 1e6;
        mc_p.llcBlockSize = int(ceil(XML_interface.sys.mc.llc_line_length/8.0))+XML_interface.sys.mc.llc_line_length;
        mc_p.dataBusWidth = int(ceil(XML_interface.sys.mc.databus_width/8.0))+XML_interface.sys.mc.databus_width;
        mc_p.addressBusWidth = int(ceil(XML_interface.sys.mc.addressbus_width));
        mc_p.opcodeW = 16; // fixed
        mc_p.num_mcs = XML_interface.sys.mc.number_mcs;
        mc_p.num_channels = XML_interface.sys.mc.memory_channels_per_mc;
        mc_p.peakDataTransferRate = XML_interface.sys.mc.peak_transfer_rate;
        mc_p.memRank = XML_interface.sys.mc.number_ranks;
        mc_p.frontend_duty_cycle = 0.5; // fixed
        mc_p.LVDS = XML_interface.sys.mc.LVDS;
        mc_p.type = XML_interface.sys.mc.type;
        mc_p.withPHY = XML_interface.sys.mc.withPHY;
        
        switch(energy_submodel)
        {
          case MEMORY_CONTROLLER_FRONTEND:
          {
            set_variable(XML_interface.sys.mc.req_window_size_per_channel,parameters_module,"request_window_size",0,true);
            set_variable(XML_interface.sys.mc.IO_buffer_size_per_channel,parameters_module,"IO_buffer_size",0,true);
            set_variable(XML_interface.sys.physical_address_width,parameters_module,"physical_address_width",XML_interface.sys.mc.addressbus_width);
            
            McPAT_MCFrontEnd = new MCFrontEnd(&XML_interface,&input_p,mc_p,memoryctrl_type);
            if(!McPAT_MCFrontEnd)
              EI_ERROR("McPAT","unable to create a memory_controller frontend "+parameters_module.name);
              
            break;
          }
          case MEMORY_CONTROLLER_BACKEND:
          {
            McPAT_MCBackend = new MCBackend(&input_p,mc_p,memoryctrl_type);
            if(!McPAT_MCBackend)
              EI_ERROR("McPAT","unable to create a memory_controller backend "+parameters_module.name);
              
            break;
          }
          case MEMORY_CONTROLLER_PHY:
          {
            McPAT_MCPHY = new MCPHY(&input_p,mc_p,memoryctrl_type);
            if(!McPAT_MCPHY)
              EI_ERROR("McPAT","unable to create a memory_controller phy "+parameters_module.name);
              
            break;
          }
        }

        break;        
        /*
         // MC param debugging
         cout << "mcp.clockRate = " << mcp.clockRate << endl;
         cout << "mcp.llcBlockSize = " << mcp.llcBlockSize << endl;
         cout << "mcp.dataBusWidth = " << mcp.dataBusWidth << endl;
         cout << "mcp.addressBusWidth = " << mcp.addressBusWidth << endl;
         cout << "mcp.opcodeW = " << mcp.opcodeW << endl;
         cout << "mcp.num_mcs = " << mcp.num_mcs << endl;
         cout << "mcp.num_channels = " << mcp.num_channels;
         cout << "mcp.peakDataTransferRate = " << mcp.peakDataTransferRate << endl;
         cout << "mcp.memRank = " << mcp.memRank << endl;
         cout << "mcp.frontend_duty_cycle = " << mcp.frontend_duty_cycle << endl;
         cout << "mcp.LVDS = " << mcp.LVDS << endl;
         cout << "mcp.type = " << mcp.type << endl;
         cout << "mcp.withPHY = " << mcp.withPHY << endl;
         */      
      }
      case NETWORK:
      {
        // correction
        if(XML_interface.sys.Embedded)
        {
          input_p.wt = (Wire_type)Global_30;
          input_p.wire_is_mat_type = 0;
          input_p.wire_os_mat_type = 1;
        }
        else
        { 
          input_p.wt = (Wire_type)Global;
          input_p.wire_is_mat_type = 2;
          input_p.wire_os_mat_type = 2;
        }
        
        XML_interface.sys.NoC[0].clockrate = clock_frequency*1e-6;
        
        double link_length = 0.0;        
        switch(energy_submodel)
        {
          case NETWORK_BUS:
          {  
            XML_interface.sys.NoC[0].type = 0;
            
            set_variable(XML_interface.sys.NoC[0].link_throughput,parameters_module,"link_throughput",1.0);
            set_variable(XML_interface.sys.NoC[0].link_latency,parameters_module,"link_latency",1.0);
            set_variable(XML_interface.sys.NoC[0].chip_coverage,parameters_module,"chip_coverage",1.0);
            set_variable(XML_interface.sys.NoC[0].route_over_perc,parameters_module,"route_over_percentage",0.5);
            
            set_variable(link_length,parameters_module,"link_length",0.0,false,false);
            
            if(link_length == 0.0)
            {
              double chip_area;
              set_variable(chip_area,parameters_module,"chip_area",0.0);
              
              if(chip_area == 0.0) // use the EI information
              {
                string partition_name = pseudo_module.partition;
                if(energy_introspector->partition.find(partition_name) != energy_introspector->partition.end())
                  chip_area = energy_introspector->pull_data<dimension_t>(0.0,"package",energy_introspector->partition.find(partition_name)->second.package,"dimension").area;
              }
              link_length = sqrt(chip_area*XML_interface.sys.NoC[0].chip_coverage*1e12); // chip area in um^2
            }                
            if(link_length == 0.0) // still zero?
              EI_ERROR("McPAT","both link_length and chip_area are unknown for network");
              
            break;
          }
          case NETWORK_ROUTER:
          {
            XML_interface.sys.NoC[0].type = 1;
            
            set_variable(XML_interface.sys.NoC[0].input_ports,parameters_module,"input_ports",0,true);
            set_variable(XML_interface.sys.NoC[0].output_ports,parameters_module,"output_ports",0,true);
            set_variable(XML_interface.sys.NoC[0].virtual_channel_per_port,parameters_module,"virtual_channels",0,true);
            set_variable(XML_interface.sys.NoC[0].input_buffer_entries_per_vc,parameters_module,"input_buffer_entries",0,true);
            break;
          }
        }
        
        // scaled up by "scaling" option
        XML_interface.sys.NoC[0].vertical_nodes = 1;
        XML_interface.sys.NoC[0].horizontal_nodes = 1;      
        
        set_variable(XML_interface.sys.NoC[0].duty_cycle,parameters_module,"duty_cycle",1.0);      
        set_variable(XML_interface.sys.NoC[0].flit_bits,parameters_module,"flit_bits",0,true);
        
        double traffic_pattern;
        set_variable(traffic_pattern,parameters_module,"traffic_pattern",1.0);
        
        McPAT_NoC = new NoC(&XML_interface,0,&input_p,traffic_pattern,link_length); // chip area in um^2
        
        if(!McPAT_NoC)
          EI_ERROR("McPAT","unable to create a network "+parameters_module.name);
          
        break;
      }
      case NIU_CONTROLLER:
      {
        XML_interface.sys.niu.clockrate = clock_frequency * 1e-6; // in MHz
        
        XML_interface.sys.niu.number_units = 1; // number of controller is set by scaling option
        
        set_variable(option,parameters_module,"type","high_performance",true);
        if(!stricmp(option,"high_performance"))
          XML_interface.sys.niu.type = 0;
        else if(!stricmp(option,"low_power"))
          XML_interface.sys.niu.type = 1;
        else 
          EI_ERROR("McPAT","unknown niu_controller type "+option);
        
        set_variable(XML_interface.sys.niu.total_load_perc,parameters_module,"load_percentage",1.0); // load to BW ratio
        
        McPAT_NIUController = new NIUController(&XML_interface,&input_p);
        
        if(!McPAT_NIUController)
          EI_ERROR("McPAT","unable to create an niu_controller "+parameters_module.name);
          
        break;
      }
      case PCIE_CONTROLLER:
      {
        XML_interface.sys.pcie.clockrate = clock_frequency * 1e-6; // in MHz
        
        XML_interface.sys.pcie.number_units = 1; // number of controller is set by scaling option
        
        set_variable(option,parameters_module,"type","high_performance",true);
        if(!stricmp(option,"high_performance"))
          XML_interface.sys.niu.type = 0;
        else if(!stricmp(option,"low_power"))
          XML_interface.sys.niu.type = 1;
        else 
          EI_ERROR("McPAT","unknown pcie_controller type "+option);
        
        set_variable(XML_interface.sys.pcie.total_load_perc,parameters_module,"load_percentage",1.0); // load to BW ratio
        set_variable(XML_interface.sys.pcie.num_channels,parameters_module,"channels",0,true);
        set_variable(option,parameters_module,"withPHY","true",true);
        XML_interface.sys.flashc.withPHY = stricmp(option,"false");
        
        McPAT_PCIeController = new PCIeController(&XML_interface,&input_p);
        
        if(!McPAT_PCIeController)
          EI_ERROR("McPAT","unable to create an pcie_controller "+parameters_module.name);
          
        break;
      }
      case PIPELINE:
      {
        core_p.Embedded = XML_interface.sys.Embedded;
        
        set_variable(input_p.pipeline_stages,parameters_module,"pipeline_stages",0,true);
        set_variable(input_p.per_stage_vector,parameters_module,"per_stage_vector",0);
        
        if(input_p.per_stage_vector == 0) // McPAT calculates stage vector      
        {
          core_p.pipeline_stages = input_p.pipeline_stages;	
          set_variable(option,parameters_module,"x86","false");
          core_p.x86 = stricmp(option,"false");
          
          if(core_p.x86)
            set_variable(core_p.micro_opcode_length,parameters_module,"microopcode",0,true);
          else
            set_variable(core_p.opcode_length,parameters_module,"opcode",0,true);
          
          set_variable(core_p.pc_width,parameters_module,"pc",0,true);
          set_variable(core_p.fetchW,parameters_module,"fetch_width",0,true);
          set_variable(core_p.decodeW,parameters_module,"decode_width",0,true);
          set_variable(core_p.issueW,parameters_module,"issue_width",0,true);
          set_variable(core_p.instruction_length,parameters_module,"instruction_length",0,true);
          set_variable(core_p.int_data_width,parameters_module,"int_data_width",0,true);
          
          set_variable(core_p.num_hthreads,parameters_module,"hthreads",1,true);
          core_p.multithreaded = (core_p.num_hthreads > 1);
          if(core_p.multithreaded)
            set_variable(core_p.perThreadState,parameters_module,"thread_states",0,true);
          
          if(core_p.core_ty == Inorder)
          {
            set_variable(core_p.arch_ireg_width,parameters_module,"arch_int_regs",0,true);
            core_p.arch_ireg_width = int(ceil(log2(core_p.arch_ireg_width)));
          }
          else
          {
            set_variable(core_p.phy_ireg_width,parameters_module,"phy_int_regs",0,true);
            core_p.phy_ireg_width = int(ceil(log2(core_p.phy_ireg_width)));
            set_variable(core_p.v_address_width,parameters_module,"virtual_address",0,true);
            set_variable(core_p.commitW,parameters_module,"commit_width",0,true);
          }
        }
        
        McPAT_Pipeline = new Pipeline(&input_p,core_p,device_ty,(device_ty==Core_device)&&(input_p.per_stage_vector==0),true);
        
        if(!McPAT_Pipeline)
          EI_ERROR("McPAT","unable to create a pipeline "+parameters_module.name);
          
        break;
      }
      case SELECTION_LOGIC:
      {
        int input, output;
        set_variable(input,parameters_module,"selection_input",0,true);
        set_variable(output,parameters_module,"selection_output",0,true);
        
        McPAT_selection_logic = new selection_logic(true,input,output,&input_p,device_ty,core_p.core_ty);
        
        if(!McPAT_selection_logic)
          EI_ERROR("McPAT","unable to create a selection_logic "+parameters_module.name);
          
        break;
      }
      case UNDIFFERENTIATED_CORE:
      {
        set_variable(core_p.pipeline_stages,parameters_module,"pipeline_stages",0,true);
        set_variable(core_p.num_hthreads,parameters_module,"hthreads",1);
        set_variable(core_p.issueW,parameters_module,"issue_width",0,true);
        
        if(XML_interface.sys.Embedded)
        {
          set_variable(option,parameters_module,"opt_clockrate","true",true);
          XML_interface.sys.opt_clockrate = stricmp(option,"false");
        }
        
        McPAT_UndiffCore = new UndiffCore(&XML_interface,0,&input_p,core_p);
        
        if(!McPAT_UndiffCore)
          EI_ERROR("McPAT","unable to create an undifferentiated_core "+parameters_module.name);
          
        break;
      }
    }
    
    input_p = *g_ip; // Copy back error-corrected modular parameters
    tech_ref_p = tech_p = g_tp; // Copy back initialzed technology parameters

    double voltage;
    set_variable(voltage,parameters_module,"vdd",0.0);
    if(voltage > 0.0)
      update_variable("vdd",&voltage);

/*    
    // architectural parameters debugging
    cout << "cache_sz: " << g_ip->cache_sz << endl;
    cout << "line_sz: " << g_ip->line_sz << endl;
    cout << "assoc: " << g_ip->assoc << endl;
    cout << "nbanks: " << g_ip->nbanks << endl;
    cout << "out_w: " << g_ip->out_w << endl;
    cout << "specific_tag: " << g_ip->specific_tag << endl;
    cout << "tag_w: " << g_ip->tag_w << endl;
    cout << "access_mode: " << g_ip->access_mode << endl;
    cout << "obj_func_dyn_energy: " << g_ip->obj_func_dyn_energy << endl;
    cout << "obj_func_dyn_power: " << g_ip->obj_func_dyn_power << endl;
    cout << "obj_func_leak_power: " << g_ip->obj_func_leak_power << endl;
    cout << "obj_func_cycle_t: " << g_ip->obj_func_cycle_t << endl;
    cout << "F_sz_nm: " << g_ip->F_sz_nm << endl;
    cout << "F_sz_um: " << g_ip->F_sz_um << endl;
    cout << "num_rw_ports: " << g_ip->num_rw_ports << endl;
    cout << "num_rd_ports: " << g_ip->num_rd_ports << endl;
    cout << "num_wr_ports: " << g_ip->num_wr_ports << endl;
    cout << "num_se_rd_ports: " << g_ip->num_se_rd_ports << endl;
    cout << "num_search_ports: " << g_ip->num_search_ports << endl;
    cout << "is_main_mem: " << g_ip->is_main_mem << endl;
    cout << "is_cache: " << g_ip->is_cache << endl;
    cout << "pure_ram: " << g_ip->pure_ram << endl;
    cout << "pure_cam: " << g_ip->pure_cam << endl;
    cout << "rpters_in_htree: " << g_ip->rpters_in_htree << endl;
    cout << "ver_htree_wires_over_array: " << g_ip->ver_htree_wires_over_array << endl;
    cout << "broadcast_addr_din_over_ver_htrees: " << g_ip->broadcast_addr_din_over_ver_htrees << endl;
    cout << "temp: " << g_ip->temp << endl;
    cout << "ram_cell_tech_type: " << g_ip->ram_cell_tech_type << endl;
    cout << "peri_global_tech_type: " << g_ip->peri_global_tech_type << endl;
    cout << "data_arr_ram_cell_tech_type: " << g_ip->data_arr_ram_cell_tech_type << endl;
    cout << "data_arr_peri_global_tech_type: " << g_ip->data_arr_peri_global_tech_type << endl;
    cout << "tag_arr_ram_cell_tech_type: " << g_ip->tag_arr_ram_cell_tech_type << endl;
    cout << "tag_arr_peri_global_tech_type: " << g_ip->tag_arr_peri_global_tech_type << endl;
    cout << "burst_len: " << g_ip->burst_len << endl;
    cout << "int_prefetch_w: " << g_ip->int_prefetch_w << endl;
    cout << "page_sz_bits: " << g_ip->page_sz_bits << endl;
    cout << "ic_proj_type: " << g_ip->ic_proj_type << endl;
    cout << "wire_is_mat_type: " << g_ip->wire_is_mat_type << endl;
    cout << "wire_os_mat_type: " << g_ip->wire_os_mat_type << endl;
    cout << "Wiretype: " << g_ip->wt << endl;
    cout << "force_wiretype: " << g_ip->force_wiretype << endl;
    cout << "print_input_args: " << g_ip->print_input_args << endl;
    cout << " nuca_cache_sz: " << g_ip->nuca_cache_sz << endl;
    cout << "ndbl: " << g_ip->ndbl << endl;
    cout << "ndwl: " << g_ip->ndwl << endl;
    cout << "nspd: " << g_ip->nspd << endl;
    cout << "ndsam1: " << g_ip->ndsam1 << endl;
    cout << "ndsam2: " << g_ip->ndsam2 << endl;
    cout << "ndcm: " << g_ip->ndcm << endl;
    cout << "force_cache_config: " << g_ip->force_cache_config << endl;
    cout << "cache_level: " << g_ip->cache_level << endl;
    cout << "cores: " << g_ip->cores << endl;
    cout << "nuca_bank_count: " << g_ip->nuca_bank_count << endl;
    cout << "force_nuca_bank: " << g_ip->force_nuca_bank << endl;
    cout << "delay_wt: " << g_ip->delay_wt << endl;
    cout << "dynamic_power_wt: " << g_ip->dynamic_power_wt << endl;
    cout << "leakage_power_wt: " << g_ip->leakage_power_wt << endl;
    cout << "cycle_time_wt: " << g_ip->cycle_time_wt << endl;
    cout << "area_wt: " << g_ip->area_wt << endl;
    cout << "delay_wt_nuca: " << g_ip->delay_wt_nuca << endl;
    cout << "dynamic_power_wt_nuca: " << g_ip->dynamic_power_wt_nuca << endl;
    cout << "leakage_power_wt_nuca: " << g_ip->leakage_power_wt_nuca << endl;
    cout << "cycle_time_wt_nuca: " << g_ip->cycle_time_wt_nuca << endl;
    cout << "area_wt_nuca: " << g_ip->area_wt_nuca << endl;
    cout << "delay_dev: " << g_ip->delay_dev << endl;
    cout << "dynamic_power_dev: " << g_ip->dynamic_power_dev << endl;
    cout << "leakage_power_dev: " << g_ip->leakage_power_dev << endl;
    cout << "cycle_time_dev: " << g_ip->cycle_time_dev << endl;
    cout << "area_dev: " << g_ip->area_dev << endl;
    cout << "delay_dev_nuca: " << g_ip->delay_dev_nuca << endl;
    cout << "dynamic_power_dev_nuca: " << g_ip->dynamic_power_dev_nuca << endl;
    cout << "leakage_power_dev_nuca: " << g_ip->leakage_power_dev_nuca << endl;
    cout << "cycle_time_dev_nuca: " << g_ip->cycle_time_dev_nuca << endl;
    cout << "area_dev_nuca: " << g_ip->area_dev_nuca << endl;
    cout << "ed: " << g_ip->ed << endl;
    cout << "nuca: " << g_ip->nuca << endl;
    cout << "fast_access: " << g_ip->fast_access << endl;
    cout << "block_sz: " << g_ip->block_sz << endl;
    cout << "tag_assoc: " << g_ip->tag_assoc << endl;
    cout << "data_assoc: " << g_ip->data_assoc << endl;
    cout << "is_seq_acc: " << g_ip->is_seq_acc << endl;
    cout << "fully_assoc: " << g_ip->fully_assoc << endl;
    cout << "nsets: " << g_ip->nsets << endl;
    cout << "print_detail: " << g_ip->print_detail << endl;
    cout << "add_ecc_b_: " << g_ip->add_ecc_b_ << endl;
    cout << "throughput: " << g_ip->throughput << endl;
    cout << "latency: " << g_ip->latency << endl;
    cout << "pipelinable: " << g_ip->pipelinable << endl;
    cout << "pipeline_stages: " << g_ip->pipeline_stages << endl;
    cout << "per_stage_vector: " << g_ip->per_stage_vector << endl;
    cout << "with_clock_grid: " << g_ip->with_clock_grid << endl;
    cout << "\n" << endl;
  
    
    // technology parameters debugging
    cout << "g_tp.peri_global.Vdd = " << g_tp.peri_global.Vdd << endl;
    cout << "g_tp.peri_global.Vth = " << g_tp.peri_global.Vth << endl;
    cout << "g_tp.peri_global.l_phy = " << g_tp.peri_global.l_phy << endl;
    cout << "g_tp.peri_global.l_elec = " << g_tp.peri_global.l_elec << endl;
    cout << "g_tp.peri_global.t_ox = " << g_tp.peri_global.t_ox << endl;
    cout << "g_tp.peri_global.C_ox = " << g_tp.peri_global.C_ox << endl;
    cout << "g_tp.peri_global.C_g_ideal = " << g_tp.peri_global.C_g_ideal << endl;
    cout << "g_tp.peri_global.C_fringe = " << g_tp.peri_global.C_fringe << endl;
    cout << "g_tp.peri_global.C_overlap = " << g_tp.peri_global.C_overlap << endl;
    cout << "g_tp.peri_global.C_junc = " << g_tp.peri_global.C_junc << endl;
    cout << "g_tp.peri_global.C_junc_sidewall = " << g_tp.peri_global.C_junc_sidewall << endl;
    cout << "g_tp.peri_global.I_on_n = " << g_tp.peri_global.I_on_n << endl;
    cout << "g_tp.peri_global.I_on_p = " << g_tp.peri_global.I_on_p << endl;
    cout << "g_tp.peri_global.R_nch_on = " << g_tp.peri_global.R_nch_on << endl;
    cout << "g_tp.peri_global.R_pch_on = " << g_tp.peri_global.R_pch_on << endl;
    cout << "g_tp.peri_global.n_to_p_eff_curr_drv_ratio = " << g_tp.peri_global.n_to_p_eff_curr_drv_ratio << endl;
    cout << "g_tp.peri_global.long_channel_leakage_reduction = " << g_tp.peri_global.long_channel_leakage_reduction << endl;
    cout << "g_tp.peri_global.I_off_n = " << g_tp.peri_global.I_off_n << endl;
    cout << "g_tp.peri_global.I_off_p = " << g_tp.peri_global.I_off_p << endl;
    cout << "g_tp.peri_global.I_g_on_n = " << g_tp.peri_global.I_g_on_n << endl;
    cout << "g_tp.peri_global.I_g_on_p = " << g_tp.peri_global.I_g_on_p << endl;
    cout << "g_tp.sram_cell.Vdd = " << g_tp.sram_cell.Vdd << endl;
    cout << "g_tp.sram_cell.Vth = " << g_tp.sram_cell.Vth << endl;
    cout << "g_tp.sram_cell.l_phy = " << g_tp.sram_cell.l_phy << endl;
    cout << "g_tp.sram_cell.l_elec = " << g_tp.sram_cell.l_elec << endl;
    cout << "g_tp.sram_cell.t_ox = " << g_tp.sram_cell.t_ox << endl;
    cout << "g_tp.sram_cell.C_ox = " << g_tp.sram_cell.C_ox << endl;
    cout << "g_tp.sram_cell.C_g_ideal = " << g_tp.sram_cell.C_g_ideal << endl;
    cout << "g_tp.sram_cell.C_fringe = " << g_tp.sram_cell.C_fringe << endl;
    cout << "g_tp.sram_cell.C_overlap = " << g_tp.sram_cell.C_overlap << endl;
    cout << "g_tp.sram_cell.C_junc = " << g_tp.sram_cell.C_junc << endl;
    cout << "g_tp.sram_cell.C_junc_sidewall = " << g_tp.sram_cell.C_junc_sidewall << endl;
    cout << "g_tp.sram_cell.I_on_n = " << g_tp.sram_cell.I_on_n << endl;
    cout << "g_tp.sram_cell.I_on_p = " << g_tp.sram_cell.I_on_p << endl;
    cout << "g_tp.sram_cell.R_nch_on = " << g_tp.sram_cell.R_nch_on << endl;
    cout << "g_tp.sram_cell.R_pch_on = " << g_tp.sram_cell.R_pch_on << endl;
    cout << "g_tp.sram_cell.n_to_p_eff_curr_drv_ratio = " << g_tp.sram_cell.n_to_p_eff_curr_drv_ratio << endl;
    cout << "g_tp.sram_cell.long_channel_leakage_reduction = " << g_tp.sram_cell.long_channel_leakage_reduction << endl;
    cout << "g_tp.sram_cell.I_off_n = " << g_tp.sram_cell.I_off_n << endl;
    cout << "g_tp.sram_cell.I_off_p = " << g_tp.sram_cell.I_off_p << endl;
    cout << "g_tp.sram_cell.I_g_on_n = " << g_tp.sram_cell.I_g_on_n << endl;
    cout << "g_tp.sram_cell.I_g_on_p = " << g_tp.sram_cell.I_g_on_p << endl;
    cout << "g_tp.cam_cell.Vdd = " << g_tp.cam_cell.Vdd << endl;
    cout << "g_tp.cam_cell.Vth = " << g_tp.cam_cell.Vth << endl;
    cout << "g_tp.cam_cell.l_phy = " << g_tp.cam_cell.l_phy << endl;
    cout << "g_tp.cam_cell.l_elec = " << g_tp.cam_cell.l_elec << endl;
    cout << "g_tp.cam_cell.t_ox = " << g_tp.cam_cell.t_ox << endl;
    cout << "g_tp.cam_cell.C_ox = " << g_tp.cam_cell.C_ox << endl;
    cout << "g_tp.cam_cell.C_g_ideal = " << g_tp.cam_cell.C_g_ideal << endl;
    cout << "g_tp.cam_cell.C_fringe = " << g_tp.cam_cell.C_fringe << endl;
    cout << "g_tp.cam_cell.C_overlap = " << g_tp.cam_cell.C_overlap << endl;
    cout << "g_tp.cam_cell.C_junc = " << g_tp.cam_cell.C_junc << endl;
    cout << "g_tp.cam_cell.C_junc_sidewall = " << g_tp.cam_cell.C_junc_sidewall << endl;
    cout << "g_tp.cam_cell.I_on_n = " << g_tp.cam_cell.I_on_n << endl;
    cout << "g_tp.cam_cell.I_on_p = " << g_tp.cam_cell.I_on_p << endl;
    cout << "g_tp.cam_cell.R_nch_on = " << g_tp.cam_cell.R_nch_on << endl;
    cout << "g_tp.cam_cell.R_pch_on = " << g_tp.cam_cell.R_pch_on << endl;
    cout << "g_tp.cam_cell.n_to_p_eff_curr_drv_ratio = " << g_tp.cam_cell.n_to_p_eff_curr_drv_ratio << endl;
    cout << "g_tp.cam_cell.long_channel_leakage_reduction = " << g_tp.cam_cell.long_channel_leakage_reduction << endl;
    cout << "g_tp.cam_cell.I_off_n = " << g_tp.cam_cell.I_off_n << endl;
    cout << "g_tp.cam_cell.I_off_p = " << g_tp.cam_cell.I_off_p << endl;
    cout << "g_tp.cam_cell.I_g_on_n = " << g_tp.cam_cell.I_g_on_n << endl;
    cout << "g_tp.cam_cell.I_g_on_p = " << g_tp.cam_cell.I_g_on_p << endl;
    cout << "g_tp.wire_local.pitch = " << g_tp.wire_local.pitch << endl;
    cout << "g_tp.wire_local.R_per_um = " << g_tp.wire_local.R_per_um << endl;
    cout << "g_tp.wire_local.C_per_um = " << g_tp.wire_local.C_per_um << endl;
    cout << "g_tp.wire_local.horiz_dielectric_constant = " << g_tp.wire_local.horiz_dielectric_constant << endl;
    cout << "g_tp.wire_local.vert_dielectric_constant = " << g_tp.wire_local.vert_dielectric_constant << endl;
    cout << "g_tp.wire_local.aspect_ratio = " << g_tp.wire_local.aspect_ratio << endl;
    cout << "g_tp.wire_local.ild_thickness = " << g_tp.wire_local.ild_thickness << endl;
    cout << "g_tp.wire_local.miller_value = " << g_tp.wire_local.miller_value << endl;
    cout << "g_tp.wire_inside_mat.pitch = " << g_tp.wire_inside_mat.pitch << endl;
    cout << "g_tp.wire_inside_mat.R_per_um = " << g_tp.wire_inside_mat.R_per_um << endl;
    cout << "g_tp.wire_inside_mat.C_per_um = " << g_tp.wire_inside_mat.C_per_um << endl;
    cout << "g_tp.wire_inside_mat.horiz_dielectric_constant = " << g_tp.wire_inside_mat.horiz_dielectric_constant << endl;
    cout << "g_tp.wire_inside_mat.vert_dielectric_constant = " << g_tp.wire_inside_mat.vert_dielectric_constant << endl;
    cout << "g_tp.wire_inside_mat.aspect_ratio = " << g_tp.wire_inside_mat.aspect_ratio << endl;
    cout << "g_tp.wire_inside_mat.ild_thickness = " << g_tp.wire_inside_mat.ild_thickness << endl;
    cout << "g_tp.wire_inside_mat.miller_value = " << g_tp.wire_inside_mat.miller_value << endl;
    cout << "g_tp.wire_outside_mat.pitch = " << g_tp.wire_outside_mat.pitch << endl;
    cout << "g_tp.wire_outside_mat.R_per_um = " << g_tp.wire_outside_mat.R_per_um << endl;
    cout << "g_tp.wire_outside_mat.C_per_um = " << g_tp.wire_outside_mat.C_per_um << endl;
    cout << "g_tp.wire_outside_mat.horiz_dielectric_constant = " << g_tp.wire_outside_mat.horiz_dielectric_constant << endl;
    cout << "g_tp.wire_outside_mat.vert_dielectric_constant = " << g_tp.wire_outside_mat.vert_dielectric_constant << endl;
    cout << "g_tp.wire_outside_mat.aspect_ratio = " << g_tp.wire_outside_mat.aspect_ratio << endl;
    cout << "g_tp.wire_outside_mat.ild_thickness = " << g_tp.wire_outside_mat.ild_thickness << endl;
    cout << "g_tp.wire_outside_mat.miller_value = " << g_tp.wire_outside_mat.miller_value << endl;
    cout << "g_tp.scaling_factor.logic_scaling_co_eff = " << g_tp.scaling_factor.logic_scaling_co_eff << endl;
    cout << "g_tp.scaling_factor.core_tx_density = " << g_tp.scaling_factor.core_tx_density << endl;
    cout << "g_tp.scaling_factor.long_channel_leakage_reduction = " << g_tp.scaling_factor.long_channel_leakage_reduction << endl;
    cout << "g_tp.sram.b_w = " << g_tp.sram.b_w << endl;
    cout << "g_tp.sram.b_h = " << g_tp.sram.b_h << endl;
    cout << "g_tp.sram.cell_a_w = " << g_tp.sram.cell_a_w << endl;
    cout << "g_tp.sram.cell_pmos_w = " << g_tp.sram.cell_pmos_w << endl;
    cout << "g_tp.sram.cell_nmos_w = " << g_tp.sram.cell_nmos_w << endl;
    cout << "g_tp.sram.Vitbpre = " << g_tp.sram.Vbitpre << endl;
    cout << "g_tp.cam.b_w = " << g_tp.cam.b_w << endl;
    cout << "g_tp.cam.b_h = " << g_tp.cam.b_h << endl;
    cout << "g_tp.cam.cell_a_w = " << g_tp.cam.cell_a_w << endl;
    cout << "g_tp.cam.cell_pmos_w = " << g_tp.cam.cell_pmos_w << endl;
    cout << "g_tp.cam.cell_nmos_w = " << g_tp.cam.cell_nmos_w << endl;
    cout << "g_tp.cam.Vitbpre = " << g_tp.cam.Vbitpre << endl;
    cout << "g_tp.ram_wl_stitching_overhead_ = " << g_tp.ram_wl_stitching_overhead_ << endl;
    cout << "g_tp.min_w_nmos_ = " << g_tp.min_w_nmos_ << endl;
    cout << "g_tp.max_w_nmos_ = " << g_tp.max_w_nmos_ << endl;
    cout << "g_tp.max_w_nmos_dec = " << g_tp.max_w_nmos_dec << endl;
    cout << "g_tp.unit_len_wire_del = " << g_tp.unit_len_wire_del << endl;
    cout << "g_tp.FO4 = " << g_tp.FO4 << endl;
    cout << "g_tp.kinv = " << g_tp.kinv << endl;
    cout << "g_tp.vpp = " << g_tp.vpp << endl;
    cout << "g_tp.w_sense_en = " << g_tp.w_sense_en << endl;
    cout << "g_tp.w_sense_n = " << g_tp.w_sense_n << endl;
    cout << "g_tp.w_sense_p = " << g_tp.w_sense_p << endl;
    cout << "g_tp.sense_delay = " << g_tp.sense_delay << endl;
    cout << "g_tp.sense_dy_power = " << g_tp.sense_dy_power << endl;
    cout << "g_tp.w_iso = " << g_tp.w_iso << endl;
    cout << "g_tp.w_poly_contact = " << g_tp.w_poly_contact << endl;
    cout << "g_tp.spacing_poly_to_contact = " << g_tp.spacing_poly_to_contact << endl;
    cout << "g_tp.spacing_poly_to_poly = " << g_tp.spacing_poly_to_poly << endl;
    cout << "g_tp.w_comp_inv_p1 = " << g_tp.w_comp_inv_p1 << endl;
    cout << "g_tp.w_comp_inv_n1 = " << g_tp.w_comp_inv_n1 << endl;
    cout << "g_tp.w_comp_inv_p2 = " << g_tp.w_comp_inv_p2 << endl;
    cout << "g_tp.w_comp_inv_n2 = " << g_tp.w_comp_inv_n2 << endl;
    cout << "g_tp.w_comp_inv_p3 = " << g_tp.w_comp_inv_p3 << endl;
    cout << "g_tp.w_comp_inv_n3 = " << g_tp.w_comp_inv_n3 << endl;
    cout << "g_tp.w_eval_inv_p = " << g_tp.w_eval_inv_p << endl;
    cout << "g_tp.w_eval_inv_n = " << g_tp.w_eval_inv_n << endl;
    cout << "g_tp.w_comp_n = " << g_tp.w_comp_n << endl;
    cout << "g_tp.w_comp_p = " << g_tp.w_comp_p << endl;
    cout << "g_tp.dram_cell_I_on = " << g_tp.dram_cell_I_on << endl;
    cout << "g_tp.dram_cell_Vdd = " << g_tp.dram_cell_Vdd << endl;
    cout << "g_tp.dram_cell_I_off_worst_case_len_temp = " << g_tp.dram_cell_I_off_worst_case_len_temp << endl;
    cout << "g_tp.dram_cell_C = " << g_tp.dram_cell_C << endl;
    cout << "g_tp.gm_sense_amp_latch = " << g_tp.gm_sense_amp_latch << endl;
    cout << "g_tp.w_nmos_b_mux = " << g_tp.w_nmos_b_mux << endl;
    cout << "g_tp.w_nmos_sa_mux = " << g_tp.w_nmos_sa_mux << endl;
    cout << "g_tp.w_pmos_bl_precharge = " << g_tp.w_pmos_bl_precharge << endl;
    cout << "g_tp.w_pmos_bl_eq = " << g_tp.w_pmos_bl_eq << endl;
    cout << "g_tp.MIN_GAP_BET_P_AND_N_DIFFS = " << g_tp.MIN_GAP_BET_P_AND_N_DIFFS << endl;
    cout << "g_tp.MIN_GAP_BET_SAME_TYPE_DIFFS = " << g_tp.MIN_GAP_BET_SAME_TYPE_DIFFS << endl;
    cout << "g_tp.HPOWERRAIL = " << g_tp.HPOWERRAIL << endl;
    cout << "g_tp.cell_h_def = " << g_tp.cell_h_def << endl;
    cout << "g_tp.chip_layout_overhead = " << g_tp.chip_layout_overhead << endl;
    cout << "g_tp.macro_layout_overhead = " << g_tp.macro_layout_overhead << endl;
    cout << "g_tp.sckt_co_eff = " << g_tp.sckt_co_eff << endl;
    cout << "g_tp.fringe_cap = " << g_tp.fringe_cap << endl;
    cout << "g_tp.h_dec = " << g_tp.h_dec << endl;

    cout << "g_tp.dram_acc.Vth = " << g_tp.dram_acc.Vth << endl;
    cout << "g_tp.dram_acc.l_phy = " << g_tp.dram_acc.l_phy << endl;
    cout << "g_tp.dram_acc.l_elec = " << g_tp.dram_acc.l_elec << endl;
    cout << "g_tp.dram_acc.C_g_ideal = " << g_tp.dram_acc.C_g_ideal << endl;
    cout << "g_tp.dram_acc.C_fringe = " << g_tp.dram_acc.C_fringe << endl;
    cout << "g_tp.dram_acc.C_junc = " << g_tp.dram_acc.C_junc << endl;
    cout << "g_tp.dram_acc.C_junc_sidewall = " << g_tp.dram_acc.C_junc_sidewall << endl;
    cout << "g_tp.dram_acc.I_on_n = " << g_tp.dram_acc.I_on_n << endl;
    cout << "g_tp.dram_wl.l_phy = " << g_tp.dram_wl.l_phy << endl;
    cout << "g_tp.dram_wl.l_elec = " << g_tp.dram_wl.l_elec << endl;
    cout << "g_tp.dram_wl.C_g_ideal = " << g_tp.dram_wl.C_g_ideal << endl;
    cout << "g_tp.dram_wl.C_fringe = " << g_tp.dram_wl.C_fringe << endl;
    cout << "g_tp.dram_wl.C_junc = " << g_tp.dram_wl.C_junc << endl;
    cout << "g_tp.dram_wl.C_junc_sidewall = " << g_tp.dram_wl.C_junc_sidewall << endl;
    cout << "g_tp.dram_wl.I_on_n = " << g_tp.dram_wl.I_on_n << endl;
    cout << "g_tp.dram_wl.R_nch_on = " << g_tp.dram_wl.R_nch_on << endl;
    cout << "g_tp.dram_wl.R_pch_on = " << g_tp.dram_wl.R_pch_on << endl;
    cout << "g_tp.dram_wl.n_to_p_eff_curr_drv_ratio = " << g_tp.dram_wl.n_to_p_eff_curr_drv_ratio << endl;
    cout << "g_tp.dram_wl.long_channel_leakage_reduction = " << g_tp.dram_wl.long_channel_leakage_reduction << endl;
    cout << "g_tp.dram_wl.I_off_n = " << g_tp.dram_wl.I_off_n << endl;
    cout << "g_tp.dram_wl.I_off_p = " << g_tp.dram_wl.I_off_p << endl;
*/    
  }
}


energy_t ENERGYLIB_MCPAT::get_unit_energy(void)
{
  energy_t unit_energy;

  switch(energy_model)
  {
    case NO_ENERGY_MODEL:
      break;
    case ARRAY:
    {
      unit_energy.read = McPAT_ArrayST->local_result.power.readOp.dynamic\
      +McPAT_ArrayST->local_result.power.readOp.short_circuit;
      unit_energy.write = McPAT_ArrayST->local_result.power.writeOp.dynamic\
      +McPAT_ArrayST->local_result.power.writeOp.short_circuit;
      
      if(McPAT_ArrayST->l_ip.tag_w > 0)
      {
        if(McPAT_ArrayST->local_result.tag_array2) // tag array exists
        {
          unit_energy.read_tag = McPAT_ArrayST->local_result.tag_array2->power.readOp.dynamic+McPAT_ArrayST->local_result.tag_array2->power.readOp.short_circuit;
          unit_energy.write_tag = McPAT_ArrayST->local_result.tag_array2->power.writeOp.dynamic+McPAT_ArrayST->local_result.tag_array2->power.writeOp.short_circuit;
        }
        else
        {
          unit_energy.read_tag = unit_energy.read;
          unit_energy.write_tag = unit_energy.write;
        }
      }
      
      if(McPAT_ArrayST->l_ip.num_search_ports)
        unit_energy.search = McPAT_ArrayST->local_result.power.searchOp.dynamic+McPAT_ArrayST->local_result.power.searchOp.short_circuit;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_ArrayST->local_result.power.readOp.longer_channel_leakage+McPAT_ArrayST->local_result.power.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_ArrayST->local_result.power.readOp.leakage+McPAT_ArrayST->local_result.power.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case DEPENDENCY_CHECK_LOGIC:
    {
      unit_energy.read = McPAT_dep_resource_conflict_check->power.readOp.dynamic+McPAT_dep_resource_conflict_check->power.readOp.short_circuit;
      unit_energy.write = unit_energy.read;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_dep_resource_conflict_check->power.readOp.longer_channel_leakage+McPAT_dep_resource_conflict_check->power.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_dep_resource_conflict_check->power.readOp.leakage+McPAT_dep_resource_conflict_check->power.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case FLASH_CONTROLLER:
    {
      unit_energy.read = unit_energy.write = McPAT_FlashController->power_t.readOp.dynamic;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_FlashController->power_t.readOp.longer_channel_leakage+McPAT_FlashController->power_t.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_FlashController->power_t.readOp.leakage+McPAT_FlashController->power_t.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case FUNCTIONAL_UNIT:
    {
      unit_energy.baseline = McPAT_FunctionalUnit->base_energy/clock_frequency*tech_p.sckt_co_eff;
      unit_energy.read = McPAT_FunctionalUnit->per_access_energy*tech_p.sckt_co_eff;
      unit_energy.write = unit_energy.read;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_FunctionalUnit->leakage*longer_channel_device_reduction(device_ty,McPAT_FunctionalUnit->coredynp.core_ty)+McPAT_FunctionalUnit->gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_FunctionalUnit->leakage+McPAT_FunctionalUnit->gate_leakage)/clock_frequency;

      // McPAT Functiona Unit is awful      
      if((energy_submodel == FUNCTIONAL_UNIT_ALU)||(energy_submodel == FUNCTIONAL_UNIT_MUL))
        unit_energy.leakage *= scaling;
      break;
    }
    case INSTRUCTION_DECODER:
    {
      unit_energy.read = McPAT_inst_decoder->power.readOp.dynamic+McPAT_inst_decoder->power.readOp.short_circuit;
      unit_energy.write = unit_energy.read;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_inst_decoder->power.readOp.longer_channel_leakage+McPAT_inst_decoder->power.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_inst_decoder->power.readOp.leakage+McPAT_inst_decoder->power.readOp.gate_leakage)/clock_frequency;
      
      /* This part works different from the McPAT. */
      /* x86 instruction decoder leakage does not scale correctly. */
      if(core_p.x86)
        unit_energy.leakage *= (area_scaling/4.0);
      break;
    }
    case INTERCONNECT:
    {
      unit_energy.read = McPAT_interconnect->power.readOp.dynamic+McPAT_interconnect->power.readOp.short_circuit;
      unit_energy.write = unit_energy.read;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_interconnect->power.readOp.longer_channel_leakage+McPAT_interconnect->power.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_interconnect->power.readOp.leakage+McPAT_interconnect->power.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case MEMORY_CONTROLLER:
    {
      switch(energy_submodel)
      {
        case MEMORY_CONTROLLER_FRONTEND:
        {
          McPAT_MCFrontEnd->computeEnergy(true); // lekaage is calculated here
          
          McPAT_MCFrontEnd->mcp.executionTime = (double)1.0/McPAT_MCFrontEnd->mcp.clockRate; // single clock time
          
          // separate calculation for read and write energies
          McPAT_MCFrontEnd->XML->sys.mc.memory_reads = 1;
          McPAT_MCFrontEnd->XML->sys.mc.memory_writes = 0;
          McPAT_MCFrontEnd->computeEnergy(false);
          unit_energy.read = McPAT_MCFrontEnd->rt_power.readOp.dynamic+McPAT_MCFrontEnd->rt_power.readOp.short_circuit;
          
          McPAT_MCFrontEnd->XML->sys.mc.memory_reads = 0;
          McPAT_MCFrontEnd->XML->sys.mc.memory_writes = 1;
          McPAT_MCFrontEnd->computeEnergy(false);
          
          unit_energy.write = McPAT_MCFrontEnd->rt_power.readOp.dynamic+McPAT_MCFrontEnd->rt_power.readOp.short_circuit; // McPAT MC has no power.writeOp
          
          if(XML_interface.sys.longer_channel_device)
            unit_energy.leakage = (McPAT_MCFrontEnd->rt_power.readOp.longer_channel_leakage+McPAT_MCFrontEnd->rt_power.readOp.gate_leakage)/clock_frequency;
          else
            unit_energy.leakage = (McPAT_MCFrontEnd->rt_power.readOp.leakage+McPAT_MCFrontEnd->rt_power.readOp.gate_leakage)/clock_frequency;
          break;
        }
        case MEMORY_CONTROLLER_BACKEND:
        { 
          McPAT_MCBackend->computeEnergy(true); // leakage is calculated here
          
          McPAT_MCBackend->mcp.executionTime = (double)1.0/McPAT_MCBackend->mcp.clockRate; // single clock time
          
          // separate calculation for read and write energies
          McPAT_MCBackend->mcp.reads = 1;
          McPAT_MCBackend->mcp.writes = 0;
          McPAT_MCBackend->computeEnergy(false);
          unit_energy.read = McPAT_MCBackend->rt_power.readOp.dynamic+McPAT_MCBackend->rt_power.readOp.short_circuit;
          
          McPAT_MCBackend->mcp.reads = 0;
          McPAT_MCBackend->mcp.writes = 1;
          McPAT_MCBackend->computeEnergy(false);
          unit_energy.write = McPAT_MCBackend->rt_power.readOp.dynamic+McPAT_MCBackend->rt_power.readOp.short_circuit; // McPAT MC has no power.writeOp
          
          if(XML_interface.sys.longer_channel_device)
            unit_energy.leakage = (McPAT_MCBackend->rt_power.readOp.longer_channel_leakage+McPAT_MCBackend->rt_power.readOp.gate_leakage)/clock_frequency;
          else
            unit_energy.leakage = (McPAT_MCBackend->rt_power.readOp.leakage+McPAT_MCBackend->rt_power.readOp.gate_leakage)/clock_frequency;
          break;
        }
        case MEMORY_CONTROLLER_PHY:
        {
          McPAT_MCPHY->computeEnergy(true); // leakage is calculated here
          
          McPAT_MCPHY->mcp.executionTime = (double)1.0/McPAT_MCPHY->mcp.clockRate; // single clock time
          
          // separate calculation for read and write energies
          McPAT_MCPHY->mcp.reads = 1;
          McPAT_MCPHY->mcp.writes = 0;
          McPAT_MCPHY->computeEnergy(false);
          unit_energy.read = McPAT_MCPHY->rt_power.readOp.dynamic+McPAT_MCPHY->rt_power.readOp.short_circuit;
          
          McPAT_MCPHY->mcp.reads = 0;
          McPAT_MCPHY->mcp.writes = 1;
          McPAT_MCPHY->computeEnergy(false);
          unit_energy.write = McPAT_MCPHY->rt_power.readOp.dynamic+McPAT_MCPHY->rt_power.readOp.short_circuit; // McPAT MC has no power.writeOp
          
          if(XML_interface.sys.longer_channel_device)
            unit_energy.leakage = (McPAT_MCPHY->rt_power.readOp.longer_channel_leakage+McPAT_MCPHY->rt_power.readOp.gate_leakage)/clock_frequency;
          else
            unit_energy.leakage = (McPAT_MCPHY->rt_power.readOp.leakage+McPAT_MCPHY->rt_power.readOp.gate_leakage)/clock_frequency;
          break;
        }
      }
      break;
    }
    case NETWORK:
    {
      McPAT_NoC->computeEnergy(true); // leakage is calculated here
      
      McPAT_NoC->XML->sys.NoC[0].total_accesses = 1;
      McPAT_NoC->computeEnergy(false);
      unit_energy.read = McPAT_NoC->rt_power.readOp.dynamic+McPAT_NoC->rt_power.readOp.short_circuit;
      unit_energy.write = unit_energy.read;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_NoC->rt_power.readOp.longer_channel_leakage+McPAT_NoC->rt_power.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_NoC->rt_power.readOp.leakage+McPAT_NoC->rt_power.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case NIU_CONTROLLER:
    {
      unit_energy.read = unit_energy.write = McPAT_NIUController->power_t.readOp.dynamic;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_NIUController->power_t.readOp.longer_channel_leakage+McPAT_NIUController->power_t.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_NIUController->power_t.readOp.leakage+McPAT_NIUController->power_t.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case PCIE_CONTROLLER:
    {
      unit_energy.read = unit_energy.write = McPAT_PCIeController->power_t.readOp.dynamic;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_PCIeController->power_t.readOp.longer_channel_leakage+McPAT_PCIeController->power_t.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_PCIeController->power_t.readOp.leakage+McPAT_PCIeController->power_t.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case PIPELINE:
    {
      unit_energy.read = McPAT_Pipeline->power.readOp.dynamic+McPAT_Pipeline->power.readOp.short_circuit;
      unit_energy.write = unit_energy.read;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_Pipeline->power.readOp.longer_channel_leakage+McPAT_Pipeline->power.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_Pipeline->power.readOp.leakage+McPAT_Pipeline->power.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case SELECTION_LOGIC:
    {
      unit_energy.read = McPAT_selection_logic->power.readOp.dynamic+McPAT_selection_logic->power.readOp.short_circuit;
      unit_energy.write = unit_energy.read;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_selection_logic->power.readOp.longer_channel_leakage+McPAT_selection_logic->power.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_selection_logic->power.readOp.leakage+McPAT_selection_logic->power.readOp.gate_leakage)/clock_frequency;
      break;
    }
    case UNDIFFERENTIATED_CORE:
    {
      unit_energy.read = McPAT_UndiffCore->power.readOp.dynamic+McPAT_UndiffCore->power.readOp.short_circuit;
      unit_energy.write = unit_energy.read;
      
      if(XML_interface.sys.longer_channel_device)
        unit_energy.leakage = (McPAT_UndiffCore->power.readOp.longer_channel_leakage+McPAT_UndiffCore->power.readOp.gate_leakage)/clock_frequency;
      else
        unit_energy.leakage = (McPAT_UndiffCore->power.readOp.leakage+McPAT_UndiffCore->power.readOp.gate_leakage)/clock_frequency;
      break;
    }
    default:
    {
      EI_WARNING("McPAT","unknown energy_model in get_unit_energy()");
      break;
    }
  }
  
  unit_energy.baseline *= energy_scaling;
  unit_energy.read *= energy_scaling;
  unit_energy.read_tag *= energy_scaling;
  unit_energy.write *= energy_scaling;
  unit_energy.write_tag *= energy_scaling;
  unit_energy.leakage *= (energy_scaling*scaling);

  return unit_energy;
}


power_t ENERGYLIB_MCPAT::get_tdp_power(void)
{
  energy_t unit_energy = get_unit_energy();
  energy_t tdp_energy;
  power_t tdp_power;

  switch(energy_model)
  {
    case NO_ENERGY_MODEL:
      break;
    case ARRAY:
    {
      tdp_energy.read = unit_energy.read*(McPAT_ArrayST->l_ip.num_rd_ports?McPAT_ArrayST->l_ip.num_rd_ports:McPAT_ArrayST->l_ip.num_rw_ports);
      tdp_energy.write = unit_energy.write*(McPAT_ArrayST->l_ip.num_wr_ports?McPAT_ArrayST->l_ip.num_wr_ports:McPAT_ArrayST->l_ip.num_rw_ports);
      
      if(McPAT_ArrayST->l_ip.num_search_ports)
        tdp_energy.search = unit_energy.search*(McPAT_ArrayST->l_ip.num_search_ports);
      break;
    }
    case DEPENDENCY_CHECK_LOGIC:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?McPAT_dep_resource_conflict_check->coredynp.decodeW:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:McPAT_dep_resource_conflict_check->coredynp.decodeW);
      break;
    }
    case FLASH_CONTROLLER:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?1.0:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:1.0);
      break;
    }
    case FUNCTIONAL_UNIT:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?1.0:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:1.0);
      tdp_energy.baseline = unit_energy.baseline*(TDP_duty_cycle.read>0.0?TDP_duty_cycle.read:TDP_duty_cycle.write);
      break;
    }
    case INSTRUCTION_DECODER:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?1.0:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:1.0);
      
      if(core_p.x86)
      {
        tdp_energy.read *= area_scaling;
        tdp_energy.write *= area_scaling;
      }
      break;
    }
    case INTERCONNECT:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?1.0:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:1.0);
      break;
    }
    case MEMORY_CONTROLLER:
    {
      switch(energy_submodel)
      {
        case MEMORY_CONTROLLER_FRONTEND:
        {
          McPAT_MCFrontEnd->computeEnergy(true);
          tdp_energy.read = McPAT_MCFrontEnd->power.readOp.dynamic;
          break;
        }
        case MEMORY_CONTROLLER_BACKEND:
        {
          McPAT_MCBackend->computeEnergy(true);
          tdp_energy.read = McPAT_MCBackend->power.readOp.dynamic;
          break;
        }
        case MEMORY_CONTROLLER_PHY:
        {
          McPAT_MCPHY->computeEnergy(true);
          tdp_energy.read = McPAT_MCPHY->power.readOp.dynamic;
          break;
        }
        default:
        {
          EI_WARNING("McPAT","unknown energy_submodel in get_tdp_power()");
          break;
        }
      }
      break;
    }
    case NETWORK:
    {
      McPAT_NoC->computeEnergy(true);
      tdp_energy.read = McPAT_NoC->power.readOp.dynamic;
      break;
    }
    case NIU_CONTROLLER:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?1.0:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:1.0);
      break;
    }
    case PCIE_CONTROLLER:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?1.0:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:1.0);
      break;
    }
    case PIPELINE:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?1.0:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:1.0);
      break;
    }
    case SELECTION_LOGIC:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?McPAT_selection_logic->issue_width:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:McPAT_selection_logic->issue_width);
      break;
    }
    case UNDIFFERENTIATED_CORE:
    {
      tdp_energy.read = unit_energy.read*(TDP_duty_cycle.read>0.0?1.0:0.0);
      tdp_energy.write = unit_energy.write*(TDP_duty_cycle.read>0.0?0.0:1.0);
      break;
    }
    default:
    {
      EI_WARNING("McPAT","unknown energy_model in get_tdp_power()");
      break;
    }
  }

  tdp_energy.leakage = unit_energy.leakage;
  tdp_energy.read *= (TDP_duty_cycle.read*scaling);
  tdp_energy.write *= (TDP_duty_cycle.write*scaling);
  tdp_energy.read_tag *= (TDP_duty_cycle.read_tag*scaling);
  tdp_energy.write_tag *= (TDP_duty_cycle.write_tag*scaling);
  tdp_energy.search *= (TDP_duty_cycle.search*scaling);
  tdp_energy.total = tdp_energy.get_total();
  tdp_power = tdp_energy*clock_frequency;

  return tdp_power;
}


power_t ENERGYLIB_MCPAT::get_runtime_power(double time_tick, double period, counters_t counters)
{
  energy_t unit_energy = get_unit_energy();
  energy_t runtime_energy;
  power_t runtime_power;

//energy.baseline = unit_energy.baseline*period*module_it->second.energy_library->clock_frequency;
  runtime_energy.baseline = unit_energy.baseline*(double)(counters.read+counters.write);
  runtime_energy.search = unit_energy.search*(double)counters.search;
  runtime_energy.read = unit_energy.read*(double)counters.read;
  runtime_energy.write = unit_energy.write*(double)counters.write;
  runtime_energy.read_tag = unit_energy.read_tag*(double)counters.read_tag;
  runtime_energy.write_tag = unit_energy.write_tag*(double)counters.write_tag;
  runtime_energy.leakage = unit_energy.leakage*period*clock_frequency;
  runtime_energy.total = runtime_energy.get_total();
  runtime_power = runtime_energy*(1.0/period); // conversion from energy to power

  return runtime_power;
}

double ENERGYLIB_MCPAT::get_leakage_power(double time_tick, double period)
{
  energy_t unit_energy = get_unit_energy();
  double leakage_power;

  leakage_power = unit_energy.leakage*clock_frequency;

  return leakage_power;
}


double ENERGYLIB_MCPAT::get_area(void)
{
  double area = 0.0;
  
  switch(energy_model)
  {
    case NO_ENERGY_MODEL:
      break;
    case ARRAY:
    {
      area = McPAT_ArrayST->local_result.area;
      break;
    }
    case DEPENDENCY_CHECK_LOGIC:
    {
      area = McPAT_dep_resource_conflict_check->area.get_area();
      break;
    }
    case FLASH_CONTROLLER:
    {
      area = McPAT_FlashController->area.get_area();
      break;
    }
    case FUNCTIONAL_UNIT:
    {
      area = McPAT_FunctionalUnit->area.get_area();
      // McPAT Functional Unit model is awful
      if((energy_submodel == FUNCTIONAL_UNIT_ALU)||(energy_submodel == FUNCTIONAL_UNIT_MUL))
        area *= scaling;
      break;
    }
    case INSTRUCTION_DECODER:
    {
      area = McPAT_inst_decoder->area.get_area();
      break;
    }
    case INTERCONNECT:
    {
      area = McPAT_interconnect->area.get_area();
      break;
    }
    case MEMORY_CONTROLLER:
    {
      switch(energy_submodel)
      {
        case MEMORY_CONTROLLER_FRONTEND:
        {
          area = McPAT_MCFrontEnd->area.get_area();
          break;
        }
        case MEMORY_CONTROLLER_BACKEND:
        {
          area = McPAT_MCBackend->area.get_area();
          break;
        }
        case MEMORY_CONTROLLER_PHY:
        {
          area = McPAT_MCPHY->area.get_area();
          break;
        }
        default:
        {
          EI_WARNING("McPAT","unknown energy_submodel in get_area()");
          break;
        }
      }
      break;
    }
    case NETWORK:
    {
      area = McPAT_NoC->area.get_area();
      break;
    }
    case NIU_CONTROLLER:
    {
      area = McPAT_NIUController->area.get_area();
      break;
    }
    case PCIE_CONTROLLER:
    {
      area = McPAT_PCIeController->area.get_area();
      break;
    }
    case PIPELINE:
    {
      area = McPAT_Pipeline->area.get_area();
      break;
    }
    case SELECTION_LOGIC:
    {
      area = McPAT_selection_logic->area.get_area();
      break;
    }
    case UNDIFFERENTIATED_CORE:
    {
      area = McPAT_UndiffCore->area.get_area();
      break;
    }
    default:
    {
      EI_WARNING("McPAT","unknown energy_model in get_area()");
      break;
    }
  }

  return area*area_scaling*scaling*1e-12; // um^2 to m^2
}

void ENERGYLIB_MCPAT::update_variable(string variable, void *value)
{
  if(!stricmp(variable,"temperature"))
  {
    double temperature = *(double*)value;
    if (temperature > 400) temperature = 400;
    if((temperature < 273.0)||(temperature > 420.0))
    {
      char buf[8];
      sprintf(buf,"%3.2lf",temperature);
      EI_ERROR("McPAT","invalid temperature "+string(buf)+" in update_energy()");
    }

    input_p.temp = (unsigned int)temperature;

    init_interface(&input_p); // reset g_tp

    // update leakage current
#if 0
    tech_p.peri_global.I_off_n = g_tp.peri_global.I_off_n;
    tech_p.peri_global.I_off_p = g_tp.peri_global.I_off_p;
    tech_p.peri_global.I_g_on_n = g_tp.peri_global.I_g_on_n;
    tech_p.peri_global.I_g_on_p = g_tp.peri_global.I_g_on_p;
    tech_p.sram_cell.I_off_n = g_tp.sram_cell.I_off_n;
    tech_p.sram_cell.I_off_p = g_tp.sram_cell.I_off_p;
    tech_p.sram_cell.I_g_on_n = g_tp.sram_cell.I_g_on_n;
    tech_p.sram_cell.I_g_on_p = g_tp.sram_cell.I_g_on_p;
    tech_p.dram_wl.I_off_n = g_tp.dram_wl.I_off_n;
    tech_p.dram_wl.I_off_p = g_tp.dram_wl.I_off_p;
    tech_p.cam_cell.I_off_n = g_tp.cam_cell.I_off_n;
    tech_p.cam_cell.I_off_p = g_tp.cam_cell.I_off_p;
    tech_p.cam_cell.I_g_on_n = g_tp.cam_cell.I_g_on_n;
    tech_p.cam_cell.I_g_on_p = g_tp.cam_cell.I_g_on_p;
#endif
tech_p.peri_global.I_off_n = g_tp.peri_global.I_off_n*(tech_p.peri_global.Vdd-tech_p.peri_global.Vth)/(g_tp.peri_global.Vdd-g_tp.peri_global.Vth);
tech_p.peri_global.I_off_p = g_tp.peri_global.I_off_p*(tech_p.peri_global.Vdd-tech_p.peri_global.Vth)/(g_tp.peri_global.Vdd-g_tp.peri_global.Vth);
tech_p.peri_global.I_g_on_n = g_tp.peri_global.I_g_on_n*(tech_p.peri_global.Vdd-tech_p.peri_global.Vth)/(g_tp.peri_global.Vdd-g_tp.peri_global.Vth);
tech_p.peri_global.I_g_on_p = g_tp.peri_global.I_g_on_p*(tech_p.peri_global.Vdd-tech_p.peri_global.Vth)/(g_tp.peri_global.Vdd-g_tp.peri_global.Vth);
tech_p.sram_cell.I_off_n = g_tp.sram_cell.I_off_n*(tech_p.sram_cell.Vdd-tech_p.sram_cell.Vth)/(g_tp.sram_cell.Vdd-g_tp.sram_cell.Vth);
tech_p.sram_cell.I_off_p = g_tp.sram_cell.I_off_p*(tech_p.sram_cell.Vdd-tech_p.sram_cell.Vth)/(g_tp.sram_cell.Vdd-g_tp.sram_cell.Vth);
tech_p.sram_cell.I_g_on_n = g_tp.sram_cell.I_g_on_n*(tech_p.sram_cell.Vdd-tech_p.sram_cell.Vth)/(g_tp.sram_cell.Vdd-g_tp.sram_cell.Vth);
tech_p.sram_cell.I_g_on_p = g_tp.sram_cell.I_g_on_p*(tech_p.sram_cell.Vdd-tech_p.sram_cell.Vth)/(g_tp.sram_cell.Vdd-g_tp.sram_cell.Vth);
tech_p.dram_wl.I_off_n = g_tp.dram_wl.I_off_n*(tech_p.dram_cell_Vdd-tech_p.dram_acc.Vth)/(g_tp.dram_cell_Vdd-g_tp.dram_acc.Vth);
tech_p.dram_wl.I_off_p = g_tp.dram_wl.I_off_p*(tech_p.dram_cell_Vdd-tech_p.dram_acc.Vth)/(g_tp.dram_cell_Vdd-g_tp.dram_acc.Vth);
tech_p.cam_cell.I_off_n = g_tp.cam_cell.I_off_n*(tech_p.cam_cell.Vdd-tech_p.cam_cell.Vth)/(g_tp.cam_cell.Vdd-g_tp.cam_cell.Vth);
tech_p.cam_cell.I_off_p = g_tp.cam_cell.I_off_p*(tech_p.cam_cell.Vdd-tech_p.cam_cell.Vth)/(g_tp.cam_cell.Vdd-g_tp.cam_cell.Vth);
tech_p.cam_cell.I_g_on_n = g_tp.cam_cell.I_g_on_n*(tech_p.cam_cell.Vdd-tech_p.cam_cell.Vth)/(g_tp.cam_cell.Vdd-g_tp.cam_cell.Vth);
tech_p.cam_cell.I_g_on_p = g_tp.cam_cell.I_g_on_p*(tech_p.cam_cell.Vdd-tech_p.cam_cell.Vth)/(g_tp.cam_cell.Vdd-g_tp.cam_cell.Vth);
  }
  else if(!stricmp(variable,"frequency"))
  {
    clock_frequency = *(double*)value;

    switch(energy_model)
    {
      case NO_ENERGY_MODEL:
        break;
      case ARRAY:
      {
        // nothing to do
        break;
      }
      case DEPENDENCY_CHECK_LOGIC:
      {
        McPAT_dep_resource_conflict_check->coredynp.clockRate = clock_frequency;
        break;
      }
      case FLASH_CONTROLLER:
      {
        McPAT_FlashController->fcp.clockRate = clock_frequency;
        break;
      }
      case FUNCTIONAL_UNIT:
      {
        McPAT_FunctionalUnit->clockRate = McPAT_FunctionalUnit->coredynp.clockRate = clock_frequency;
        break;
      }
      case INSTRUCTION_DECODER:
      {
        // nothing to do
        break;
      }
      case INTERCONNECT:
      {
        // nothing to do
        break;
      }
      case MEMORY_CONTROLLER:
      {
        clock_frequency *= 2; // DDR
        switch(energy_submodel)
        {
          case MEMORY_CONTROLLER_FRONTEND:
          {
            McPAT_MCFrontEnd->mcp.clockRate = clock_frequency;
            break;
          }
          case MEMORY_CONTROLLER_BACKEND:
          {
            McPAT_MCBackend->mcp.clockRate = clock_frequency;
            break;
          }
          case MEMORY_CONTROLLER_PHY:
          {
            McPAT_MCPHY->mcp.clockRate = clock_frequency;
            break;
          }
          default:
          {
            EI_WARNING("McPAT","unknown energy_submodel in update_variable()");
            break;
          }
        }
        break;
      }
      case NETWORK:
      {
        McPAT_NoC->nocdynp.clockRate = clock_frequency;
        break;
      }
      case NIU_CONTROLLER:
      {
        McPAT_NIUController->niup.clockRate = clock_frequency;
        break;
      }
      case PCIE_CONTROLLER:
      {
        McPAT_PCIeController->pciep.clockRate = clock_frequency;
        break;
      }
      case PIPELINE:
      {
        McPAT_Pipeline->coredynp.clockRate = clock_frequency;
        break;
      }
      case SELECTION_LOGIC:
      {
        break;
      }
      case UNDIFFERENTIATED_CORE:
      {
        McPAT_UndiffCore->coredynp.clockRate = clock_frequency;
        break;
      }
      default:
      {
        EI_WARNING("McPAT","unknown energy_model in update_variable()");
        break;
      }
    }
  }
  else if(!stricmp(variable,"vdd")) // TBD -- this should do better than updating all tech flavors' Vdd
  {
    double vdd = *(double*)value;
    // McPAT library should be using one of the following tech types
    tech_p.cam_cell.R_nch_on *= (vdd/tech_p.cam_cell.Vdd);
    tech_p.cam_cell.R_pch_on *= (vdd/tech_p.cam_cell.Vdd);
    tech_p.cam_cell.Vdd = vdd;
    tech_p.sram_cell.R_nch_on *= (vdd/tech_p.sram_cell.Vdd);
    tech_p.sram_cell.R_pch_on *= (vdd/tech_p.sram_cell.Vdd);
    tech_p.sram_cell.Vdd = vdd;
    tech_p.peri_global.R_nch_on *= (vdd/tech_p.peri_global.Vdd);
    tech_p.peri_global.R_pch_on *= (vdd/tech_p.peri_global.Vdd);
    tech_p.peri_global.Vdd = vdd;
    
    tech_p.dram_cell_Vdd = vdd;    
  }
  else if(!stricmp(variable,"vpp"))
  {
    double vpp = *(double*)value;

    tech_p.dram_wl.R_nch_on *= (vpp/tech_p.vpp);
    tech_p.dram_wl.R_pch_on *= (vpp/tech_p.vpp);    
    tech_p.vpp = vpp;
  }
  else
    EI_ERROR("McPAT","updating unknown varaible "+name+" in update_variable()");

  switch(energy_model)
  {
    case NO_ENERGY_MODEL:
      break;
    case ARRAY:
    {
      McPAT_ArrayST->update_energy(input_p,tech_p);
      break;
    }
    case DEPENDENCY_CHECK_LOGIC:
    {
      McPAT_dep_resource_conflict_check->update_energy(input_p,tech_p);
      break;
    }
    case FLASH_CONTROLLER:
    {
      McPAT_FlashController->update_energy(input_p,tech_p);
      break;
    }
    case FUNCTIONAL_UNIT:
    {
      McPAT_FunctionalUnit->update_energy(input_p,tech_p);
      break;
    }
    case INSTRUCTION_DECODER:
    {
      McPAT_inst_decoder->update_energy(input_p,tech_p);
      break;
    }
    case INTERCONNECT:
    {
      McPAT_interconnect->update_energy(input_p,tech_p);
      break;
    }
    case MEMORY_CONTROLLER:
    {
      switch(energy_submodel)
      {
        case MEMORY_CONTROLLER_FRONTEND:
        {
          McPAT_MCFrontEnd->update_energy(input_p,tech_p);
          break;
        }
        case MEMORY_CONTROLLER_BACKEND:
        {
          McPAT_MCBackend->update_energy(input_p,tech_p);
          break;
        }
        case MEMORY_CONTROLLER_PHY:
        {
          McPAT_MCPHY->update_energy(input_p,tech_p);
          break;
        }
        default:
        {
          EI_WARNING("McPAT","unknown energy_submodel in update_variable()");
          break;
        }
      }
      break;
    }
    case NETWORK:
    {
      McPAT_NoC->update_energy(input_p,tech_p);
      break;
    }
    case NIU_CONTROLLER:
    {
      McPAT_NIUController->update_energy(input_p,tech_p);
      break;
    }
    case PCIE_CONTROLLER:
    {
      McPAT_PCIeController->update_energy(input_p,tech_p);
      break;
    }
    case PIPELINE:
    {
      McPAT_Pipeline->update_energy(input_p,tech_p);
      break;
    }
    case SELECTION_LOGIC:
    {
      McPAT_selection_logic->update_energy(input_p,tech_p);
      break;
    }
    case UNDIFFERENTIATED_CORE:
    {
      McPAT_UndiffCore->update_energy(input_p,tech_p);
      break;
    }
    default:
    {
      EI_WARNING("McPAT","unknown energy_model in update_variable()");
      break;
    }
  }
    
  tech_p = g_tp;
  input_p = *g_ip;
}

void ENERGYLIB_MCPAT::EI_module_config(FILE *McPAT_config, ENERGYLIB_MCPAT *McPAT_obj, int ref_idx, int idx,\
double TDP_read_cycle, double TDP_write_cycle, double TDP_search_cycle, double TDP_read_tag_cycle, double TDP_write_tag_cycle)
{
  if(!McPAT_config) return;
  
  if(McPAT_obj->input_p.obj_func_dyn_energy != 0) // 0 by default
    fprintf(McPAT_config,"-module.obj_func_dyn_energy\t\t%u\n",McPAT_obj->input_p.obj_func_dyn_energy);
  if(McPAT_obj->input_p.obj_func_dyn_power != 0)
    fprintf(McPAT_config,"-module.obj_func_dyn_power\t\t%u\n",McPAT_obj->input_p.obj_func_dyn_power);
  if(McPAT_obj->input_p.obj_func_leak_power != 0)
    fprintf(McPAT_config,"-module.obj_func_leak_power\t\t%u\n",McPAT_obj->input_p.obj_func_leak_power);
  if(McPAT_obj->input_p.obj_func_cycle_t != 1)
    fprintf(McPAT_config,"-module.obj_func_cycle_t\t\t%u\n",McPAT_obj->input_p.obj_func_cycle_t);
  if(McPAT_obj->input_p.delay_wt != 100)
    fprintf(McPAT_config,"-module.delay_wt\t\t%d\n",McPAT_obj->input_p.delay_wt);
  if(McPAT_obj->input_p.area_wt != 0)
    fprintf(McPAT_config,"-module.area_wt\t\t%d\n",McPAT_obj->input_p.area_wt);
  if(McPAT_obj->input_p.dynamic_power_wt != 100)
    fprintf(McPAT_config,"-module.dynamic_power_wt\t\t%d\n",McPAT_obj->input_p.dynamic_power_wt);
  if(McPAT_obj->input_p.leakage_power_wt != 0)
    fprintf(McPAT_config,"-module.leakage_power_wt\t\t%d\n",McPAT_obj->input_p.leakage_power_wt);
  if(McPAT_obj->input_p.cycle_time_wt != 0)
    fprintf(McPAT_config,"-module.cycle_time_wt\t\t%d\n",McPAT_obj->input_p.cycle_time_wt);
  if(McPAT_obj->input_p.delay_dev != 10000)
    fprintf(McPAT_config,"-module.delay_dev\t\t%d\n",McPAT_obj->input_p.delay_dev);
  if(McPAT_obj->input_p.area_dev != 10000)
    fprintf(McPAT_config,"-module.area_dev\t\t%d\n",McPAT_obj->input_p.area_dev);
  if(McPAT_obj->input_p.dynamic_power_dev != 10000)
    fprintf(McPAT_config,"-module.dynamic_power_dev\t\t%d\n",McPAT_obj->input_p.dynamic_power_dev);
  if(McPAT_obj->input_p.leakage_power_dev != 10000)
    fprintf(McPAT_config,"-module.leakage_power_dev\t\t%d\n",McPAT_obj->input_p.leakage_power_dev);
  if(McPAT_obj->input_p.cycle_time_dev != 10000)
    fprintf(McPAT_config,"-module.cycle_time_dev\t\t%d\n",McPAT_obj->input_p.cycle_time_dev);
  if(McPAT_obj->input_p.ed != 2)
    fprintf(McPAT_config,"-module.ed\t\t%d\n",McPAT_obj->input_p.ed);
  if(McPAT_obj->input_p.nuca != 0)
  {
    fprintf(McPAT_config,"-module.nuca\t\t%d\n",McPAT_obj->input_p.nuca);
    if(McPAT_obj->input_p.nuca_bank_count != 0)
      fprintf(McPAT_config,"-module.nuca_bank_count\t\t%d\n",McPAT_obj->input_p.nuca_bank_count);
    if(McPAT_obj->input_p.delay_wt_nuca != 0)
      fprintf(McPAT_config,"-module.delay_wt_nuca\t\t%d\n",McPAT_obj->input_p.delay_wt_nuca);
    if(McPAT_obj->input_p.area_wt_nuca != 0)
      fprintf(McPAT_config,"-module.area_wt_nuca\t\t%d\n",McPAT_obj->input_p.area_wt_nuca);
    if(McPAT_obj->input_p.dynamic_power_wt_nuca != 0)
      fprintf(McPAT_config,"-module.dynamic_power_wt_nuca\t\t%d\n",McPAT_obj->input_p.dynamic_power_wt_nuca);
    if(McPAT_obj->input_p.leakage_power_wt_nuca != 0)
      fprintf(McPAT_config,"-module.leakage_power_wt_nuca\t\t%d\n",McPAT_obj->input_p.leakage_power_wt_nuca);
    if(McPAT_obj->input_p.cycle_time_wt_nuca != 0)
      fprintf(McPAT_config,"-module.cycle_time_wt_nuca\t\t%d\n",McPAT_obj->input_p.cycle_time_wt_nuca);
    if(McPAT_obj->input_p.delay_dev_nuca != 10000)
      fprintf(McPAT_config,"-module.delay_dev_nuca\t\t%d\n",McPAT_obj->input_p.delay_dev_nuca);
    if(McPAT_obj->input_p.area_dev_nuca != 10000)
      fprintf(McPAT_config,"-module.area_dev_nuca\t\t%d\n",McPAT_obj->input_p.area_dev_nuca);
    if(McPAT_obj->input_p.dynamic_power_dev_nuca != 10)
      fprintf(McPAT_config,"-module.dynamic_power_dev_nuca\t\t%d\n",McPAT_obj->input_p.dynamic_power_dev_nuca);
    if(McPAT_obj->input_p.leakage_power_dev_nuca != 10000)
      fprintf(McPAT_config,"-module.leakage_power_dev_nuca\t\t%d\n",McPAT_obj->input_p.leakage_power_dev_nuca);
    if(McPAT_obj->input_p.cycle_time_dev_nuca != 10000)
      fprintf(McPAT_config,"-module.cycle_time_dev_nuca\t\t%d\n",McPAT_obj->input_p.cycle_time_dev_nuca);
  }
  if(McPAT_obj->input_p.force_wiretype) // false by default
    fprintf(McPAT_config,"-module.force_wire_type\t\ttrue\n");
  if(!McPAT_obj->input_p.rpters_in_htree) // true by default
    fprintf(McPAT_config,"-module.repeaters_in_htree\t\tfalse\n");
  if(!McPAT_obj->input_p.with_clock_grid) // true by default
    fprintf(McPAT_config,"-module.with_clock_grid\t\tfalse\n");
  if(McPAT_obj->input_p.force_cache_config) // false by default
  {
    fprintf(McPAT_config,"-module.force_cache_config\t\ttrue\n");
  
    fprintf(McPAT_config,"-module.ndbl\t\t%d\n",McPAT_obj->input_p.ndbl);
    fprintf(McPAT_config,"-module.ndwl\t\t%d\n",McPAT_obj->input_p.ndwl);
    fprintf(McPAT_config,"-module.nspd\t\t%d\n",McPAT_obj->input_p.nspd);
    fprintf(McPAT_config,"-module.ndsam1\t\t%d\n",McPAT_obj->input_p.ndsam1);
    fprintf(McPAT_config,"-module.ndsam2\t\t%d\n",McPAT_obj->input_p.ndsam2);
    fprintf(McPAT_config,"-module.ndcm\t\t%d\n",McPAT_obj->input_p.ndcm);
  }
  
  // skip printing don't-care parameters
  
  double TDP_duty_cycle = 1.0/McPAT_obj->scaling;
  
  switch(McPAT_obj->energy_model)
  {
    case ARRAY:
    {
      assert(McPAT_obj->McPAT_ArrayST);
      fprintf(McPAT_config,"-module.line_width\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.line_sz);
      fprintf(McPAT_config,"-module.output_width\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.out_w);
      if(McPAT_obj->input_p.assoc != 1)
        fprintf(McPAT_config,"-module.assoc\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.assoc);
      if(McPAT_obj->input_p.nbanks != 1)
        fprintf(McPAT_config,"-module.banks\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.nbanks);
      fprintf(McPAT_config,"-module.size\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.cache_sz);
      if(McPAT_obj->input_p.tag_w > 0)
        fprintf(McPAT_config,"-module.tag_width\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.tag_w);
      if(McPAT_obj->input_p.num_rw_ports > 0)
        fprintf(McPAT_config,"-module.rw_ports\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.num_rw_ports);
      if(McPAT_obj->input_p.num_rd_ports > 0)
        fprintf(McPAT_config,"-module.rd_ports\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.num_rd_ports);
      if(McPAT_obj->input_p.num_wr_ports > 0)
        fprintf(McPAT_config,"-module.wr_ports\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.num_wr_ports);
      if(McPAT_obj->input_p.num_se_rd_ports > 0)
        fprintf(McPAT_config,"-module.se_rd_ports\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.num_se_rd_ports);
      if(McPAT_obj->input_p.num_search_ports > 0)
        fprintf(McPAT_config,"-module.search_ports\t\t%u\n",McPAT_obj->McPAT_ArrayST->l_ip.num_search_ports);
      if(!McPAT_obj->input_p.add_ecc_b_)
        fprintf(McPAT_config,"-module.add_ecc\t\tfalse\n");
      
      int cycle_time = (int)ceil(McPAT_obj->McPAT_ArrayST->l_ip.throughput*McPAT_obj->clock_frequency);
      int access_time = (int)ceil(McPAT_obj->McPAT_ArrayST->l_ip.latency*McPAT_obj->clock_frequency);
      if((cycle_time != 1)||(access_time != 1))
      {
        fprintf(McPAT_config,"-module.cycle_time\t\t%d\n",cycle_time);
        fprintf(McPAT_config,"-module.access_time\t\t%d\n",access_time);
      }
      
      switch(McPAT_obj->McPAT_ArrayST->l_ip.access_mode)
      {
        case 0: fprintf(McPAT_config,"-module.access_mode\t\tnormal\n"); break;
        case 1: fprintf(McPAT_config,"-module.access_mode\t\tsequential\n"); break;
        case 2: fprintf(McPAT_config,"-module.access_mode\t\tfast\n"); break;
        default: fprintf(McPAT_config,"-module.access_mode\t\tnormal\n"); break;
      }
      
      // TDP read duty cycle
      TDP_duty_cycle = 1.0/McPAT_obj->scaling;
      TDP_duty_cycle *= TDP_read_cycle/(McPAT_obj->McPAT_ArrayST->l_ip.num_rd_ports>0?McPAT_obj->McPAT_ArrayST->l_ip.num_rd_ports:McPAT_obj->McPAT_ArrayST->l_ip.num_rw_ports);
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      // TDP write duty cycle
      TDP_duty_cycle = 1.0/McPAT_obj->scaling;
      TDP_duty_cycle *= TDP_write_cycle/(McPAT_obj->McPAT_ArrayST->l_ip.num_wr_ports>0?McPAT_obj->McPAT_ArrayST->l_ip.num_wr_ports:McPAT_obj->McPAT_ArrayST->l_ip.num_rw_ports);
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.write\t\t%lf\n",TDP_duty_cycle);
      // TDP search duty cycle
      if(McPAT_obj->McPAT_ArrayST->l_ip.num_search_ports > 0)
      {
        TDP_duty_cycle = 1.0/McPAT_obj->scaling;
        TDP_duty_cycle *= TDP_search_cycle/McPAT_obj->McPAT_ArrayST->l_ip.num_search_ports;
        if(TDP_duty_cycle != 1.0)
          fprintf(McPAT_config,"-module.TDP_duty_cycle.search\t\t%lf\n",TDP_duty_cycle);
      }
      // TDP read_tag duty cycle
      TDP_duty_cycle = 1.0/McPAT_obj->scaling;
      TDP_duty_cycle *= TDP_read_tag_cycle/(McPAT_obj->McPAT_ArrayST->l_ip.num_rd_ports>0?McPAT_obj->McPAT_ArrayST->l_ip.num_rd_ports:McPAT_obj->McPAT_ArrayST->l_ip.num_rw_ports);
      if(TDP_duty_cycle > 0.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read_tag\t\t%lf\n",TDP_duty_cycle);
      // TDP write_tag duty cycle
      TDP_duty_cycle = 1.0/McPAT_obj->scaling;
      TDP_duty_cycle *= TDP_write_tag_cycle/(McPAT_obj->McPAT_ArrayST->l_ip.num_wr_ports>0?McPAT_obj->McPAT_ArrayST->l_ip.num_wr_ports:McPAT_obj->McPAT_ArrayST->l_ip.num_rw_ports);
      if(TDP_duty_cycle > 0.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.write_tag\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case DEPENDENCY_CHECK_LOGIC:
    {
      assert(McPAT_obj->McPAT_dep_resource_conflict_check);
      fprintf(McPAT_config,"-module.compare_bits\t\t%d # TODO: check logic.cc:225-228\n",McPAT_obj->McPAT_dep_resource_conflict_check->compare_bits-16-8-8);
      fprintf(McPAT_config,"-module.decode_width\t\t%d\n",McPAT_obj->McPAT_dep_resource_conflict_check->coredynp.decodeW);
      
      TDP_duty_cycle *= TDP_read_cycle/McPAT_obj->McPAT_dep_resource_conflict_check->coredynp.decodeW;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case FLASH_CONTROLLER:
    {
      assert(McPAT_obj->McPAT_FlashController);
      fprintf(McPAT_config,"-module.type\t\t%s\n",McPAT_obj->McPAT_FlashController->fcp.type==0?"high_performance":"low_power");
      fprintf(McPAT_config,"-module.load_percentage\t\t%lf # load to bandwidth ratio\n",McPAT_obj->McPAT_FlashController->fcp.perc_load);
      fprintf(McPAT_config,"-module.peak_transfer_rate\t\t%lf # in MBps\n",McPAT_obj->McPAT_FlashController->fcp.peakDataTransferRate);
      if(!McPAT_obj->McPAT_FlashController->fcp.withPHY)
        fprintf(McPAT_config,"-module.withPHY\t\tfalse\n");
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case FUNCTIONAL_UNIT:
    {
      assert(McPAT_obj->McPAT_FunctionalUnit);
      
      fprintf(McPAT_config,"-module.scaling\t\t%lf\n",McPAT_obj->McPAT_FunctionalUnit->num_fu);
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case INSTRUCTION_DECODER:
    {
      assert(McPAT_obj->McPAT_inst_decoder);
      fprintf(McPAT_config,"-module.x86\t\t%s\n",McPAT_obj->McPAT_inst_decoder->x86?"true":"false");
      fprintf(McPAT_config,"-module.opcode\t\t%d\n",McPAT_obj->McPAT_inst_decoder->opcode_length);
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case INTERCONNECT:
    {
      assert(McPAT_obj->McPAT_interconnect);
      if((bool)McPAT_obj->McPAT_interconnect->pipelinable)
      {
        fprintf(McPAT_config,"-module.pipelinable\t\ttrue\n");
        fprintf(McPAT_config,"-module.routing_over_percentage\t\t%lf\n",McPAT_obj->McPAT_interconnect->route_over_perc);
      }
      if(McPAT_obj->McPAT_interconnect->opt_local)
        fprintf(McPAT_config,"-module.opt_local\t\ttrue\n");
      if(McPAT_obj->McPAT_interconnect->l_ip.wt != (Wire_type)Global)
      {
        switch(McPAT_obj->McPAT_interconnect->l_ip.wt)
        {
          case (Wire_type)Global_5: fprintf(McPAT_config,"-module.wire_type\t\tglobal_5\n"); break;
          case (Wire_type)Global_10: fprintf(McPAT_config,"-module.wire_type\t\tglobal_10\n"); break;
          case (Wire_type)Global_20: fprintf(McPAT_config,"-module.wire_type\t\tglobal_20\n"); break;
          case (Wire_type)Global_30: fprintf(McPAT_config,"-module.wire_type\t\tglobal_30\n"); break;
          case (Wire_type)Low_swing: fprintf(McPAT_config,"-module.wire_type\t\tlow_swing\n"); break;
          case (Wire_type)Semi_global: fprintf(McPAT_config,"-module.wire_type\t\tsemi_global\n"); break;
          case (Wire_type)Transmission: fprintf(McPAT_config,"-module.wire_type\t\ttransmission\n"); break;
          case (Wire_type)Optical: fprintf(McPAT_config,"-module.wire_type\t\toptical\n"); break;
          default: fprintf(McPAT_config,"-module.wire_type\t\tglobal\n"); break;
        }
      }
      fprintf(McPAT_config,"-module.data\t\t%d\n",McPAT_obj->McPAT_interconnect->data_width);
      fprintf(McPAT_config,"-module.wire_length\t\t%lf # if wire_length is unknown list the connected modules (e.g., -module.connect icache)\n",McPAT_obj->McPAT_interconnect->length*1e-6);
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case MEMORY_CONTROLLER:
    {
      switch(McPAT_obj->energy_submodel)
      {
        case MEMORY_CONTROLLER_FRONTEND:
        {
          assert(McPAT_obj->McPAT_MCFrontEnd);
          fprintf(McPAT_config,"-module.line_width\t\t%d # LLC line width\n",McPAT_obj->McPAT_MCFrontEnd->mcp.llcBlockSize*8/9);
          fprintf(McPAT_config,"-module.databus_width\t\t%d # data bus width\n",McPAT_obj->McPAT_MCFrontEnd->mcp.dataBusWidth*8/9);
          fprintf(McPAT_config,"-module.addressbus_width\t\t%d # address bus width\n",McPAT_obj->McPAT_MCFrontEnd->mcp.addressBusWidth);
          fprintf(McPAT_config,"-module.memory_channels\t\t%d # memory channels per MC\n",(int)McPAT_obj->McPAT_MCFrontEnd->mcp.num_channels);
          fprintf(McPAT_config,"-module.transfer_rate\t\t%lf # peak transfer rate\n",McPAT_obj->McPAT_MCFrontEnd->mcp.peakDataTransferRate);
          fprintf(McPAT_config,"-module.memory_ranks\t\t%d\n",McPAT_obj->McPAT_MCFrontEnd->mcp.memRank);
          fprintf(McPAT_config,"-module.request_window_size\t\t%d\n",XML_interface.sys.mc.req_window_size_per_channel);
          fprintf(McPAT_config,"-module.IO_buffer_size\t\t%d\n",XML_interface.sys.mc.IO_buffer_size_per_channel);
          if(McPAT_obj->McPAT_MCFrontEnd->mcp.addressBusWidth != XML_interface.sys.physical_address_width)
            fprintf(McPAT_config,"-module.physical_address_width\t\t%d\n",XML_interface.sys.physical_address_width);
          fprintf(McPAT_config,"-module.model\t\t%s\n",McPAT_obj->McPAT_MCFrontEnd->mcp.type==0?"high_performance":"low_power");
          if(!McPAT_obj->McPAT_MCFrontEnd->mcp.LVDS)
            fprintf(McPAT_config,"-module.lvds\t\tfalse\n");
          if(McPAT_obj->McPAT_MCFrontEnd->mcp.withPHY)
            fprintf(McPAT_config,"-module.with_phy\t\ttrue\n");
          break;
        }
        case MEMORY_CONTROLLER_BACKEND:
        {
          assert(McPAT_obj->McPAT_MCBackend);
          fprintf(McPAT_config,"-module.line_width\t\t%d # LLC line width\n",McPAT_obj->McPAT_MCBackend->mcp.llcBlockSize*8/9);
          fprintf(McPAT_config,"-module.databus_width\t\t%d # data bus width\n",McPAT_obj->McPAT_MCBackend->mcp.dataBusWidth*8/9);
          fprintf(McPAT_config,"-module.addressbus_width\t\t%d # address bus width\n",McPAT_obj->McPAT_MCBackend->mcp.addressBusWidth);
          fprintf(McPAT_config,"-module.memory_channels\t\t%d # memory channels per MC\n",(int)McPAT_obj->McPAT_MCBackend->mcp.num_channels);
          fprintf(McPAT_config,"-module.transfer_rate\t\t%lf # peak transfer rate\n",McPAT_obj->McPAT_MCBackend->mcp.peakDataTransferRate);
          fprintf(McPAT_config,"-module.memory_ranks\t\t%d\n",McPAT_obj->McPAT_MCBackend->mcp.memRank);
          fprintf(McPAT_config,"-module.model\t\t%s\n",McPAT_obj->McPAT_MCBackend->mcp.type==0?"high_performance":"low_power");
          if(!McPAT_obj->McPAT_MCBackend->mcp.LVDS)
            fprintf(McPAT_config,"-module.lvds\t\tfalse\n");
          if(McPAT_obj->McPAT_MCBackend->mcp.withPHY)
            fprintf(McPAT_config,"-module.with_phy\t\ttrue\n");
          break;
        }
        case MEMORY_CONTROLLER_PHY:
        {
          assert(McPAT_obj->McPAT_MCPHY);
          fprintf(McPAT_config,"-module.line_width\t\t%d # LLC line width\n",McPAT_obj->McPAT_MCPHY->mcp.llcBlockSize*8/9);
          fprintf(McPAT_config,"-module.databus_width\t\t%d # data bus width\n",McPAT_obj->McPAT_MCPHY->mcp.dataBusWidth*8/9);
          fprintf(McPAT_config,"-module.addressbus_width\t\t%d # address bus width\n",McPAT_obj->McPAT_MCPHY->mcp.addressBusWidth);
          fprintf(McPAT_config,"-module.memory_channels\t\t%d # memory channels per MC\n",(int)McPAT_obj->McPAT_MCPHY->mcp.num_channels);
          fprintf(McPAT_config,"-module.transfer_rate\t\t%lf # peak transfer rate\n",McPAT_obj->McPAT_MCPHY->mcp.peakDataTransferRate);
          fprintf(McPAT_config,"-module.memory_ranks\t\t%d\n",McPAT_obj->McPAT_MCPHY->mcp.memRank);
          fprintf(McPAT_config,"-module.model\t\t%s\n",McPAT_obj->McPAT_MCPHY->mcp.type==0?"high_performance":"low_power");
          if(!McPAT_obj->McPAT_MCPHY->mcp.LVDS)
            fprintf(McPAT_config,"-module.lvds\t\tfalse\n");
          if(McPAT_obj->McPAT_MCPHY->mcp.withPHY)
            fprintf(McPAT_config,"-module.with_phy\t\ttrue\n");
          break;
        }
        default:
        {
          EI_WARNING("McPAT","unknown energy_submodel in EI_module_config()");
          break;
        }
      }
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case NETWORK:
    {
      assert(McPAT_obj->McPAT_NoC);
      
      switch(XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].type)
      {
        case 0: // bus
          fprintf(McPAT_config,"-module.link_throughput\t\t%d\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].link_throughput);
          fprintf(McPAT_config,"-module.link_latency\t\t%d\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].link_latency);
          fprintf(McPAT_config,"-module.chip_coverage\t\t%lf\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].chip_coverage);
          fprintf(McPAT_config,"-module.route_over_percentage\t\t%lf\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].route_over_perc);
          fprintf(McPAT_config,"-module.link_length\t\t%lf\n",McPAT_obj->McPAT_NoC->link_len);
          break;
        case 1: // router
          fprintf(McPAT_config,"-module.input_ports\t\t%d\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].input_ports);
          fprintf(McPAT_config,"-module.output_ports\t\t%d\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].output_ports);
          fprintf(McPAT_config,"-module.virtual_channels\t\t%d\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].virtual_channel_per_port);
          fprintf(McPAT_config,"-module.input_buffer_entries\t\t%d\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].input_buffer_entries_per_vc);
          break;
        default: 
          break;
      }
      
      if(XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.duty_cycle\t\t%lf\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].duty_cycle);
      fprintf(McPAT_config,"-module.flit_bits\t\t%d\n",XML_interface.sys.NoC[McPAT_obj->McPAT_NoC->ithNoC].flit_bits);    
      fprintf(McPAT_config,"-module.traffic_pattern\t\t%lf\n",McPAT_obj->McPAT_NoC->M_traffic_pattern);
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case NIU_CONTROLLER:
    {
      assert(McPAT_obj->McPAT_NIUController);
      fprintf(McPAT_config,"-module.type\t\t%s\n",McPAT_obj->McPAT_NIUController->niup.type==0?"high_performance":"low_power");
      fprintf(McPAT_config,"-module.load_percentage\t\t%lf # load to bandwidth ratio\n",McPAT_obj->McPAT_NIUController->niup.perc_load);
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case PCIE_CONTROLLER:
    {
      assert(McPAT_obj->McPAT_PCIeController);
      fprintf(McPAT_config,"-module.type\t\t%s\n",McPAT_obj->McPAT_PCIeController->pciep.type==0?"high_performance":"low_power");
      fprintf(McPAT_config,"-module.load_percentage\t\t%lf # load to bandwidth ratio\n",McPAT_obj->McPAT_PCIeController->pciep.perc_load);
      fprintf(McPAT_config,"-module.channels\t\t%d\n",McPAT_obj->McPAT_PCIeController->pciep.num_channels);
      if(!McPAT_obj->McPAT_PCIeController->pciep.withPHY)
        fprintf(McPAT_config,"-module.withPHY\t\tfalse\n");
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case PIPELINE:
    {
      assert(McPAT_obj->McPAT_Pipeline);
      fprintf(McPAT_config,"-module.pipeline_stages\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.pipeline_stages);
      //if(McPAT_obj->McPAT_Pipeline->l_ip.per_stage_vector > 0)
      //fprintf(McPAT_config,"-module.per_stage_vector\t\t%d\n",McPAT_obj->McPAT_Pipeline->l_ip.per_stage_vector);
      if(McPAT_obj->McPAT_Pipeline->coredynp.x86)
      {
        fprintf(McPAT_config,"-module.x86\t\ttrue\n");
        fprintf(McPAT_config,"-module.microopcode\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.micro_opcode_length);
      }
      else
      {
        fprintf(McPAT_config,"-module.x86\t\tfalse\n");
        fprintf(McPAT_config,"-module.opcode\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.opcode_length);
      }
      fprintf(McPAT_config,"-module.pc\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.pc_width);
      fprintf(McPAT_config,"-module.fetch_width\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.fetchW);
      fprintf(McPAT_config,"-module.decode_width\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.decodeW);
      fprintf(McPAT_config,"-module.issue_width\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.issueW);
      fprintf(McPAT_config,"-module.instruction_length\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.instruction_length);
      fprintf(McPAT_config,"-module.int_data_width\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.int_data_width);
      fprintf(McPAT_config,"-module.hthreads\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.num_hthreads);    
      if(McPAT_obj->McPAT_Pipeline->coredynp.num_hthreads > 1)
      {
        fprintf(McPAT_config,"-module.thread_states\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.perThreadState);
      }
      if(McPAT_obj->McPAT_Pipeline->coredynp.core_ty == Inorder)
      {
        fprintf(McPAT_config,"-module.arch_int_regs\t\t%d\n",(int)pow(2.0,McPAT_obj->McPAT_Pipeline->coredynp.arch_ireg_width));
      }
      else
      {
        fprintf(McPAT_config,"-module.phy_int_regs\t\t%d\n",(int)pow(2.0,McPAT_obj->McPAT_Pipeline->coredynp.phy_ireg_width));
        fprintf(McPAT_config,"-module.virtual_address\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.v_address_width);
        fprintf(McPAT_config,"-module.commit_width\t\t%d\n",McPAT_obj->McPAT_Pipeline->coredynp.commitW);
      }
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    case SELECTION_LOGIC:
    {
      assert(McPAT_obj->McPAT_selection_logic);
      fprintf(McPAT_config,"-module.selection_input\t\t%d\n",XML_interface.sys.core[ref_idx].instruction_window_size);
      fprintf(McPAT_config,"-module.selection_output\t\t%d\n",McPAT_obj->McPAT_selection_logic->issue_width);
      
      TDP_duty_cycle *= (TDP_read_cycle/McPAT_obj->McPAT_selection_logic->issue_width);
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);    
      break;
    }
    case UNDIFFERENTIATED_CORE:
    {
      assert(McPAT_obj->McPAT_UndiffCore);
      fprintf(McPAT_config,"-module.pipeline_stages\t\t%d\n",McPAT_obj->McPAT_UndiffCore->coredynp.pipeline_stages);
      fprintf(McPAT_config,"-module.hthreads\t\t%d\n",McPAT_obj->McPAT_UndiffCore->coredynp.num_hthreads);
      fprintf(McPAT_config,"-module.issue_width\t\t%d\n",McPAT_obj->McPAT_UndiffCore->coredynp.issueW);
      
      if(XML_interface.sys.Embedded&&!XML_interface.sys.opt_clockrate)
        fprintf(McPAT_config,"-module.opt_clockrate\t\tfalse\n");
      
      TDP_duty_cycle *= TDP_read_cycle;
      if(TDP_duty_cycle != 1.0)
        fprintf(McPAT_config,"-module.TDP_duty_cycle.read\t\t%lf\n",TDP_duty_cycle);
      break;
    }
    default:
    {
      EI_WARNING("McPAT","unknown energy_model in EI_module_config()");
      break;
    }
  }  
}

void ENERGYLIB_MCPAT::EI_partition_config(FILE *McPAT_config, string partition_name, InputParameter McPAT_input,\
                                          double target_clock_frequency, string component_type, string core_type)
{
  if(!McPAT_config) return;

  fprintf(McPAT_config,"-partition.name\t\t%s\n",partition_name.c_str());
  fprintf(McPAT_config,"-partition.package\t\t%s\n","mcpat");
  fprintf(McPAT_config,"-partition.opt_for_clk\t\t%s # optimize the result on clock\n",opt_for_clk?"true":"false");
  fprintf(McPAT_config,"-partition.embedded\t\t%s # embedded processor?\n",XML_interface.sys.Embedded?"true":"false");
  fprintf(McPAT_config,"-partition.feature_size\t\t%de-9 # partition node in meters\n",(int)XML_interface.sys.core_tech_node);
  fprintf(McPAT_config,"-partition.temperature\t\t%u # temperature in Kelvin\n",(unsigned int)XML_interface.sys.temperature);
  fprintf(McPAT_config,"-partition.component_type\t\t%s # core, uncore, llc(non-private caches)\n",component_type.c_str()); // core device
  fprintf(McPAT_config,"-partition.clock_frequency\t\t%e # clock frequency in Hz\n",target_clock_frequency); // clock frequency in Hz
  fprintf(McPAT_config,"-partition.longer_channel_device\t\t%s\n",XML_interface.sys.longer_channel_device?"true":"false");
  if(!stricmp(component_type,"core"))
    fprintf(McPAT_config,"-partition.core_type\t\t%s\n",core_type.c_str());

  switch(McPAT_input.wt) // TODO: designate a particular input parameters of core components
  {
    case (Wire_type)Global: fprintf(McPAT_config,"-partition.wire_type\t\tglobal\n"); break;
    case (Wire_type)Global_5: fprintf(McPAT_config,"-partition.wire_type\t\tglobal_5\n"); break;
    case (Wire_type)Global_10: fprintf(McPAT_config,"-partition.wire_type\t\tglobal_10\n"); break;
    case (Wire_type)Global_20: fprintf(McPAT_config,"-partition.wire_type\t\tglobal_20\n"); break;
    case (Wire_type)Global_30: fprintf(McPAT_config,"-partition.wire_type\t\tglobal_30\n"); break;
    case (Wire_type)Low_swing: fprintf(McPAT_config,"-partition.wire_type\t\tlow_swing\n"); break;
    case (Wire_type)Semi_global: fprintf(McPAT_config,"-partition.wire_type\t\tsemi_global\n"); break;
    case (Wire_type)Transmission: fprintf(McPAT_config,"-partition.wire_type\t\ttransmission\n"); break;
    case (Wire_type)Optical: fprintf(McPAT_config,"-partition.wire_type\t\toptical\n"); break;
    default: fprintf(McPAT_config,"-partition.wire_type\t\tglobal\n"); break;
  }
  
  if((McPAT_input.data_arr_ram_cell_tech_type-McPAT_input.data_arr_peri_global_tech_type\
     -McPAT_input.tag_arr_ram_cell_tech_type-McPAT_input.tag_arr_peri_global_tech_type)==0)
  {
    switch(McPAT_input.data_arr_ram_cell_tech_type)
    {
      case 0: fprintf(McPAT_config,"-partition.device_type\t\thp\n"); break;
      case 1: fprintf(McPAT_config,"-partition.device_type\t\tlstp\n"); break;
      case 2: fprintf(McPAT_config,"-partition.device_type\t\tlop\n"); break;
      case 3: fprintf(McPAT_config,"-partition.device_type\t\tlp_dram\n"); break;
      case 4: fprintf(McPAT_config,"-partition.device_type\t\tcomm_dram\n"); break;
      default: fprintf(McPAT_config,"-partition.device_type\t\thp\n"); break;
    }
  }
  else 
  {
    switch(McPAT_input.data_arr_ram_cell_tech_type)
    {
      case 0: fprintf(McPAT_config,"-partition.device_type.data_arr_ram_cell_tech_type\t\thp\n"); break;
      case 1: fprintf(McPAT_config,"-partition.device_type.data_arr_ram_cell_tech_type\t\tlstp\n"); break;
      case 2: fprintf(McPAT_config,"-partition.device_type.data_arr_ram_cell_tech_type\t\tlop\n"); break;
      case 3: fprintf(McPAT_config,"-partition.device_type.data_arr_ram_cell_tech_type\t\tlp_dram\n"); break;
      case 4: fprintf(McPAT_config,"-partition.device_type.data_arr_ram_cell_tech_type\t\tcomm_dram\n"); break;
      default: fprintf(McPAT_config,"-partition.device_type.data_arr_ram_cell_tech_type\t\thp\n"); break;
    }
    switch(McPAT_input.data_arr_peri_global_tech_type)
    {
      case 0: fprintf(McPAT_config,"-partition.device_type.data_arr_peri_global_tech_type\t\thp\n"); break;
      case 1: fprintf(McPAT_config,"-partition.device_type.data_arr_peri_global_tech_type\t\tlstp\n"); break;
      case 2: fprintf(McPAT_config,"-partition.device_type.data_arr_peri_global_tech_type\t\tlop\n"); break;
      case 3: fprintf(McPAT_config,"-partition.device_type.data_arr_peri_global_tech_type\t\tlp_dram\n"); break;
      case 4: fprintf(McPAT_config,"-partition.device_type.data_arr_peri_global_tech_type\t\tcomm_dram\n"); break;
      default: fprintf(McPAT_config,"-partition.device_type.data_arr_peri_global_tech_type\t\thp\n"); break;
    }
    switch(McPAT_input.tag_arr_ram_cell_tech_type)
    {
      case 0: fprintf(McPAT_config,"-partition.device_type.tag_arr_ram_cell_tech_type\t\thp\n"); break;
      case 1: fprintf(McPAT_config,"-partition.device_type.tag_arr_ram_cell_tech_type\t\tlstp\n"); break;
      case 2: fprintf(McPAT_config,"-partition.device_type.tag_arr_ram_cell_tech_type\t\tlop\n"); break;
      case 3: fprintf(McPAT_config,"-partition.device_type.tag_arr_ram_cell_tech_type\t\tlp_dram\n"); break;
      case 4: fprintf(McPAT_config,"-partition.device_type.tag_arr_ram_cell_tech_type\t\tcomm_dram\n"); break;
      default: fprintf(McPAT_config,"-partition.device_type.tag_arr_ram_cell_tech_type\t\thp\n"); break;
    }
    switch(McPAT_input.tag_arr_peri_global_tech_type)
    {
      case 0: fprintf(McPAT_config,"-partition.device_type.tag_arr_peri_global_tech_type\t\thp\n"); break;
      case 1: fprintf(McPAT_config,"-partition.device_type.tag_arr_peri_global_tech_type\t\tlstp\n"); break;
      case 2: fprintf(McPAT_config,"-partition.device_type.tag_arr_peri_global_tech_type\t\tlop\n"); break;
      case 3: fprintf(McPAT_config,"-partition.device_type.tag_arr_peri_global_tech_type\t\tlp_dram\n"); break;
      case 4: fprintf(McPAT_config,"-partition.device_type.tag_arr_peri_global_tech_type\t\tcomm_dram\n"); break;
      default: fprintf(McPAT_config,"-partition.device_type.tag_arr_peri_global_tech_type\t\thp\n"); break;
    }
  }

  fprintf(McPAT_config,"-partition.interconnect_projection\t\t%s\n",McPAT_input.ic_proj_type==0?"aggressive":"conservative");
    
  if((McPAT_input.wire_is_mat_type-McPAT_input.wire_os_mat_type) == 0)
  {
    switch(McPAT_input.wire_is_mat_type)
    {
      case 0: fprintf(McPAT_config,"-partition.wiring_type\t\tlocal\n"); break;
      case 1: fprintf(McPAT_config,"-partition.wiring_type\t\tsemi_global\n"); break;
      case 2: fprintf(McPAT_config,"-partition.wiring_type\t\tglobal\n"); break;
      case 3: fprintf(McPAT_config,"-partition.wiring_type\t\tdram\n"); break;
      default: fprintf(McPAT_config,"-partition.wiring_type\t\tglobal\n"); break;
    }
  }
  else
  {
    switch(McPAT_input.wire_is_mat_type)
    {
      case 0: fprintf(McPAT_config,"-partition.wiring_type.wire_is_mat_type\t\tlocal\n"); break;
      case 1: fprintf(McPAT_config,"-partition.wiring_type.wire_is_mat_type\t\tsemi_global\n"); break;
      case 2: fprintf(McPAT_config,"-partition.wiring_type.wire_is_mat_type\t\tglobal\n"); break;
      case 3: fprintf(McPAT_config,"-partition.wiring_type.wire_is_mat_type\t\tdram\n"); break;
      default: fprintf(McPAT_config,"-partition.wiring_type.wire_is_mat_type\t\tglobal\n"); break;
    }
    switch(McPAT_input.wire_os_mat_type)
    {
      case 0: fprintf(McPAT_config,"-partition.wiring_type.wire_os_mat_type\t\tlocal\n"); break;
      case 1: fprintf(McPAT_config,"-partition.wiring_type.wire_os_mat_type\t\tsemi_global\n"); break;
      case 2: fprintf(McPAT_config,"-partition.wiring_type.wire_os_mat_type\t\tglobal\n"); break;
      case 3: fprintf(McPAT_config,"-partition.wiring_type.wire_os_mat_type\t\tdram\n"); break;
      default: fprintf(McPAT_config,"-partition.wiring_type.wire_os_mat_type\t\tglobal\n"); break;
    }
  }
/*  
  switch(McPAT_input.ver_htree_wires_over_array)
  {
    case 0: fprintf(McPAT_config,"-partition.ver_htree_wires_over_array\t\tlocal\n"); break;
    case 1: fprintf(McPAT_config,"-partition.ver_htree_wires_over_array\t\tsemi_global\n"); break;
    case 2: fprintf(McPAT_config,"-partition.ver_htree_wires_over_array\t\tglobal\n"); break;
    case 3: fprintf(McPAT_config,"-partition.ver_htree_wires_over_array\t\tdram\n"); break;
    default: fprintf(McPAT_config,"-partition.ver_htree_wires_over_array\t\tlocal\n"); break;
  }
  switch(McPAT_input.broadcast_addr_din_over_ver_htrees)
  {
    case 0: fprintf(McPAT_config,"-partition.broadcast_addr_din_over_ver_htrees\t\tlocal\n"); break;
    case 1: fprintf(McPAT_config,"-partition.broadcast_addr_din_over_ver_htrees\t\tsemi_global\n"); break;
    case 2: fprintf(McPAT_config,"-partition.broadcast_addr_din_over_ver_htrees\t\tglobal\n"); break;
    case 3: fprintf(McPAT_config,"-partition.braodcast_addr_din_over_ver_htrees\t\tdram\n"); break;
    default: fprintf(McPAT_config,"-partition.broadcast_addr_din_over_ver_htrees\t\tlocal\n"); break;
  }
*/  
  fprintf(McPAT_config,"-partition.end\n\n");
}

void ENERGYLIB_MCPAT::parse_XML(Processor *McPAT_processor, parameters_component_t &parameters_module)
{
  string file_config;
  set_variable(file_config,parameters_module,"file_config","CONFIG/MCPAT/mcpat.config");
  FILE *McPAT_config = fopen((char*)file_config.c_str(),"w");
  
  if(!McPAT_config) 
    EI_ERROR("McPAT","cannot open config file "+(string)file_config+" to dump");
    
  fprintf(McPAT_config,"### AUTOGENERATED ENERGY INTROSPECTOR CONFIG FOR MCPAT ###\n\n");
  
  char module_name[128], partition_name[128];

  int default_queue_size;  
  set_variable(default_queue_size,parameters_module,"default_queue_size",1);
  
  fprintf(McPAT_config,"-package.name\t\t%s\n","mcpat");
  fprintf(McPAT_config,"-package.end\n\n");
  
  // create pseudo package
  pseudo_package_t pseudo_package;
  pseudo_package.queue.create<dimension_t>("dimension",1);  
  pseudo_package.queue.create<power_t>("TDP",1);
  pseudo_package.queue.create<power_t>("power",default_queue_size);
  
  // core
  for(int core_idx = 0; core_idx < XML_interface.sys.number_of_cores; core_idx++)
  {
    pseudo_partition_t pseudo_partition;
  
    int ref_core_idx;
    
    if(XML_interface.sys.homogeneous_cores)
      ref_core_idx = 0;
    else
      ref_core_idx = core_idx;

    sprintf(partition_name,"core[%d]",core_idx);

    // refer to icache variables to setup core-wide parameters
    assert(McPAT_processor->cores[ref_core_idx]->ifu&&McPAT_processor->cores[ref_core_idx]->ifu->icache.caches);
    InputParameter McPAT_input = McPAT_processor->cores[ref_core_idx]->ifu->icache.caches->l_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.core[ref_core_idx].clock_rate*1e6,\
                             "core",(enum Core_type)XML_interface.sys.core[ref_core_idx].machine_type==Inorder?"inorder":"OOO");
                             
    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);      
    pseudo_partition.package = "mcpat";
    
    if(McPAT_processor->cores[ref_core_idx]->ifu)
    {
      if(McPAT_processor->cores[ref_core_idx]->ifu->icache.caches) // instruction cache 
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->icache.caches->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.caches,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.caches->l_ip,\
        1.0,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.caches->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->ifu->icache.caches->tdp_stats.readAc.miss,0.0,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
        
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->icache.missb) // instruction cache missbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->icache.missb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.missb,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.missb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->ifu->icache.missb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->ifu->icache.missb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);

        pseudo_partition.module.push_back(string(module_name));                
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->icache.ifb) // instruction cache fillbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->icache.ifb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.ifb,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.ifb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->ifu->icache.ifb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->ifu->icache.ifb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->icache.prefetchb) // instruction cache prefetchbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->icache.prefetchb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.prefetchb,\
        McPAT_processor->cores[ref_core_idx]->ifu->icache.prefetchb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->ifu->icache.prefetchb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->ifu->icache.prefetchb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->IB) // instruction buffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->IB->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->IB,\
        McPAT_processor->cores[ref_core_idx]->ifu->IB->l_ip,\
        1.0,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->ifu->IB->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->ifu->IB->tdp_stats.writeAc.access,0.0,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->BTB) // branch target buffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->BTB->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->BTB,\
        McPAT_processor->cores[ref_core_idx]->ifu->BTB->l_ip,\
        1.0,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->ifu->BTB->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->ifu->BTB->tdp_stats.writeAc.access,0.0,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->ID_inst) // instruction decoder
      {
        sprintf(module_name,"core[%d].%s",core_idx,"ID_inst" /* McPAT doesn't name a decoder */);
        populate_EI<inst_decoder>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_inst,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_inst->l_ip,\
        XML_interface.sys.core[ref_core_idx].decode_width,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_inst->tdp_stats.readAc.access,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INSTRUCTION_DECODER);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->ID_operand) // operand decoder
      {
        sprintf(module_name,"core[%d].%s",core_idx,"ID_operand" /* McPAT doesn't name a decoder */);
        populate_EI<inst_decoder>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_operand,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_operand->l_ip,\
        XML_interface.sys.core[ref_core_idx].decode_width,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_operand->tdp_stats.readAc.access,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INSTRUCTION_DECODER);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->ID_misc) // micro-op sequencer
      {
        sprintf(module_name,"core[%d].%s",core_idx,"ID_misc" /* McPAT doesn't name a decoder */);
        populate_EI<inst_decoder>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_misc,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_misc->l_ip,\
        XML_interface.sys.core[ref_core_idx].decode_width,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->ifu->ID_misc->tdp_stats.readAc.access,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INSTRUCTION_DECODER);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->ifu->BPT)
      {
        if(McPAT_processor->cores[ref_core_idx]->ifu->BPT->globalBPT) // global predictor
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->BPT->globalBPT->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->globalBPT,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->globalBPT->l_ip,\
          1.0,1.0,1.0,false,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->globalBPT->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->ifu->BPT->globalBPT->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,XML_interface.sys.core[ref_core_idx].number_hardware_threads>1?ARRAY_CACHE:ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->ifu->BPT->L1_localBPT) // local predictor L1
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->BPT->L1_localBPT->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->L1_localBPT,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->L1_localBPT->l_ip,\
          1.0,1.0,1.0,false,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->L1_localBPT->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->ifu->BPT->L1_localBPT->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,XML_interface.sys.core[ref_core_idx].number_hardware_threads>1?ARRAY_CACHE:ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->ifu->BPT->L2_localBPT) // local predictor L2
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->BPT->L2_localBPT->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->L2_localBPT,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->L2_localBPT->l_ip,\
          1.0,1.0,1.0,false,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->L2_localBPT->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->ifu->BPT->L2_localBPT->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,XML_interface.sys.core[ref_core_idx].number_hardware_threads>1?ARRAY_CACHE:ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->ifu->BPT->chooser) // chooser
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->BPT->chooser->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->chooser,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->chooser->l_ip,\
          1.0,1.0,1.0,false,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->chooser->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->ifu->BPT->chooser->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,XML_interface.sys.core[ref_core_idx].number_hardware_threads>1?ARRAY_CACHE:ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->ifu->BPT->RAS) // RAS
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->ifu->BPT->RAS->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->RAS,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->RAS->l_ip,\
          XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,false,\
          McPAT_processor->cores[ref_core_idx]->ifu->BPT->RAS->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->ifu->BPT->RAS->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
      }
    }
    if(McPAT_processor->cores[ref_core_idx]->exu)
    {
      if(McPAT_processor->cores[ref_core_idx]->exu->scheu)
      {
        if(McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window) // instruction window
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window->l_ip,\
          1.0,XML_interface.sys.core[ref_core_idx].pipelines_per_core[0],1.0,/*McPAT leakge is wrong*/false,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window->tdp_stats.searchAc.access,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window) // fp instruction window
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window->l_ip,\
          1.0,XML_interface.sys.core[ref_core_idx].pipelines_per_core[1],1.0,false,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window->tdp_stats.searchAc.access,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->exu->scheu->ROB) // ROB
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->scheu->ROB->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->ROB,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->ROB->l_ip,\
          1.0,XML_interface.sys.core[ref_core_idx].pipelines_per_core[0],1.0,false,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->ROB->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->exu->scheu->ROB->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->exu->scheu->instruction_selection) // instruction selection logic
        {
          sprintf(module_name,"core[%d].%s",core_idx,"instruction_selection" /* McPAT doesn't name a selection logic */);
          populate_EI<selection_logic>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->instruction_selection,\
          McPAT_processor->cores[ref_core_idx]->exu->scheu->instruction_selection->l_ip,\
          (McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window?1.0:0.0)+(McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window?1.0:0.0),1.0,1.0,false,\
          (McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window?McPAT_processor->cores[ref_core_idx]->exu->scheu->int_inst_window->tdp_stats.readAc.access:0.0)+(McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window?McPAT_processor->cores[ref_core_idx]->exu->scheu->fp_inst_window->tdp_stats.readAc.access:0.0),0.0,0.0,0.0,0.0,\
          string(partition_name),string(module_name),SELECTION_LOGIC);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->rfu)
      {
        if(McPAT_processor->cores[ref_core_idx]->exu->rfu->IRF) // integer RF
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->rfu->IRF->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->IRF,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->IRF->l_ip,\
          XML_interface.sys.core[ref_core_idx].number_hardware_threads,cdb_overhead*XML_interface.sys.core[ref_core_idx].pipelines_per_core[0],1.0,false,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->IRF->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->exu->rfu->IRF->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->exu->rfu->FRF) // fp RF
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->rfu->FRF->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->FRF,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->FRF->l_ip,\
          XML_interface.sys.core[ref_core_idx].number_hardware_threads,cdb_overhead*XML_interface.sys.core[ref_core_idx].pipelines_per_core[1],1.0,false,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->FRF->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->exu->rfu->FRF->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->cores[ref_core_idx]->exu->rfu->RFWIN) // RF window
        {
          sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->rfu->RFWIN->name.c_str());
          populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->RFWIN,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->RFWIN->l_ip,\
          1.0,XML_interface.sys.core[ref_core_idx].pipelines_per_core[0],1.0,false,\
          McPAT_processor->cores[ref_core_idx]->exu->rfu->RFWIN->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->exu->rfu->RFWIN->tdp_stats.writeAc.access,0.0,0.0,0.0,\
          string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                  
          pseudo_partition.module.push_back(string(module_name));
        }
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->exeu) // integer ALU
      {
        sprintf(module_name,"core[%d].%s",core_idx,"exeu" /* McPAT doesn't name a FU */);
        populate_EI<FunctionalUnit>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->exeu,\
        McPAT_processor->cores[ref_core_idx]->exu->exeu->interface_ip,\
        1.0,1.0,1.0,false,\
        /*XML_interface.sys.core[ref_core_idx].ALU_per_core*/McPAT_processor->cores[ref_core_idx]->exu->exeu->coredynp.ALU_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),FUNCTIONAL_UNIT,FUNCTIONAL_UNIT_ALU);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->fp_u) // FPU
      {
        sprintf(module_name,"core[%d].%s",core_idx,"fp_u" /* McPAT doens't name a FU */);
        populate_EI<FunctionalUnit>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->fp_u,\
        McPAT_processor->cores[ref_core_idx]->exu->fp_u->interface_ip,\
        1.0,1.0,1.0,false,\
        /*XML_interface.sys.core[ref_core_idx].FPU_per_core*/McPAT_processor->cores[ref_core_idx]->exu->fp_u->coredynp.FPU_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),FUNCTIONAL_UNIT,FUNCTIONAL_UNIT_FPU);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->mul) // MUL
      {
        sprintf(module_name,"core[%d].%s",core_idx,"mul" /* McPAT doesn't name a FU */);
        populate_EI<FunctionalUnit>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->mul,\
        McPAT_processor->cores[ref_core_idx]->exu->mul->interface_ip,\
        1.0,1.0,1.0,false,\
        /*XML_interface.sys.core[ref_core_idx].MUL_per_core*/McPAT_processor->cores[ref_core_idx]->exu->mul->coredynp.MUL_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),FUNCTIONAL_UNIT,FUNCTIONAL_UNIT_MUL);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->int_bypass) // integer bypass
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->int_bypass->name.c_str());
        populate_EI<interconnect>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->int_bypass,\
        McPAT_processor->cores[ref_core_idx]->exu->int_bypass->l_ip,
        2.0,1.0/2.0,1.0,false,\
        2.0*XML_interface.sys.core[ref_core_idx].ALU_cdb_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INTERCONNECT);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->intTagBypass) // integer tag bypass
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->intTagBypass->name.c_str());
        populate_EI<interconnect>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->intTagBypass,\
        McPAT_processor->cores[ref_core_idx]->exu->intTagBypass->l_ip,\
        2.0,1.0/2.0,1.0,/*McPAT scales only leakage*/false,\
        2.0*XML_interface.sys.core[ref_core_idx].ALU_cdb_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INTERCONNECT);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->int_mul_bypass) // integer mul bypass
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->int_mul_bypass->name.c_str());
        populate_EI<interconnect>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->int_mul_bypass,\
        McPAT_processor->cores[ref_core_idx]->exu->int_mul_bypass->l_ip,\
        2.0,1.0/2.0,1.0,false,\
        2.0*XML_interface.sys.core[ref_core_idx].MUL_cdb_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INTERCONNECT);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->intTag_mul_Bypass) // integer mul tag bypass
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->intTag_mul_Bypass->name.c_str());
        populate_EI<interconnect>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->intTag_mul_Bypass,\
        McPAT_processor->cores[ref_core_idx]->exu->intTag_mul_Bypass->l_ip,\
        2.0,1.0/2.0,1.0,false,\
        2.0*XML_interface.sys.core[ref_core_idx].MUL_cdb_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INTERCONNECT);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->fp_bypass) // fp bypass
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->fp_bypass->name.c_str());
        populate_EI<interconnect>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->fp_bypass,\
        McPAT_processor->cores[ref_core_idx]->exu->fp_bypass->l_ip,\
        3.0,1.0/3.0,1.0,false,\
        3.0*XML_interface.sys.core[ref_core_idx].FPU_cdb_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INTERCONNECT);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->exu->fpTagBypass) // fp tag bypass
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->exu->fpTagBypass->name.c_str());
        populate_EI<interconnect>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->exu->fpTagBypass,\
        McPAT_processor->cores[ref_core_idx]->exu->fpTagBypass->l_ip,\
        3.0,1.0/3.0,1.0,false,\
        3.0*XML_interface.sys.core[ref_core_idx].FPU_cdb_duty_cycle,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),INTERCONNECT);
                
        pseudo_partition.module.push_back(string(module_name));
      }
    }
    if(McPAT_processor->cores[ref_core_idx]->rnu)
    {
      if(McPAT_processor->cores[ref_core_idx]->rnu->iFRAT) // integer FRAT
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->rnu->iFRAT->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->rnu->iFRAT,\
        McPAT_processor->cores[ref_core_idx]->rnu->iFRAT->l_ip,\
        XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,(((enum Scheduler_type)XML_interface.sys.core[ref_core_idx].instruction_window_scheme==ReservationStation)&&((enum Renaming_type)XML_interface.sys.core[ref_core_idx].rename_scheme)==RAMbased)?true:false,\
        XML_interface.sys.core[ref_core_idx].rename_scheme==RAMbased?McPAT_processor->cores[ref_core_idx]->rnu->iFRAT->tdp_stats.readAc.access:0.0,McPAT_processor->cores[ref_core_idx]->rnu->iFRAT->tdp_stats.writeAc.access,(enum Renaming_type)XML_interface.sys.core[ref_core_idx].rename_scheme==RAMbased?0.0:McPAT_processor->cores[ref_core_idx]->rnu->iFRAT->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,McPAT_processor->cores[ref_core_idx]->rnu->iFRAT->l_ip.pure_ram?ARRAY_RAM:ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->rnu->fFRAT) // fp FRAT
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->rnu->fFRAT->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->rnu->fFRAT,\
        McPAT_processor->cores[ref_core_idx]->rnu->fFRAT->l_ip,\
        XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,(((enum Scheduler_type)XML_interface.sys.core[ref_core_idx].instruction_window_scheme==ReservationStation)&&((enum Renaming_type)XML_interface.sys.core[ref_core_idx].rename_scheme)==RAMbased)?true:false,\
        XML_interface.sys.core[ref_core_idx].rename_scheme==RAMbased?McPAT_processor->cores[ref_core_idx]->rnu->fFRAT->tdp_stats.readAc.access:0.0,McPAT_processor->cores[ref_core_idx]->rnu->fFRAT->tdp_stats.writeAc.access,(enum Renaming_type)XML_interface.sys.core[ref_core_idx].rename_scheme==RAMbased?0.0:McPAT_processor->cores[ref_core_idx]->rnu->fFRAT->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,McPAT_processor->cores[ref_core_idx]->rnu->fFRAT->l_ip.pure_ram?ARRAY_RAM:ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->rnu->iRRAT) // integer RRAT
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->rnu->iRRAT->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->rnu->iRRAT,\
        McPAT_processor->cores[ref_core_idx]->rnu->iRRAT->l_ip,\
        XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->rnu->iRRAT->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->rnu->iRRAT->tdp_stats.writeAc.access,0.0,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->rnu->fRRAT) // fp RRAT
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->rnu->fRRAT->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->rnu->fRRAT,\
        McPAT_processor->cores[ref_core_idx]->rnu->fRRAT->l_ip,\
        XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->rnu->fRRAT->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->rnu->fRRAT->tdp_stats.writeAc.access,0.0,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->rnu->ifreeL) // integer freelist
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->rnu->ifreeL->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->rnu->ifreeL,\
        McPAT_processor->cores[ref_core_idx]->rnu->ifreeL->l_ip,\
        XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->rnu->ifreeL->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->rnu->ifreeL->tdp_stats.writeAc.access,0.0,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->rnu->ffreeL) // fp freelist
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->rnu->ffreeL->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->rnu->ffreeL,\
        McPAT_processor->cores[ref_core_idx]->rnu->ffreeL->l_ip,\
        XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->rnu->ffreeL->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->rnu->ffreeL->tdp_stats.writeAc.access,0.0,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_RAM);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->rnu->idcl) // integer dependency check
      {
        sprintf(module_name,"core[%d].%s",core_idx,"idcl" /* McPAT doesn't name a dependency check logic */);
        populate_EI<dep_resource_conflict_check>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->rnu->idcl,\
        McPAT_processor->cores[ref_core_idx]->rnu->idcl->l_ip,\
        XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->rnu->iFRAT?McPAT_processor->cores[ref_core_idx]->rnu->iFRAT->tdp_stats.readAc.access:McPAT_processor->cores[ref_core_idx]->rnu->idcl->tdp_stats.readAc.access,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),DEPENDENCY_CHECK_LOGIC);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->rnu->fdcl) // fp dependency check
      {
        sprintf(module_name,"core[%d].%s",core_idx,"fdcl" /* McPAT doens't name a dependency check logic */);
        populate_EI<dep_resource_conflict_check>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->rnu->fdcl,\
        McPAT_processor->cores[ref_core_idx]->rnu->fdcl->l_ip,\
        XML_interface.sys.core[ref_core_idx].number_hardware_threads,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->rnu->fFRAT?McPAT_processor->cores[ref_core_idx]->rnu->fFRAT->tdp_stats.readAc.access:McPAT_processor->cores[ref_core_idx]->rnu->fdcl->tdp_stats.readAc.access,0.0,0.0,0.0,0.0,\
        string(partition_name),string(module_name),DEPENDENCY_CHECK_LOGIC);
                
        pseudo_partition.module.push_back(string(module_name));
      }
    }
    if(McPAT_processor->cores[ref_core_idx]->lsu)
    {
      if(McPAT_processor->cores[ref_core_idx]->lsu->dcache.caches) // data cache
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->lsu->dcache.caches->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.caches,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.caches->l_ip,\
        1.0,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.caches->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->lsu->dcache.caches->tdp_stats.writeAc.access,0.0,McPAT_processor->cores[ref_core_idx]->lsu->dcache.caches->tdp_stats.writeAc.miss,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->lsu->dcache.missb) // data cache missbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->lsu->dcache.missb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.missb,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.missb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->lsu->dcache.missb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->lsu->dcache.missb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->lsu->dcache.ifb) // data cache fillbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->lsu->dcache.ifb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.ifb,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.ifb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->lsu->dcache.ifb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->lsu->dcache.ifb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->lsu->dcache.prefetchb) // data cache prefetchbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->lsu->dcache.prefetchb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.prefetchb,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.prefetchb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->lsu->dcache.prefetchb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->lsu->dcache.prefetchb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->lsu->dcache.wbb) // data cache writebackbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->lsu->dcache.wbb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.wbb,\
        McPAT_processor->cores[ref_core_idx]->lsu->dcache.wbb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->lsu->dcache.wbb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->lsu->dcache.wbb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->lsu->LSQ) // LSQ or store queue
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->lsu->LSQ->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->lsu->LSQ,\
        McPAT_processor->cores[ref_core_idx]->lsu->LSQ->l_ip,\
        1.0,cdb_overhead,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->lsu->LSQ->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->lsu->LSQ->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->lsu->LSQ->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->lsu->LoadQ) // load queue
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->lsu->LoadQ->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->lsu->LoadQ,\
        McPAT_processor->cores[ref_core_idx]->lsu->LoadQ->l_ip,\
        1.0,cdb_overhead,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->lsu->LoadQ->tdp_stats.readAc.access,McPAT_processor->cores[ref_core_idx]->lsu->LoadQ->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->lsu->LoadQ->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
    }
    if(McPAT_processor->cores[ref_core_idx]->mmu)
    {
      if(McPAT_processor->cores[ref_core_idx]->mmu->itlb) // ITLB
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->mmu->itlb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->mmu->itlb,\
        McPAT_processor->cores[ref_core_idx]->mmu->itlb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->mmu->itlb->tdp_stats.readAc.miss,McPAT_processor->cores[ref_core_idx]->mmu->itlb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->mmu->dtlb) // DTLB
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->mmu->dtlb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->mmu->dtlb,\
        McPAT_processor->cores[ref_core_idx]->mmu->dtlb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->mmu->dtlb->tdp_stats.readAc.miss,McPAT_processor->cores[ref_core_idx]->mmu->dtlb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
    }
    if(McPAT_processor->cores[ref_core_idx]->undiffCore) // undifferentiated core
    {
      sprintf(module_name,"core[%d].%s",core_idx,"undiffCore" /* McPAT doesn't name a control logic */);
      populate_EI<UndiffCore>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
      McPAT_processor->cores[ref_core_idx]->undiffCore,\
      McPAT_processor->cores[ref_core_idx]->undiffCore->interface_ip,\
      1.0,1.0,1.0,false,\
      (((enum Core_type)XML_interface.sys.core[ref_core_idx].machine_type==OOO)?(1.0+XML_interface.sys.core[ref_core_idx].IFU_duty_cycle+XML_interface.sys.core[ref_core_idx].LSU_duty_cycle+XML_interface.sys.core[ref_core_idx].ALU_duty_cycle+XML_interface.sys.core[ref_core_idx].LSU_duty_cycle)/5.0:(XML_interface.sys.core[ref_core_idx].IFU_duty_cycle+XML_interface.sys.core[ref_core_idx].LSU_duty_cycle+XML_interface.sys.core[ref_core_idx].ALU_duty_cycle+XML_interface.sys.core[ref_core_idx].LSU_duty_cycle)/4.0),1.0,1.0,0.0,0.0,\
      string(partition_name),string(module_name),UNDIFFERENTIATED_CORE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    if(McPAT_processor->cores[ref_core_idx]->corepipe) // pipeline
    {
      sprintf(module_name,"core[%d].%s",core_idx,"corepipe" /* McPAT doesn't name a pipeline logic */);
      populate_EI<Pipeline>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
      McPAT_processor->cores[ref_core_idx]->corepipe,\
      McPAT_processor->cores[ref_core_idx]->corepipe->l_ip,\
      1.0,1.0,1.0,false,\
      XML_interface.sys.core[ref_core_idx].pipelines_per_core[0],0.0,0.0,0.0,0.0,\
      string(partition_name),string(module_name),PIPELINE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    if(McPAT_processor->cores[ref_core_idx]->l2cache) // L2 cache
    {
      if(McPAT_processor->cores[ref_core_idx]->l2cache->unicache.caches) // data cache
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.caches->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.caches,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.caches->l_ip,\
        1.0,1.0,1.0,false,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.caches->tdp_stats.readAc.hit+(McPAT_processor->cores[ref_core_idx]->l2cache->cachep.dir_ty==SBT?(McPAT_processor->cores[ref_core_idx]->l2cache->homenode_stats_t.readAc.hit*McPAT_processor->cores[ref_core_idx]->l2cache->dir_overhead):0.0),McPAT_processor->cores[ref_core_idx]->l2cache->unicache.caches->tdp_stats.writeAc.access+(McPAT_processor->cores[ref_core_idx]->l2cache->cachep.dir_ty==SBT?(McPAT_processor->cores[ref_core_idx]->l2cache->homenode_stats_t.writeAc.hit*McPAT_processor->cores[ref_core_idx]->l2cache->dir_overhead+McPAT_processor->cores[ref_core_idx]->l2cache->homenode_stats_t.writeAc.miss):0.0),0.0,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.caches->tdp_stats.readAc.miss+(McPAT_processor->cores[ref_core_idx]->l2cache->cachep.dir_ty==SBT?(McPAT_processor->cores[ref_core_idx]->l2cache->homenode_stats_t.readAc.miss+McPAT_processor->cores[ref_core_idx]->l2cache->homenode_stats_t.writeAc.miss-McPAT_processor->cores[ref_core_idx]->l2cache->homenode_stats_t.readAc.hit*(McPAT_processor->cores[ref_core_idx]->l2cache->dir_overhead-1.0)-McPAT_processor->cores[ref_core_idx]->l2cache->homenode_stats_t.writeAc.hit*(McPAT_processor->cores[ref_core_idx]->l2cache->dir_overhead-1.0)):0.0),McPAT_processor->cores[ref_core_idx]->l2cache->unicache.caches->tdp_stats.writeAc.miss,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->l2cache->unicache.missb) // data cache missbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.missb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.missb,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.missb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.missb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.missb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->l2cache->unicache.ifb) // data cache fillbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.ifb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.ifb,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.ifb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.ifb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.ifb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->l2cache->unicache.prefetchb) // data cache prefetchbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.prefetchb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.prefetchb,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.prefetchb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.prefetchb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.prefetchb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->cores[ref_core_idx]->l2cache->unicache.wbb) // data cache writebackbuffer
      {
        sprintf(module_name,"core[%d].%s",core_idx,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.wbb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_core_idx,core_idx,McPAT_processor->cores[ref_core_idx]->clockRate,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.wbb,\
        McPAT_processor->cores[ref_core_idx]->l2cache->unicache.wbb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.wbb->tdp_stats.writeAc.access,McPAT_processor->cores[ref_core_idx]->l2cache->unicache.wbb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
    }
    
    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
  
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }
  if(!XML_interface.sys.Private_L2)
  {
    for(int L2_idx = 0; L2_idx < XML_interface.sys.number_of_L2s; L2_idx++)
    {
      pseudo_partition_t pseudo_partition;
    
      int ref_L2_idx;

      if(XML_interface.sys.homogeneous_L2s)
        ref_L2_idx = 0;
      else
        ref_L2_idx = L2_idx;

      sprintf(partition_name,"L2[%d]",L2_idx);

      // refer to L2 cache variables to setup L2-wide parameters
      assert(McPAT_processor->l2array[ref_L2_idx]&&McPAT_processor->l2array[ref_L2_idx]->unicache.caches);
      InputParameter McPAT_input = McPAT_processor->l2array[ref_L2_idx]->unicache.caches->l_ip;

      EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.L2[ref_L2_idx].clockrate*1e6,"llc","n/a");
        
      pseudo_partition.queue.create<dimension_t>("dimension",1);
      pseudo_partition.queue.create<power_t>("TDP",1);
      pseudo_partition.queue.create<power_t>("power",default_queue_size);
      pseudo_partition.queue.create<double>("temperature",1);
      pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
      pseudo_partition.package = "mcpat";
      
      if(McPAT_processor->l2array[ref_L2_idx]->unicache.caches) // data cache
      {
        sprintf(module_name,"L2[%d].%s",L2_idx,McPAT_processor->l2array[ref_L2_idx]->unicache.caches->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_L2_idx,L2_idx,McPAT_processor->l2array[ref_L2_idx]->cachep.clockRate,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.caches,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.caches->l_ip,\
        1.0,1.0,1.0,false,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.caches->tdp_stats.readAc.hit+(McPAT_processor->l2array[ref_L2_idx]->cachep.dir_ty==SBT?(McPAT_processor->l2array[ref_L2_idx]->homenode_stats_t.readAc.hit*McPAT_processor->l2array[ref_L2_idx]->dir_overhead):0.0),McPAT_processor->l2array[ref_L2_idx]->unicache.caches->tdp_stats.writeAc.access+(McPAT_processor->l2array[ref_L2_idx]->cachep.dir_ty==SBT?(McPAT_processor->l2array[ref_L2_idx]->homenode_stats_t.writeAc.hit*McPAT_processor->l2array[ref_L2_idx]->dir_overhead+McPAT_processor->l2array[ref_L2_idx]->homenode_stats_t.writeAc.miss):0.0),0.0,McPAT_processor->l2array[ref_L2_idx]->unicache.caches->tdp_stats.readAc.miss+(McPAT_processor->l2array[ref_L2_idx]->cachep.dir_ty==SBT?(McPAT_processor->l2array[ref_L2_idx]->homenode_stats_t.readAc.miss+McPAT_processor->l2array[ref_L2_idx]->homenode_stats_t.writeAc.miss-McPAT_processor->l2array[ref_L2_idx]->homenode_stats_t.readAc.hit*(McPAT_processor->l2array[ref_L2_idx]->dir_overhead-1.0)-McPAT_processor->l2array[ref_L2_idx]->homenode_stats_t.writeAc.hit*(McPAT_processor->l2array[ref_L2_idx]->dir_overhead-1.0)):0.0),McPAT_processor->l2array[ref_L2_idx]->unicache.caches->tdp_stats.writeAc.miss,
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->l2array[ref_L2_idx]->unicache.missb) // data cache missbuffer
      {
        sprintf(module_name,"L2[%d].%s",L2_idx,McPAT_processor->l2array[ref_L2_idx]->unicache.missb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_L2_idx,L2_idx,McPAT_processor->l2array[ref_L2_idx]->cachep.clockRate,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.missb,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.missb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->l2array[ref_L2_idx]->unicache.missb->tdp_stats.writeAc.access,McPAT_processor->l2array[ref_L2_idx]->unicache.missb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->l2array[ref_L2_idx]->unicache.ifb) // data cache fillbuffer
      {
        sprintf(module_name,"L2[%d].%s",L2_idx,McPAT_processor->l2array[ref_L2_idx]->unicache.ifb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_L2_idx,L2_idx,McPAT_processor->l2array[ref_L2_idx]->cachep.clockRate,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.ifb,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.ifb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->l2array[ref_L2_idx]->unicache.ifb->tdp_stats.writeAc.access,McPAT_processor->l2array[ref_L2_idx]->unicache.ifb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->l2array[ref_L2_idx]->unicache.prefetchb) // data cache prefetchbuffer
      {
        sprintf(module_name,"L2[%d].%s",L2_idx,McPAT_processor->l2array[ref_L2_idx]->unicache.prefetchb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_L2_idx,L2_idx,McPAT_processor->l2array[ref_L2_idx]->cachep.clockRate,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.prefetchb,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.prefetchb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->l2array[ref_L2_idx]->unicache.prefetchb->tdp_stats.writeAc.access,McPAT_processor->l2array[ref_L2_idx]->unicache.prefetchb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      if(McPAT_processor->l2array[ref_L2_idx]->unicache.wbb) // data cache writebackbuffer
      {
        sprintf(module_name,"L2[%d].%s",L2_idx,McPAT_processor->l2array[ref_L2_idx]->unicache.wbb->name.c_str());
        populate_EI<ArrayST>(McPAT_config,ref_L2_idx,L2_idx,McPAT_processor->l2array[ref_L2_idx]->cachep.clockRate,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.wbb,\
        McPAT_processor->l2array[ref_L2_idx]->unicache.wbb->l_ip,\
        1.0,1.0,1.0,false,\
        0.0,McPAT_processor->l2array[ref_L2_idx]->unicache.wbb->tdp_stats.writeAc.access,McPAT_processor->l2array[ref_L2_idx]->unicache.wbb->tdp_stats.readAc.access,0.0,0.0,\
        string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
                
        pseudo_partition.module.push_back(string(module_name));
      }
      
      dimension_t dimension;
      for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
          str_module_it < pseudo_partition.module.end(); str_module_it++)
        dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
      pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
        
      pseudo_package.partition.push_back(string(partition_name));
        
      energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
    }
  }
  for(int L3_idx = 0; L3_idx < XML_interface.sys.number_of_L3s; L3_idx++)
  {
    pseudo_partition_t pseudo_partition;
    
    int ref_L3_idx;

    if(XML_interface.sys.homogeneous_L3s)
      ref_L3_idx = 0;
    else
      ref_L3_idx = L3_idx;

    sprintf(partition_name,"L3[%d]",L3_idx);

    // refer to L3 cache variables to setup L3-wide parameters
    assert(McPAT_processor->l3array[ref_L3_idx]&&McPAT_processor->l3array[ref_L3_idx]->unicache.caches);
    InputParameter McPAT_input = McPAT_processor->l3array[ref_L3_idx]->unicache.caches->l_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.L3[ref_L3_idx].clockrate*1e6,"llc","n/a");
      
    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
    pseudo_partition.package = "mcpat";
    
    if(McPAT_processor->l3array[ref_L3_idx]->unicache.caches) // data cache
    {
      sprintf(module_name,"L3[%d].%s",L3_idx,McPAT_processor->l3array[ref_L3_idx]->unicache.caches->name.c_str());
      populate_EI<ArrayST>(McPAT_config,ref_L3_idx,L3_idx,McPAT_processor->l3array[ref_L3_idx]->cachep.clockRate,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.caches,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.caches->l_ip,\
      1.0,1.0,1.0,false,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.caches->tdp_stats.readAc.hit+(McPAT_processor->l3array[ref_L3_idx]->cachep.dir_ty==SBT?(McPAT_processor->l3array[ref_L3_idx]->homenode_stats_t.readAc.hit*McPAT_processor->l3array[ref_L3_idx]->dir_overhead):0.0),McPAT_processor->l3array[ref_L3_idx]->unicache.caches->tdp_stats.writeAc.access+(McPAT_processor->l3array[ref_L3_idx]->cachep.dir_ty==SBT?(McPAT_processor->l3array[ref_L3_idx]->homenode_stats_t.writeAc.hit*McPAT_processor->l3array[ref_L3_idx]->dir_overhead+McPAT_processor->l3array[ref_L3_idx]->homenode_stats_t.writeAc.miss):0.0),0.0,McPAT_processor->l3array[ref_L3_idx]->unicache.caches->tdp_stats.readAc.miss+(McPAT_processor->l3array[ref_L3_idx]->cachep.dir_ty==SBT?(McPAT_processor->l3array[ref_L3_idx]->homenode_stats_t.readAc.miss+McPAT_processor->l3array[ref_L3_idx]->homenode_stats_t.writeAc.miss-McPAT_processor->l3array[ref_L3_idx]->homenode_stats_t.readAc.hit*(McPAT_processor->l3array[ref_L3_idx]->dir_overhead-1.0)-McPAT_processor->l3array[ref_L3_idx]->homenode_stats_t.writeAc.hit*(McPAT_processor->l3array[ref_L3_idx]->dir_overhead-1.0)):0.0),McPAT_processor->l3array[ref_L3_idx]->unicache.caches->tdp_stats.writeAc.miss,\
      string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    if(McPAT_processor->l3array[ref_L3_idx]->unicache.missb) // data cache missbuffer
    {
      sprintf(module_name,"L3[%d].%s",L3_idx,McPAT_processor->l3array[ref_L3_idx]->unicache.missb->name.c_str());
      populate_EI<ArrayST>(McPAT_config,ref_L3_idx,L3_idx,McPAT_processor->l3array[ref_L3_idx]->cachep.clockRate,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.missb,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.missb->l_ip,\
      1.0,1.0,1.0,false,\
      0.0,McPAT_processor->l3array[ref_L3_idx]->unicache.missb->tdp_stats.writeAc.access,McPAT_processor->l3array[ref_L3_idx]->unicache.missb->tdp_stats.readAc.access,0.0,0.0,\
      string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    if(McPAT_processor->l3array[ref_L3_idx]->unicache.ifb) // data cache fillbuffer
    {
      sprintf(module_name,"L3[%d].%s",L3_idx,McPAT_processor->l3array[ref_L3_idx]->unicache.ifb->name.c_str());
      populate_EI<ArrayST>(McPAT_config,ref_L3_idx,L3_idx,McPAT_processor->l3array[ref_L3_idx]->cachep.clockRate,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.ifb,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.ifb->l_ip,\
      1.0,1.0,1.0,false,\
      0.0,McPAT_processor->l3array[ref_L3_idx]->unicache.ifb->tdp_stats.writeAc.access,McPAT_processor->l3array[ref_L3_idx]->unicache.ifb->tdp_stats.readAc.access,0.0,0.0,\
      string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    if(McPAT_processor->l3array[ref_L3_idx]->unicache.prefetchb) // data cache prefetchbuffer
    {
      sprintf(module_name,"L3[%d].%s",L3_idx,McPAT_processor->l3array[ref_L3_idx]->unicache.prefetchb->name.c_str());
      populate_EI<ArrayST>(McPAT_config,ref_L3_idx,L3_idx,McPAT_processor->l3array[ref_L3_idx]->cachep.clockRate,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.prefetchb,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.prefetchb->l_ip,\
      1.0,1.0,1.0,false,\
      0.0,McPAT_processor->l3array[ref_L3_idx]->unicache.prefetchb->tdp_stats.writeAc.access,McPAT_processor->l3array[ref_L3_idx]->unicache.prefetchb->tdp_stats.readAc.access,0.0,0.0,\
      string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    if(McPAT_processor->l3array[ref_L3_idx]->unicache.wbb) // data cache writebackbuffer
    {
      sprintf(module_name,"L3[%d].%s",L3_idx,McPAT_processor->l3array[ref_L3_idx]->unicache.wbb->name.c_str());
      populate_EI<ArrayST>(McPAT_config,ref_L3_idx,L3_idx,McPAT_processor->l3array[ref_L3_idx]->cachep.clockRate,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.wbb,\
      McPAT_processor->l3array[ref_L3_idx]->unicache.wbb->l_ip,\
      1.0,1.0,1.0,false,\
      0.0,McPAT_processor->l3array[ref_L3_idx]->unicache.wbb->tdp_stats.writeAc.access,McPAT_processor->l3array[ref_L3_idx]->unicache.wbb->tdp_stats.readAc.access,0.0,0.0,\
      string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    
    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
     
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }

  // L1 directory
  for(int L1dir_idx = 0; L1dir_idx < XML_interface.sys.number_of_L1Directories; L1dir_idx++)
  {
    pseudo_partition_t pseudo_partition;
    
    int ref_L1dir_idx;

    if(XML_interface.sys.homogeneous_L1Directories)
      ref_L1dir_idx = 0;
    else
      ref_L1dir_idx = L1dir_idx;

    sprintf(partition_name,"L1dir[%d]",L1dir_idx);

    // refer to L1 directory variables to setup L1dir-wide parameters
    assert(McPAT_processor->l1dirarray[ref_L1dir_idx]&&McPAT_processor->l1dirarray[ref_L1dir_idx]->unicache.caches);
    InputParameter McPAT_input = McPAT_processor->l1dirarray[ref_L1dir_idx]->unicache.caches->l_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.L1Directory[ref_L1dir_idx].clockrate*1e6,"llc","n/a");

    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
    pseudo_partition.package = "mcpat";

    if(McPAT_processor->l1dirarray[ref_L1dir_idx]->unicache.caches) // L1 directory - coherent cache
    {
      sprintf(module_name,"L1dir[%d].%s",L1dir_idx,McPAT_processor->l1dirarray[ref_L1dir_idx]->unicache.caches->name.c_str());
      populate_EI<ArrayST>(McPAT_config,ref_L1dir_idx,L1dir_idx,McPAT_processor->l1dirarray[ref_L1dir_idx]->cachep.clockRate,\
      McPAT_processor->l1dirarray[ref_L1dir_idx]->unicache.caches,\
      McPAT_processor->l1dirarray[ref_L1dir_idx]->unicache.caches->l_ip,\
      1.0,1.0,1.0,false,\
      0.0,McPAT_processor->l1dirarray[ref_L1dir_idx]->unicache.caches->tdp_stats.writeAc.access,McPAT_processor->l1dirarray[ref_L1dir_idx]->unicache.caches->tdp_stats.readAc.access,0.0,0.0,\
      string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    
    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
      
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }
  for(int L2dir_idx = 0; L2dir_idx < XML_interface.sys.number_of_L2Directories; L2dir_idx++)
  {
    pseudo_partition_t pseudo_partition;
    
    int ref_L2dir_idx;

    if(XML_interface.sys.homogeneous_L2Directories)
      ref_L2dir_idx = 0;
    else
      ref_L2dir_idx = L2dir_idx;

    sprintf(partition_name,"L2dir[%d]",L2dir_idx);

    // refer to L2 directory variables to setup L2dir-wide parameters
    assert(McPAT_processor->l2dirarray[ref_L2dir_idx]&&McPAT_processor->l2dirarray[ref_L2dir_idx]->unicache.caches);
    InputParameter McPAT_input = McPAT_processor->l2dirarray[ref_L2dir_idx]->unicache.caches->l_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.L2Directory[ref_L2dir_idx].clockrate*1e6,"llc","n/a");

    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
    pseudo_partition.package = "mcpat";

    if(McPAT_processor->l2dirarray[ref_L2dir_idx]->unicache.caches) // L2 directory - coherent cache
    {
      sprintf(module_name,"L2dir[%d].%s",L2dir_idx,McPAT_processor->l2dirarray[ref_L2dir_idx]->unicache.caches->name.c_str());
      populate_EI<ArrayST>(McPAT_config,ref_L2dir_idx,L2dir_idx,McPAT_processor->l2dirarray[ref_L2dir_idx]->cachep.clockRate,\
      McPAT_processor->l2dirarray[ref_L2dir_idx]->unicache.caches,\
      McPAT_processor->l2dirarray[ref_L2dir_idx]->unicache.caches->l_ip,\
      1.0,1.0,1.0,false,\
      0.0,McPAT_processor->l2dirarray[ref_L2dir_idx]->unicache.caches->tdp_stats.writeAc.access,McPAT_processor->l2dirarray[ref_L2dir_idx]->unicache.caches->tdp_stats.readAc.access,0.0,0.0,\
      string(partition_name),string(module_name),ARRAY,ARRAY_CACHE);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    
    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
      
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }
  if((XML_interface.sys.mc.number_mcs > 0)&&(XML_interface.sys.mc.memory_channels_per_mc > 0))
  {
    pseudo_partition_t pseudo_partition;

    sprintf(partition_name,"memory_controller");
    
    // refer to memory controller variables to setup MC-wide parameters
    assert(McPAT_processor->mc);
    InputParameter McPAT_input = McPAT_processor->mc->interface_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.mc.mc_clock*1e6,"uncore","n/a");

    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
    pseudo_partition.package = "mcpat";

    if(McPAT_processor->mc) // memory controller
    {
      // breakdown into individual MCs
      int num_mcs = XML_interface.sys.mc.number_mcs;
      for(int mc_idx = 0; mc_idx < num_mcs; mc_idx++)
      {
        McPAT_processor->mc->mcp.num_mcs = 1;
        McPAT_processor->mc->XML->sys.mc.number_mcs = 1;
        if(McPAT_processor->mc->frontend)
        {
          McPAT_processor->mc->frontend->mcp.num_mcs = 1;
          McPAT_processor->mc->frontend->XML->sys.mc.number_mcs = 1;
          
          sprintf(module_name,"mc[%d].frontend",mc_idx);
          populate_EI<MCFrontEnd>(McPAT_config,0,0,McPAT_processor->mc->frontend->mcp.clockRate,\
          McPAT_processor->mc->frontend, McPAT_processor->mc->frontend->interface_ip,\
          1.0,1.0,1.0,false,\
          1.0,1.0,1.0,0.0,0.0,\
          string(partition_name),string(module_name),MEMORY_CONTROLLER,MEMORY_CONTROLLER_FRONTEND);
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->mc->transecEngine)
        {
          McPAT_processor->mc->transecEngine->mcp.num_mcs = 1;
        
          sprintf(module_name,"mc[%d].transecEngine",mc_idx);
          populate_EI<MCBackend>(McPAT_config,0,0,McPAT_processor->mc->mcp.clockRate,\
          McPAT_processor->mc->transecEngine, McPAT_processor->mc->transecEngine->l_ip,\
          1.0,1.0,1.0,false,\
          1.0,1.0,1.0,0.0,0.0,\
          string(partition_name),string(module_name),MEMORY_CONTROLLER,MEMORY_CONTROLLER_BACKEND);
          pseudo_partition.module.push_back(string(module_name));
        }
        if(McPAT_processor->mc->PHY)
        {
          McPAT_processor->mc->PHY->mcp.num_mcs = 1;
        
          sprintf(module_name,"mc[%d].PHY",mc_idx);
          populate_EI<MCPHY>(McPAT_config,0,0,McPAT_processor->mc->mcp.clockRate,\
          McPAT_processor->mc->PHY, McPAT_processor->mc->PHY->l_ip,\
          1.0,1.0,1.0,false,\
          1.0,1.0,1.0,0.0,0.0,\
          string(partition_name),string(module_name),MEMORY_CONTROLLER,MEMORY_CONTROLLER_PHY);
          pseudo_partition.module.push_back(string(module_name));
        }
      }
    }
    
    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
      
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }
  if(XML_interface.sys.flashc.number_mcs > 0)
  {
    /*TODO*/
    EI_ERROR("McPAT","McPAT doesn't support flash_controller");

    pseudo_partition_t pseudo_partition;

    sprintf(partition_name,"flash_controller");

    // refer to flash controller variables to setup FC-wide parameters
    assert(McPAT_processor->flashcontroller);
    InputParameter McPAT_input = McPAT_processor->flashcontroller->interface_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.flashc.mc_clock*1e6,"uncore","n/a");

    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
    pseudo_partition.package = "mcpat";

    if(McPAT_processor->flashcontroller) // flash controller
    {
      populate_EI<FlashController>(McPAT_config,0,0,McPAT_processor->flashcontroller->fcp.clockRate,\
      McPAT_processor->flashcontroller,\
      McPAT_processor->flashcontroller->interface_ip,\
      XML_interface.sys.flashc.number_mcs,1.0,1.0,false,\
      XML_interface.sys.flashc.number_mcs*McPAT_processor->flashcontroller->fcp.duty_cycle,0.0,1.0,0.0,0.0,\
      string(partition_name),"flash_controller" /* fail: McPAT doesn't name a FC */,FLASH_CONTROLLER);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    
    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
      
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }
  if(XML_interface.sys.niu.number_units > 0)
  {
    /*TODO*/
    EI_ERROR("McPAT","McPAT doesn't support niu_controller");
    
    pseudo_partition_t pseudo_partition;

    sprintf(partition_name,"niu");

    // refer to network interface unit variables to setup NIU-wide parameters
    assert(McPAT_processor->niu);
    InputParameter McPAT_input = McPAT_processor->niu->interface_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.niu.clockrate*1e6,"uncore","n/a");

    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
    pseudo_partition.package = "mcpat";
    
    if(McPAT_processor->niu) // NIU controller
    {
      populate_EI<NIUController>(McPAT_config,0,0,McPAT_processor->niu->niup.clockRate,\
      McPAT_processor->niu,\
      McPAT_processor->niu->interface_ip,\
      XML_interface.sys.niu.number_units,1.0,1.0,false,\
      XML_interface.sys.niu.number_units,0.0,1.0,0.0,0.0,\
      string(partition_name),"niu" /* fail: McPAT doesn't name an NIUC */,NIU_CONTROLLER);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    
    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
      
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }
  if(XML_interface.sys.pcie.number_units > 0)
  {
    /*TODO*/
    EI_ERROR("McPAT","McPAT doesn't support pcie_controller");
    
    pseudo_partition_t pseudo_partition;

    sprintf(partition_name,"pcie");

    // refer to L1 directory variables to setup L1dir-wide parameters
    assert(McPAT_processor->pcie);
    InputParameter McPAT_input = McPAT_processor->pcie->interface_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.pcie.clockrate*1e6,"uncore","n/a");

    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
    pseudo_partition.package = "mcpat";
    
    if(McPAT_processor->pcie) // PCIe controller
    {
      populate_EI<PCIeController>(McPAT_config,0,0,McPAT_processor->pcie->pciep.clockRate,\
      McPAT_processor->pcie,\
      McPAT_processor->pcie->interface_ip,\
      XML_interface.sys.pcie.number_units,1.0,1.0,false,\
      XML_interface.sys.pcie.number_units,0.0,1.0,0.0,0.0,\
      string(partition_name),"pcie" /* fail: McPAT doesn't name a PCIeC */,PCIE_CONTROLLER);
              
      pseudo_partition.module.push_back(string(module_name));
    }
    
    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
      
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }
  for(int NoC_idx = 0; NoC_idx < XML_interface.sys.number_of_NoCs; NoC_idx++)
  {
    pseudo_partition_t pseudo_partition;
    
    int ref_NoC_idx;

    if(XML_interface.sys.homogeneous_NoCs)
      ref_NoC_idx = 0;
    else
      ref_NoC_idx = NoC_idx;

    sprintf(partition_name,"NoC[%d]",NoC_idx);

    // refer to NoC variables to setup NoC-wide parameters
    assert(McPAT_processor->nocs[ref_NoC_idx]);
    InputParameter McPAT_input = McPAT_processor->nocs[ref_NoC_idx]->interface_ip;

    EI_partition_config(McPAT_config,partition_name,McPAT_input,XML_interface.sys.NoC[ref_NoC_idx].clockrate*1e6,"uncore","n/a");

    pseudo_partition.queue.create<dimension_t>("dimension",1);
    pseudo_partition.queue.create<power_t>("TDP",1);
    pseudo_partition.queue.create<power_t>("power",default_queue_size);
    pseudo_partition.queue.create<double>("temperature",1);
    pseudo_partition.queue.push<double>(MAX_TIME,MAX_TIME,"temperature",(double)XML_interface.sys.temperature);
    pseudo_partition.package = "mcpat";
    
    /* fail: NoC doesn't name an NoC */
    // breakdown into individual nodes
    McPAT_processor->nocs[ref_NoC_idx]->area.set_area(McPAT_processor->nocs[ref_NoC_idx]->area.get_area()/McPAT_processor->nocs[ref_NoC_idx]->nocdynp.total_nodes);
    for(int node_idx = 0; node_idx < McPAT_processor->nocs[ref_NoC_idx]->nocdynp.total_nodes; node_idx++)
    {
      sprintf(module_name,"NoC[%d].node[%d]",NoC_idx,node_idx);
      populate_EI<NoC>(McPAT_config,ref_NoC_idx,NoC_idx,McPAT_processor->nocs[ref_NoC_idx]->nocdynp.clockRate,\
      McPAT_processor->nocs[ref_NoC_idx],\
      McPAT_processor->nocs[ref_NoC_idx]->interface_ip,\
      1.0,1.0,1.0,false,\
      1.0,0.0,1.0,0.0,0.0,\
      string(partition_name),string(module_name),NETWORK,XML_interface.sys.NoC[0].type?NETWORK_ROUTER:NETWORK_BUS);
      pseudo_partition.module.push_back(string(module_name));
    }    
    McPAT_processor->nocs[ref_NoC_idx]->nocdynp.horizontal_nodes = 1;
    McPAT_processor->nocs[ref_NoC_idx]->nocdynp.vertical_nodes = 1;
    McPAT_processor->nocs[ref_NoC_idx]->nocdynp.total_nodes = 1;
    McPAT_processor->nocs[ref_NoC_idx]->XML->sys.NoC[ref_NoC_idx].horizontal_nodes = 1;
    McPAT_processor->nocs[ref_NoC_idx]->XML->sys.NoC[ref_NoC_idx].vertical_nodes = 1;    

    dimension_t dimension;
    for(vector<string>::iterator str_module_it = pseudo_partition.module.begin();\
        str_module_it < pseudo_partition.module.end(); str_module_it++)
      dimension.area += energy_introspector->module.find(*str_module_it)->second.energy_library->get_area();
    pseudo_partition.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
      
    pseudo_package.partition.push_back(string(partition_name));
      
    energy_introspector->partition.insert(pair<string,pseudo_partition_t>(string(partition_name),pseudo_partition));
  }

  dimension_t dimension;
  for(vector<string>::iterator str_partition_it = pseudo_package.partition.begin();\
      str_partition_it < pseudo_package.partition.end(); str_partition_it++)
    dimension.area += energy_introspector->pull_data<dimension_t>(0.0,"partition",*str_partition_it,"dimension").area;
  pseudo_package.queue.push<dimension_t>(MAX_TIME,MAX_TIME,"dimension",dimension);
  
  energy_introspector->package.insert(pair<string,pseudo_package_t>("mcpat",pseudo_package));
  
  fflush(McPAT_config);
  fclose(McPAT_config);
}
