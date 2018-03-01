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

#include "THERMALLIB_HOTSPOT.h"

using namespace EI;

THERMALLIB_HOTSPOT::THERMALLIB_HOTSPOT(string component_name, parameters_t *parameters, energy_introspector_t *ei)
{
  // thermal_library_t parameters
  name = "hotspot";
  energy_introspector = ei;
  
  pseudo_package_t &pseudo_package = energy_introspector->package.find(component_name)->second;
  parameters_component_t &parameters_package = parameters->get_package(component_name);
  
  /* NOTE: frequency not necessary */
  /*
  set_variable(clock_frequency,parameters_package,"clock_frequency",0.0);
  thermal_config.base_proc_freq = clock_frequency;
  */

  // THERMALLIB_HOTSPOT parameters
  thermal_config = default_thermal_config();
  package_config = default_package_config();

  string thermal_analysis_option;
  set_variable(thermal_analysis_option,parameters_package,"thermal_analysis","transient");
  
  if(!stricmp(thermal_analysis_option,"transient"))
    thermal_analysis = TRANSIENT_ANALYSIS;
  else if(!stricmp(thermal_analysis_option,"steady")||!stricmp(thermal_analysis_option,"steady_state"))
    thermal_analysis = STEADY_STATE_ANALYSIS;
  else
    EI_ERROR("HotSpot","unknown thermal_analysis "+thermal_analysis_option+" - transient or steady");

  set_variable(thermal_config.thermal_threshold,parameters_package,"thermal_threshold",273.15+81.8);
  set_variable(thermal_config.t_chip,parameters_package,"chip_thickness",0.15e-3);
  set_variable(thermal_config.k_chip,parameters_package,"chip_thermal_conductivity",100.0);
  set_variable(thermal_config.p_chip,parameters_package,"chip_heat",1.75e6);
  set_variable(thermal_config.c_convec,parameters_package,"heatsink_convection_capacitance",140.4);
  set_variable(thermal_config.r_convec,parameters_package,"heatsink_convection_resistance",0.1);
  set_variable(thermal_config.s_sink,parameters_package,"heatsink_side",60e-3);
  set_variable(thermal_config.t_sink,parameters_package,"heatsink_thickness",6.9e-3);
  set_variable(thermal_config.k_sink,parameters_package,"heatsink_thermal_conductivity",400.0);
  set_variable(thermal_config.p_sink,parameters_package,"heatsink_heat",3.55e6);
  set_variable(thermal_config.s_spreader,parameters_package,"spreader_side",30e-3);
  set_variable(thermal_config.t_spreader,parameters_package,"spreader_thickness",1e-3);
  set_variable(thermal_config.k_spreader,parameters_package,"spreader_thermal_conductivity",400.0);
  set_variable(thermal_config.p_spreader,parameters_package,"spreader_heat",3.55e6);
  set_variable(thermal_config.t_interface,parameters_package,"interface_thickness",20e-6);
  set_variable(thermal_config.k_interface,parameters_package,"interface_thermal_conductivity",4.0);
  set_variable(thermal_config.p_interface,parameters_package,"interface_heat",4.0e6);
  set_variable(thermal_config.n_metal,parameters_package,"metal_layers",8);
  set_variable(thermal_config.t_metal,parameters_package,"metal_layer_thickness",10e-6);
  set_variable(thermal_config.t_c4,parameters_package,"c4_thickness",0.0001);
  set_variable(thermal_config.s_c4,parameters_package,"c4_side",20e-6);
  set_variable(thermal_config.n_c4,parameters_package,"c4_pads",400);
  set_variable(thermal_config.s_sub,parameters_package,"substrate_side",0.021);
  set_variable(thermal_config.t_sub,parameters_package,"substrate_thickness",0.001);
  set_variable(thermal_config.s_solder,parameters_package,"solder_side",0.021);
  set_variable(thermal_config.t_solder,parameters_package,"solder_thickness",0.00094);
  set_variable(thermal_config.s_pcb,parameters_package,"pcb_side",0.1);
  set_variable(thermal_config.t_pcb,parameters_package,"pcb_thickness",0.002);
  set_variable(thermal_config.r_convec_sec,parameters_package,"secondary_convection_resistance",50.0);
  set_variable(thermal_config.c_convec_sec,parameters_package,"secondary_convection_capacitance",40.0);
  set_variable(thermal_config.grid_rows,parameters_package,"grid_rows",128);
  set_variable(thermal_config.grid_cols,parameters_package,"grid_columns",128);
  set_variable(thermal_config.leakage_mode,parameters_package,"leakage_mode",0); // how are the modes indexed?
  
  string option;
  set_variable(option,parameters_package,"secondary_model","false");
  thermal_config.model_secondary = stricmp(option,"false");

  set_variable(option,parameters_package,"dtm_used","false");
  thermal_config.dtm_used = stricmp(option,"false");  

  set_variable(option,parameters_package,"block_omit_lateral","false");
  thermal_config.block_omit_lateral = stricmp(option,"false");

  set_variable(option,parameters_package,"leakage_used","false");
  thermal_config.leakage_used = stricmp(option,"false");

  set_variable(option,parameters_package,"package_model_used","false");
  thermal_config.package_model_used = stricmp(option,"false");

  // Copy of HotSpot error checks
  if((thermal_config.t_chip <= 0)||(thermal_config.s_sink <= 0)||(thermal_config.t_sink <= 0)
     ||(thermal_config.s_spreader <= 0)||(thermal_config.t_spreader <= 0)||(thermal_config.t_interface <= 0))
    EI_ERROR("HotSpot","chip and package dimensions should be greater than zero");
  if((thermal_config.t_metal <= 0)||(thermal_config.n_metal <= 0)||(thermal_config.t_c4 <= 0)
     ||(thermal_config.s_c4 <= 0)||(thermal_config.n_c4 <= 0)||(thermal_config.s_sub <= 0)||(thermal_config.t_sub <= 0)
     ||(thermal_config.s_solder <= 0)||(thermal_config.t_solder <= 0)||(thermal_config.s_pcb <= 0)
     ||(thermal_config.t_solder <= 0)||(thermal_config.r_convec_sec <= 0)||(thermal_config.c_convec_sec <= 0))
    EI_ERROR("HotSpot","secondary heat transfer layer dimension should be greater than zero"); 
  if(thermal_config.leakage_used == 1)
    EI_WARNING("HotSpot","transient leakage iteration is not supported in this release -- all transient results are without thermal-leakage loop");
  if((thermal_config.model_secondary == 1)&&(!strcasecmp(thermal_config.model_type, BLOCK_MODEL_STR)))
    EI_ERROR("HotSpot","secondary heat transfer path is supported only in the grid mode");
  if((thermal_config.thermal_threshold < 0)||(thermal_config.c_convec < 0)||(thermal_config.r_convec < 0)
     || (thermal_config.ambient < 0)||(thermal_config.base_proc_freq <= 0)||(thermal_config.sampling_intvl <= 0))
    EI_ERROR("HotSpot","invalid thermal simulation parameters");
  if(thermal_config.grid_rows <= 0||thermal_config.grid_cols <= 0||
    (thermal_config.grid_rows & (thermal_config.grid_rows-1))||
    (thermal_config.grid_cols & (thermal_config.grid_cols-1)))
    EI_ERROR("HotSpot","grid rows and columns should both be powers of two");

  if(thermal_config.package_model_used)
  {
    set_variable(option,parameters_package,"natural_convection","false");
    package_config.natural_convec = stricmp(option,"false");

    set_variable(option,parameters_package,"flow_type","side");
    if(!stricmp(option,"side"))
      package_config.flow_type = 0;
    else if(!stricmp(option,"top"))
      package_config.flow_type = 1;
    else
      EI_ERROR("HotSpot","unknown flow_type "+option);

    set_variable(option,parameters_package,"sink_type","fin_channel");
    if(!stricmp(option,"fin_channel"))
      package_config.sink_type = 0;
    else if(!stricmp(option,"pinfin"))
      package_config.sink_type = 1;
    else
      EI_ERROR("HotSpot","unknown sink_type "+option);

    set_variable(package_config.fin_height,parameters_package,"fin_height",0.03);
    set_variable(package_config.fin_width,parameters_package,"fin_width",0.001);
    set_variable(package_config.channel_width,parameters_package,"channel_width",0.002);
    set_variable(package_config.pin_height,parameters_package,"pin_height",0.02);
    set_variable(package_config.pin_diam,parameters_package,"pin_diameter",0.002);
    set_variable(package_config.pin_dist,parameters_package,"pin_distance",0.005);
    set_variable(package_config.fan_radius,parameters_package,"fan_radius",0.03);
    set_variable(package_config.motor_radius,parameters_package,"motor_radius",0.01);
    set_variable(package_config.rpm,parameters_package,"rpm",1000);

    if(!package_config.natural_convec)
      calculate_flow(&convection,&package_config,&thermal_config);
    else
      calc_natural_convec(&convection,&package_config,&thermal_config,thermal_config.ambient+SMALL_FOR_CONVEC);

    thermal_config.r_convec = convection.r_th;

    if(thermal_config.r_convec<R_CONVEC_LOW||thermal_config.r_convec>R_CONVEC_HIGH)
      EI_WARNING("HotSpot","heatsink convection resistance is not realistic, double-check package settings");
  }

  /* Sampling interval is dynamically adjusted in THERMALLIB_HOTSPOT::compute() function.
     Clock frequency really does not affect the result but complicates parameters setup.
    thermal_config.sampling_intvl = 0.0;*/

  set_variable(option,parameters_package,"model_type","grid");
  if(!stricmp(option,"block"))
    strcpy(thermal_config.model_type,"block");
  else if(!stricmp(option,"grid"))
    strcpy(thermal_config.model_type,"grid");
  else
    EI_ERROR("HotSpot","unknown model "+option);

  set_variable(option,parameters_package,"grid_map_mode","avg");
  if(!stricmp(option,"center"))
    strcpy(thermal_config.grid_map_mode,"center");
  else if(!stricmp(option,"avg")||!stricmp(option,"average"))
    strcpy(thermal_config.grid_map_mode,"avg");
  else if(!stricmp(option,"min")||!stricmp(option,"minimum"))
    strcpy(thermal_config.grid_map_mode,"min");
  else if(!stricmp(option,"max")||!stricmp(option,"maximum"))
    strcpy(thermal_config.grid_map_mode,"max");
  else
    EI_ERROR("HotSpot","unknown grid_map_mode "+option);  

  set_variable(thermal_config.ambient,parameters_package,"ambient_temperature",273.15+30.0,true);
  set_variable(thermal_config.init_temp,parameters_package,"temperature",thermal_config.ambient);

  // following lines are part of hotspot main, interfacing with energy introspector
  // create floorplan
  if(pseudo_package.partition.size() == 0)
    EI_ERROR("HotSpot","pseudo package "+pseudo_package.name+" is not partitioned");

  flp = flp_alloc_init_mem(pseudo_package.partition.size());

  // update floorplan
  vector<string>::iterator partition_str_it = pseudo_package.partition.begin();
  for(int i = 0; (i < flp->n_units)||(partition_str_it < pseudo_package.partition.end()); i++)
  {
    dimension_t flp_dimension = energy_introspector->pull_data<dimension_t>(0.0,"partition",*partition_str_it,"dimension");
  
    // HotSpot uses WxLxH, but the EI uses LxWxH
    flp->units[i].width = flp_dimension.length;
    flp->units[i].height = flp_dimension.width;
    flp->units[i].leftx = flp_dimension.x_left;
    flp->units[i].bottomy = flp_dimension.y_bottom;
    strcpy(flp->units[i].name,partition_str_it->c_str());
    partition_str_it++;
  }

  // connectivity between floorplans
  for(int i = 0; i < flp->n_units; i++)
  {
    for(int j = 0; j < flp->n_units; j++)
      flp->wire_density[i][j] = 1.0;
  }

  flp_translate(flp,0,0);

  //model = alloc_RC_model(&thermal_config,flp); // manually done below

  model = (RC_model_t*)calloc(1,sizeof(RC_model_t));
  if(!model)
    EI_ERROR("HotSpot","cannot allocate a thermal model");

  // grid model supports manually configuring layer stacks (possibly 3D)
  if(!stricmp(thermal_config.model_type,"grid"))
  {
    // the following is the part of alloc_grid_model()
    grid_model_t *grid_model = (grid_model_t*)calloc(1,sizeof(grid_model_t));
    if(!grid_model)
      EI_ERROR("HotSpot","cannot allocate a grid model");

    grid_model->config = thermal_config;
    grid_model->rows = thermal_config.grid_rows;
    grid_model->cols = thermal_config.grid_cols;

    if(!stricmp(thermal_config.grid_map_mode,"avg"))
      grid_model->map_mode = GRID_AVG;
    else if(!stricmp(thermal_config.grid_map_mode,"min"))
      grid_model->map_mode = GRID_MIN;
    else if(!stricmp(thermal_config.grid_map_mode,"max"))
      grid_model->map_mode = GRID_MAX;
    else // error already check above
      grid_model->map_mode = GRID_CENTER;

    // count the number of layers
    int num_layers = 0;
    string layer_name = parameters_package.get_option("layer",true);
    while(stricmp(layer_name,"n/a"))
    {
      num_layers++;
      layer_name = parameters_package.get_option("layer",true);
    }
    
    // create HotSpot layers
    if(num_layers > 0)
    {
      if(num_layers < DEFAULT_CHIP_LAYERS)
        EI_ERROR("HotSpot","number of layers less than the default");
      
      grid_model->n_layers = num_layers;
    }
    else
      grid_model->n_layers = DEFAULT_CHIP_LAYERS;
    
    if(grid_model->config.model_secondary)
    {
      grid_model->n_layers += SEC_CHIP_LAYERS;
      grid_model->layers = (layer_t*)calloc(grid_model->n_layers+DEFAULT_PACK_LAYERS+SEC_PACK_LAYERS,sizeof(layer_t)); 
    }
    else
      grid_model->layers = (layer_t*)calloc(grid_model->n_layers+DEFAULT_PACK_LAYERS,sizeof(layer_t));
    
    if(!grid_model->layers)
      EI_ERROR("HotSpot","cannot create package layers");

    // HotSpot needs non-source layers to be partitioned     
    map<string,int> layer_map; // <layer_name,layer_index>
    map<string,string> non_source_layer_partition; // <non_source_layer_index,referred_source_layer_index>
    
    // re-iterate and set layer options
    int layer_index = num_layers-1;
    layer_name = parameters_package.get_option("layer",true);
    while(stricmp(layer_name,"n/a"))    
    {
      grid_model->layers[layer_index].no = layer_index;
            
      set_variable(option,parameters_package,"layer."+layer_name+".lateral_heat_flow","true");
      grid_model->layers[layer_index].has_lateral = stricmp(option,"false");
      
      set_variable(option,parameters_package,"layer."+layer_name+".is_source_layer","false",true);
      grid_model->layers[layer_index].has_power = stricmp(option,"false");
      
      set_variable(grid_model->layers[layer_index].sp,parameters_package,"layer."+layer_name+".heat",0.0,true);
      set_variable(grid_model->layers[layer_index].k,parameters_package,"layer."+layer_name+".resistance",0.0,true);
      grid_model->layers[layer_index].k = 1.0/grid_model->layers[layer_index].k;
      set_variable(grid_model->layers[layer_index].thickness,parameters_package,"layer."+layer_name+".thickness",0.0,true);
      
      if(grid_model->layers[layer_index].has_power) // set floorplans for the source layer
      {
        vector<int> grid_flp_index; // use floorplan index of default flp set above
        for(vector<string>::iterator partition_str_it = pseudo_package.partition.begin();\
            partition_str_it != pseudo_package.partition.end(); partition_str_it++)
        {
          parameters_component_t &parameters_partition = parameters->get_partition(*partition_str_it);
          // find partitions(floorplans) in the layer
          if(!strcmp(parameters_partition.get_option("layer").c_str(),layer_name.c_str())) 
            grid_flp_index.push_back(get_blk_index(flp,(char*)(*partition_str_it).c_str()));
        }
        
        if(grid_flp_index.size() == 0)
          EI_ERROR("HotSpot","source layer "+layer_name+" is not partitioned");
      
        grid_model->layers[layer_index].flp = flp_alloc_init_mem(grid_flp_index.size());
      
        // copy default flp information to layer flps.
        for(int flp_index = 0; flp_index < grid_flp_index.size(); flp_index++)
          grid_model->layers[layer_index].flp->units[flp_index] = flp->units[grid_flp_index[flp_index]];
        
        // TODO: wire density
        for(int i = 0; i < grid_model->layers[layer_index].flp->n_units; i++)
          for(int j = 0; j < grid_model->layers[layer_index].flp->n_units; j++)
            grid_model->layers[layer_index].flp->wire_density[i][j] = 1.0;
      
        flp_translate(grid_model->layers[layer_index].flp,0,0);
        grid_flp_index.clear();
      }
      else
      {
        string use_layer_partition;
        set_variable(use_layer_partition,parameters_package,"layer."+layer_name+".use_layer_partition","n/a",true);
        non_source_layer_partition.insert(pair<string,string>(layer_name,use_layer_partition));
        grid_model->layers[layer_index].flp = NULL;
      }
      
      layer_map.insert(pair<string,int>(layer_name,layer_index));
      layer_name = parameters_package.get_option("layer",true);
      --layer_index;
    }

    if(num_layers > 0)
    {
      // match non-source layer flps with referred source layer flps
      for(map<string,string>::iterator non_source_layer_it = non_source_layer_partition.begin();\
          non_source_layer_it != non_source_layer_partition.end(); non_source_layer_it++)
      {
        if(layer_map.find(non_source_layer_it->second) == layer_map.end())
          EI_ERROR("HotSpot","undefined source layer "+non_source_layer_it->second);
        else
        {
          int non_source_layer_index = layer_map.find(non_source_layer_it->first)->second;
          int source_layer_index = layer_map.find(non_source_layer_it->second)->second;
          
          if(grid_model->layers[non_source_layer_index].flp||!grid_model->layers[source_layer_index].flp)
            EI_ERROR("HotSpot","cannot create floorplans for non-source layer "+non_source_layer_it->first);
          
          grid_model->layers[non_source_layer_index].flp = grid_model->layers[source_layer_index].flp;
        }
      }

      layer_map.clear();
      non_source_layer_partition.clear();

      for(int i = 0; i < grid_model->n_layers; i++)
      {
        if(i == 0)
        {
          grid_model->width = get_total_width(grid_model->layers[i].flp);
          grid_model->height = get_total_height(grid_model->layers[i].flp);
        }
        else
        {
          if(grid_model->width != get_total_width(grid_model->layers[i].flp))
            EI_ERROR("HotSpot","width differ across package layers");
          if(grid_model->height != get_total_height(grid_model->layers[i].flp))
            EI_ERROR("HotSpot","height differ across package layers");
        }
        
        grid_model->has_lcf = true; // to indicate grid layers are manually configured       
        grid_model->layers[i].b2gmap = new_b2gmap(grid_model->rows, grid_model->cols);
        grid_model->layers[i].g2bmap = (glist_t *) calloc(grid_model->layers[i].flp->n_units,sizeof(glist_t));
        if (!grid_model->layers[i].g2bmap)
          EI_ERROR("HotSpot","cannot create grid to block map");
      }
    }
    else
    {
      grid_model->has_lcf = false;
      grid_model->base_n_units = flp->n_units;
      populate_default_layers(grid_model,flp);
    }

    append_package_layers(grid_model);

    for(int i = 0; i < grid_model->n_layers; i++)
      if(grid_model->layers[i].flp)
        grid_model->total_n_blocks += grid_model->layers[i].flp->n_units;

    grid_model->last_steady = new_grid_model_vector(grid_model);
    grid_model->last_trans = new_grid_model_vector(grid_model);
    
    model->type = GRID_MODEL;
    model->grid = grid_model;
    model->config = &model->grid->config;
  }
  else // BLOCK_MODEL
  {
    model->type = BLOCK_MODEL;
    model->block = alloc_block_model(&thermal_config,flp);
    model->config = &model->block->config;
  }

  populate_R_model(model,flp);

  if(thermal_analysis == TRANSIENT_ANALYSIS)
    populate_C_model(model,flp);
    
  temperature = hotspot_vector(model);
  power = hotspot_vector(model);

  // set initial temperature
  if(model->type == GRID_MODEL)
  {
    int base = 0;
    for(int n = 0; n < model->grid->n_layers; n++)
    {
      for(int i = 0; i < flp->n_units; i++)
        temperature[base+i] = thermal_config.init_temp;

      if(model->grid->layers[n].flp)
        base += model->grid->layers[n].flp->n_units;
    }
    if(base != model->grid->total_n_blocks)
      EI_ERROR("HotSpot","total_n_blocks failed to tally");

    for(int i = 0; i < (model->config->model_secondary?(EXTRA_SEC):(EXTRA)); i++)
      temperature[base+i] = thermal_config.init_temp;
  }
  else // BLOCK_MODEL
  {
    for(int n = 0; n < NL; n++)
      for(int i = 0; i < flp->n_units; i++)
        temperature[i+n*flp->n_units] = thermal_config.init_temp;

    for(int i = 0; i < EXTRA; i++)
      temperature[i+NL*flp->n_units] = thermal_config.init_temp;
  }
}


void THERMALLIB_HOTSPOT::deliver_power(string partition_name, power_t partition_power)
{
  bool power_delivered = false;
  
  if(model->type == GRID_MODEL)
  {
    int base = 0;
    for(int layer_index = 0; layer_index < model->grid->n_layers; layer_index++)
    {
      if(model->grid->layers[layer_index].has_power) // source layer
      {
        int flp_index = get_blk_index(model->grid->layers[layer_index].flp,(char*)partition_name.c_str());
        if(flp_index != -1)
        {
          power[base+flp_index] = partition_power.get_total();
          power_delivered = true;
          break;
        }
      }
      base += model->grid->layers[layer_index].flp->n_units;      
    }
  }
  else // BLOCK_MODEL
  {
    int flp_index = get_blk_index(flp,(char*)partition_name.c_str());
    if(flp_index != -1)
    {
      power[flp_index] = partition_power.get_total();
      power_delivered = true;
    }
  }
  
  if(!power_delivered)
    EI_WARNING("HotSpot","no matching floorplan is found for pseudo partition "+partition_name+" in deliver_power()");
}


grid_t<double> THERMALLIB_HOTSPOT::get_thermal_grid(void)
{
  grid_t<double> thermal_grid;

  if(model->type == GRID_MODEL)
  {
    thermal_grid.cell_length = model->grid->width/model->grid->cols;
    thermal_grid.cell_width = model->grid->height/model->grid->rows;
  
    for(int l = 0; l < model->grid->n_layers; l++)
    {
      if(model->grid->layers[l].has_power) // source layer
      {
        for(int r = 0; r < model->grid->config.grid_rows; r++) // HotSpot grid has wrong orientation
        {
          for(int c = 0; c < model->grid->config.grid_cols; c++)
          {
            if(thermal_analysis == TRANSIENT_ANALYSIS)
              thermal_grid.push(/*x*/c,/*y*/model->grid->config.grid_rows-r-1,/*layer*/l,
                                /*grid temp*/model->grid->last_trans->cuboid[l][c][r]);
            else
              thermal_grid.push(/*x*/c,/*y*/model->grid->config.grid_rows-r-1,/*layer*/l,
                                /*grid temp*/model->grid->last_steady->cuboid[l][c][r]);
          }
        }
      }
    }
  }
  else // block model
  {
    thermal_grid.cell_length = get_total_width(flp)/thermal_config.grid_cols;
    thermal_grid.cell_width = get_total_height(flp)/thermal_config.grid_rows;
    
    for(int i = 0; i < flp->n_units; i++)
    {
      int top = thermal_config.grid_rows - tolerant_ceil((flp->units[i].bottomy+flp->units[i].height)/thermal_grid.cell_width);
      int bottom = thermal_config.grid_rows - tolerant_floor((flp->units[i].bottomy)/thermal_grid.cell_width);
      int left = tolerant_floor((flp->units[i].leftx)/thermal_grid.cell_length);
      int right = tolerant_ceil((flp->units[i].leftx+flp->units[i].width)/thermal_grid.cell_length);
      
      for(int r = bottom; r <= top; r++)
        for(int c = left; c <= right; c++)
          thermal_grid.push(c,r,0,temperature[i]);
    }
  }

  return thermal_grid;
}


double THERMALLIB_HOTSPOT::get_partition_temperature(string partition_name)
{
  double partition_temperature = 0.0;
  
  if(model->type == GRID_MODEL)
  {
    int base = 0;
    for(int layer_index = 0; layer_index < model->grid->n_layers; layer_index++)
    {
      if(model->grid->layers[layer_index].has_power) // source layer
      {
        int flp_index = get_blk_index(model->grid->layers[layer_index].flp,(char*)partition_name.c_str());
        if(flp_index != -1)
        {
          partition_temperature = temperature[base+flp_index];
          break;
        }
      }
      base += model->grid->layers[layer_index].flp->n_units;      
    }
  }
  else // BLOCK_MODEL
  {
    int flp_index = get_blk_index(flp,(char*)partition_name.c_str());
    if(flp_index != -1)
      partition_temperature = temperature[flp_index];
  }
  
  if(partition_temperature == 0.0)
    EI_WARNING("HotSpot","no matching floorplan is found for pseudo partition "+partition_name+" in get_partition_temperature()");

  return partition_temperature;
}


double THERMALLIB_HOTSPOT::get_point_temperature(double x, double y, int layer)
{
  double point_temperature = 0.0;
  
  if(model->type == GRID_MODEL)
  {
    int row_index = (int)(y/(model->grid->height/model->grid->rows));
    int col_index = (int)(x/(model->grid->width/model->grid->cols));
    
    if(thermal_analysis == TRANSIENT_ANALYSIS)
      point_temperature = model->grid->last_trans->cuboid[layer][row_index][col_index];
    else
      point_temperature = model->grid->last_steady->cuboid[layer][row_index][col_index];      
  }
  else // block model
  {
    if(layer == 0) // 2D
    {
      for(int i = 0; i < flp->n_units; i++)
      {
        if((x >= flp->units[i].leftx)&&(x <= (flp->units[i].leftx+flp->units[i].width)))
          if((y >= flp->units[i].bottomy)&&(y <= (flp->units[i].bottomy+flp->units[i].height)))
          {
            point_temperature = temperature[i];
            break;
          }
      }
    }
  }
  
  if(point_temperature == 0.0)
  {
    char warning_message[128];
    sprintf(warning_message,"cannot locate the coodinate(/*x*/%lf,/*y*/%lf,/*layer*/%d) in get_point_temperature()",x,y,layer);
    EI_WARNING("HotSpot",warning_message);
  }
  
  return point_temperature;
}


int THERMALLIB_HOTSPOT::get_partition_layer_index(string partition_name)
{
  int layer_index = -1;
  if(model->type == GRID_MODEL)
  {
    int base = 0;
    for(int i = 0; i < model->grid->n_layers; i++)
    {
      if(model->grid->layers[i].has_power) // source layer
      {
        int flp_index = get_blk_index(model->grid->layers[i].flp,(char*)partition_name.c_str());
        if(flp_index != -1)
        {
          layer_index = i;
          break;
        }
      }
      base += model->grid->layers[i].flp->n_units;      
    }
  }
  else
  {
    int flp_index = get_blk_index(flp,(char*)partition_name.c_str());
    if(flp_index != -1)
      layer_index = 0;
  }
  
  if(layer_index == -1)
    EI_WARNING("HotSpot","no matching floorplan is found for pseudo partition "+partition_name+" in get_partition_layer_index()");
  return 0;
}


void THERMALLIB_HOTSPOT::compute_temperature(double time_tick, double period)
{
  model->config->sampling_intvl = period;

  if(thermal_analysis == TRANSIENT_ANALYSIS)  
    compute_temp(model,power,temperature,model->config->sampling_intvl);
  else
    steady_state_temp(model,power,temperature);
}

