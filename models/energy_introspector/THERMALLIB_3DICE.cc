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

#include "THERMALLIB_3DICE.h"

using namespace EI;

THERMALLIB_3DICE::THERMALLIB_3DICE(string component_name, parameters_t *parameters, energy_introspector_t *ei) 
{
  // thermal_library_t parameters
  name = "3dice";
  energy_introspector = ei;
  
  pseudo_package_t &pseudo_package = energy_introspector->package.find(component_name)->second;
  parameters_component_t &parameters_package = parameters->get_package(component_name);
  
  init_stack_description(&stkd);
  init_analysis(&analysis);
  
  string grid_map_mode;
  set_variable(grid_map_mode,parameters_package,"grid_map_mode","average");
  if(!stricmp(grid_map_mode,"avg")||!stricmp(grid_map_mode,"average"))
    thermal_grid_mapping = AVG_TEMP;
  else if(!stricmp(grid_map_mode,"min")||!stricmp(grid_map_mode,"minimum"))
    thermal_grid_mapping = MIN_TEMP;
  else if(!stricmp(grid_map_mode,"max")||!stricmp(grid_map_mode,"maximum"))
    thermal_grid_mapping = MAX_TEMP;    
  else if(!stricmp(grid_map_mode,"center"))
    thermal_grid_mapping = CENTER_TEMP;  
  else
    EI_ERROR("3DICE","unknown grid_map_mode "+grid_map_mode+" - avg, min, max, or center");

  double ambient_temperature;
  set_variable(ambient_temperature,parameters_package,"ambient_temperature",273.15+30.0,true);

  // stack description - Dimensions
  stkd.Dimensions = alloc_and_init_dimensions();  
  
  double chip_width, chip_length;
  set_variable(chip_length,parameters_package,"chip_length",0.0,true);
  set_variable(chip_width,parameters_package,"chip_width",0.0,true);
  
  if((chip_length > 1.0)||(chip_width > 1.0))
    EI_ERROR("3DICE","incorrect chip_length and/or chip_width (1um = 1e-6)");
  
  stkd.Dimensions->Chip.Length = chip_length*1e6;
  stkd.Dimensions->Chip.Width = chip_width*1e6;
  
  int grid_rows, grid_columns;
  set_variable(grid_rows,parameters_package,"grid_rows",0,true);
  set_variable(grid_columns,parameters_package,"grid_columns",0,true);
  
  stkd.Dimensions->Grid.NRows = grid_rows;
  stkd.Dimensions->Grid.NColumns = grid_columns;
  
  // rows and columns representation is rotated in 3DICE?
  stkd.Dimensions->Cell.ChannelLength = stkd.Dimensions->Cell.FirstWallLength\
  = stkd.Dimensions->Cell.LastWallLength = stkd.Dimensions->Cell.WallLength\
  = stkd.Dimensions->Chip.Length/stkd.Dimensions->Grid.NColumns;
  stkd.Dimensions->Cell.Width = stkd.Dimensions->Chip.Width/stkd.Dimensions->Grid.NRows;
  
  // stack description - MaterialsList
  string material_name = parameters_package.get_option("material",true);

  if(!stricmp(material_name,"n/a"))
    EI_ERROR("3DICE","no stack material description is found");
  else
  {
    while(stricmp(material_name,"n/a"))
    {
      Material *material = alloc_and_init_material();

      material->Id = (char*)material_name.c_str();

      if(find_material_in_list(stkd.MaterialsList,material->Id))
        EI_ERROR("3DICE","Material "+material_name+" is already declared");

      if(!stkd.MaterialsList)
        stkd.MaterialsList = material;
      else
      {
        Material *last_material = stkd.MaterialsList;
        while(last_material->Next)
          last_material = last_material->Next;
        last_material->Next = material;
      }
        
      set_variable(material->ThermalConductivity,parameters_package,"material."+material_name+".thermal_conductivity",0.0,true);
      set_variable(material->VolumetricHeatCapacity,parameters_package,"material."+material_name+".volumetric_heat_capacity",0.0,true);

      material_name = parameters_package.get_option("material",true);
    }
  }

  // stack description - ConventionalHeatSink
  bool conventional_heatsink;
  set_variable(conventional_heatsink,parameters_package,"conventional_heatsink",false);

  if(conventional_heatsink)
  {
    stkd.ConventionalHeatSink = alloc_and_init_conventional_heat_sink();

    set_variable(stkd.ConventionalHeatSink->AmbientHTC,parameters_package,\
                 "conventional_heatsink.heat_transfer_coefficient",1.0e-7);
    stkd.ConventionalHeatSink->AmbientTemperature = ambient_temperature;
  }

  // stack description - Channel
  string microchannel_type;
  set_variable(microchannel_type,parameters_package,"microchannel.type","n/a",true);

  if(!stricmp(microchannel_type,"2rm"))
  {
    stkd.Channel = alloc_and_init_channel();

    stkd.Channel->ChannelModel = TDICE_CHANNEL_MODEL_MC_2RM;

    set_variable(stkd.Channel->Height,parameters_package,\
                 "microchannel.height",100e-6,true);
    if(stkd.Channel->Height > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.height unit (1um = 1e-6)");
    stkd.Channel->Height *= 1e6; // convert to um

    set_variable(stkd.Channel->Length,parameters_package,\
                 "microchannel.channel_length",50e-6,true);
    if(stkd.Channel->Length > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.channel_length unit (1um = 1e-6)");
    stkd.Channel->Length *= 1e6; // convert to um

    double microchannel_wall_length;
    set_variable(microchannel_wall_length,parameters_package,\
                 "microchannel.wall_length",50e-6,true);
    if(microchannel_wall_length > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.wall_length unit (1um = 1e-6)");
    microchannel_wall_length *= 1e6; // convert to um
    stkd.Channel->Pitch = stkd.Channel->Length + microchannel_wall_length;

    stkd.Channel->NChannels = (stkd.Dimensions->Chip.Length/stkd.Channel->Pitch)+0.5;
    
    stkd.Channel->Porosity = stkd.Channel->Length / stkd.Channel->Pitch;
    stkd.Channel->NLayers = NUM_LAYERS_CHANNEL_2RM;
    stkd.Channel->SourceLayerOffset = SOURCE_OFFSET_CHANNEL_2RM;

    set_variable(stkd.Channel->Coolant.FlowRate,parameters_package,\
                 "microchannel.coolant_flow_rate",42.0,true);
    stkd.Channel->Coolant.FlowRate = FLOW_RATE_FROM_MLMIN_TO_UM3SEC(stkd.Channel->Coolant.FlowRate);

    double coolant_heat_transfer_coefficient;
    set_variable(coolant_heat_transfer_coefficient,parameters_package,\
                 "microchannel.coolant_heat_transfer_coefficient",0.0);

    set_variable(stkd.Channel->Coolant.HTCSide,parameters_package,\
                 "microchannel.coolant_heat_transfer_coefficient_side",0.0);
    set_variable(stkd.Channel->Coolant.HTCTop,parameters_package,\
                 "microchannel.coolant_heat_transfer_coefficient_top",coolant_heat_transfer_coefficient);
    set_variable(stkd.Channel->Coolant.HTCBottom,parameters_package,\
                 "microchannel.coolant_heat_transfer_coefficient_bottom",coolant_heat_transfer_coefficient);
    set_variable(stkd.Channel->Coolant.VHC,parameters_package,\
                 "microchannel.coolant_volumetric_heat_capacity",4.172e-12);
    set_variable(stkd.Channel->Coolant.TIn,parameters_package,\
                 "microchannel.coolant_incoming_temperature",ambient_temperature);

    string microchannel_wall_material;
    set_variable(microchannel_wall_material,parameters_package,\
                 "microchannel.wall_material","n/a",true);
    stkd.Channel->WallMaterial = find_material_in_list(stkd.MaterialsList,(char*)microchannel_wall_material.c_str());

    if(!stkd.Channel->WallMaterial)
      EI_ERROR("3DICE","unknown microchannel.wall_material "+microchannel_wall_material);
    else
      stkd.Channel->WallMaterial->Used++;
  }
  else if(!stricmp(microchannel_type,"4rm"))
  {
    stkd.Channel = alloc_and_init_channel();

    stkd.Channel->ChannelModel = TDICE_CHANNEL_MODEL_MC_4RM;

    set_variable(stkd.Channel->Height,parameters_package,\
                 "microchannel.height",100.e-6,true);
    if(stkd.Channel->Height > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.height unit (1um = 1e-6)");
    stkd.Channel->Height *= 1e6; // convert to um
    
    double channel_length;
    set_variable(channel_length,parameters_package,\
                 "microchannel.channel_length",100e-6,true);
    if(channel_length > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.channel_length unit (1um = 1e-6)");

    double wall_length;
    set_variable(wall_length,parameters_package,\
                 "microchannel.wall_length",100e-6,true);
    if(wall_length > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.wall_length unit (1um = 1e-6)");

    double first_wall_length;
    set_variable(first_wall_length,parameters_package,\
                 "microchannel.first_wall_length",wall_length);
    if(first_wall_length > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.first_wall_length unit (1um = 1e-6)");

    double last_wall_length;
    set_variable(last_wall_length,parameters_package,\
                 "microchannel.last_wall_length",wall_length);
    if(last_wall_length > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.last_wall_length unit (1um = 1e-6)");
    
    stkd.Dimensions->Cell.ChannelLength = channel_length*1e6;
    stkd.Dimensions->Cell.FirstWallLength = first_wall_length*1e6;
    stkd.Dimensions->Cell.LastWallLength = last_wall_length*1e6;
    stkd.Dimensions->Cell.WallLength = wall_length*1e6;
    
    CellDimension_t ratio = (stkd.Dimensions->Chip.Length - stkd.Dimensions->Cell.FirstWallLength - stkd.Dimensions->Cell.LastWallLength - stkd.Dimensions->Cell.ChannelLength)/(stkd.Dimensions->Cell.ChannelLength + stkd.Dimensions->Cell.WallLength);
    
    if(ratio != (int)ratio)
      EI_ERROR("3DICE","cell dimensions do not fit the chip length correctly");
      
    stkd.Dimensions->Grid.NColumns = 2*ratio+3;
    
    if((stkd.Dimensions->Grid.NColumns&1) == 0)
      EI_ERROR("3DICE","column number must be odd when channel is declared");
      
    if(stkd.Dimensions->Grid.NColumns < 3)
      EI_ERROR("3DICE","not enough columns");
      
    stkd.Channel->NChannels = (stkd.Dimensions->Grid.NColumns-1)/2;

    stkd.Channel->NLayers = NUM_LAYERS_CHANNEL_4RM;
    stkd.Channel->SourceLayerOffset = SOURCE_OFFSET_CHANNEL_4RM;

    set_variable(stkd.Channel->Coolant.FlowRate,parameters_package,\
                 "microchannel.coolant_flow_rate",42.0,true);

    double coolant_heat_transfer_coefficient;
    set_variable(coolant_heat_transfer_coefficient,parameters_package,\
                 "microchannel.coolant_heat_transfer_coefficient",0.0);

    set_variable(stkd.Channel->Coolant.HTCSide,parameters_package,\
                 "microchannel.coolant_heat_transfer_coefficient_side",coolant_heat_transfer_coefficient);
    set_variable(stkd.Channel->Coolant.HTCTop,parameters_package,\
                 "microchannel.coolant_heat_transfer_coefficient_top",coolant_heat_transfer_coefficient);
    set_variable(stkd.Channel->Coolant.HTCBottom,parameters_package,\
                 "microchannel.coolant_heat_transfer_coefficient_bottom",coolant_heat_transfer_coefficient);
    set_variable(stkd.Channel->Coolant.VHC,parameters_package,\
                 "microchannel.coolant_volumetric_heat_capacity",4.172e-12);
    set_variable(stkd.Channel->Coolant.TIn,parameters_package,\
                 "microchannel.coolant_incoming_temperature",ambient_temperature);

    string microchannel_wall_material;
    set_variable(microchannel_wall_material,parameters_package,\
                 "microchannel.wall_material","n/a",true);
    stkd.Channel->WallMaterial = find_material_in_list(stkd.MaterialsList,(char*)microchannel_wall_material.c_str());

    if(!stkd.Channel->WallMaterial)
      EI_ERROR("3DICE","unknown microchannel.wall_material "+microchannel_wall_material);
    else
      stkd.Channel->WallMaterial->Used++;
  }
  else if(!stricmp(microchannel_type,"pinfin"))
  {
      stkd.Channel = alloc_and_init_channel();

    set_variable(stkd.Channel->Height,parameters_package,\
                 "microchannel.height",100.0e-6,true);
    if(stkd.Channel->Height > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.height unit (1um = 1e-6)");
    stkd.Channel->Height *= 1e6; // convert to um

    set_variable(stkd.Channel->Pitch,parameters_package,\
                 "microchannel.pin_pitch",100e-6,true);
    if(stkd.Channel->Pitch > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.pin_pitch unit (1um = 1e-6)");
    stkd.Channel->Pitch *= 1e6; // convert to um

    double microchannel_pin_diameter;
    set_variable(microchannel_pin_diameter,parameters_package,\
                 "microchannel.pin_diameter",50e-6,true);
    if(microchannel_pin_diameter > 1.0)
      EI_ERROR("3DICE","incorrect microchannel.pin_diameter unit (1um = 1e-6)");
    microchannel_pin_diameter *= 1e6;

    stkd.Channel->Porosity = POROSITY(microchannel_pin_diameter,stkd.Channel->Pitch);

    string microchannel_pin_distribution;
    set_variable(microchannel_pin_distribution,parameters_package,\
                 "microchannel.pin_distribution","inline");
    if(stricmp(microchannel_pin_distribution,"inline")&&stricmp(microchannel_pin_distribution,"staggered"))
      EI_ERROR("3DICE","invalid pin distribution "+microchannel_pin_distribution+" - inline or staggered");
    stkd.Channel->ChannelModel = !stricmp(microchannel_pin_distribution,"inline")?TDICE_CHANNEL_MODEL_PF_INLINE:TDICE_CHANNEL_MODEL_PF_STAGGERED;

    stkd.Channel->NLayers = NUM_LAYERS_CHANNEL_2RM;
    stkd.Channel->SourceLayerOffset = SOURCE_OFFSET_CHANNEL_2RM;

    set_variable(stkd.Channel->Coolant.DarcyVelocity,parameters_package,\
                 "microchannel.darcy_velocity",1.1066e+06);

    stkd.Channel->Coolant.HTCSide = 0.0;
    stkd.Channel->Coolant.HTCTop = 0.0;
    stkd.Channel->Coolant.HTCBottom = 0.0;

    set_variable(stkd.Channel->Coolant.VHC,parameters_package,\
                 "microchannel.coolant_volumetric_heat_capacity",4.172638e-12);
    set_variable(stkd.Channel->Coolant.TIn,parameters_package,\
                 "microchannel.coolant_incoming_temperature",ambient_temperature);

    string microchannel_pin_material;
    set_variable(microchannel_pin_material,parameters_package,\
                 "microchannel.pin_material","n/a",true);
    stkd.Channel->WallMaterial = find_material_in_list(stkd.MaterialsList,(char*)microchannel_pin_material.c_str());

    if(!stkd.Channel->WallMaterial)
      EI_ERROR("3DICE","unknown microchannel.pin_material "+microchannel_pin_material);
    else
      stkd.Channel->WallMaterial->Used++;
  }
  else
    EI_ERROR("3DICE","unknown microchannel.type "+microchannel_type+" - 4rm, 2rm, or pinfin");

  // stack description - DiesList
  string die_name = parameters_package.get_option("die",true);

  if(!stricmp(die_name,"n/a"))
    EI_ERROR("3DICE","no die description is found");
  else
  {
    double layer_height;
    string layer_material, layer_name;
    bool is_source_layer;

    while(stricmp(die_name,"n/a"))
    {
      Die *die = alloc_and_init_die();

      if(!stkd.DiesList)
        stkd.DiesList = die;
      else
      {
        Die *last_die = stkd.DiesList;
        while(last_die->Next)
          last_die = last_die->Next;
        last_die->Next = die;
      }
      
      die->Id = (char*)die_name.c_str();

      // layer
      layer_name = parameters_package.get_option("die."+die_name+".layer",true);

      if(!stricmp(layer_name,"n/a"))
        EI_ERROR("3DICE","no layer is declared in the die "+die_name);
      while(stricmp(layer_name,"n/a"))
      {
        Layer *layer = alloc_and_init_layer();

        if(!die->TopLayer)
          die->TopLayer = layer;
        else
        {
          Layer *last_layer = die->TopLayer;
          while(last_layer->Prev)
            last_layer = last_layer->Prev;
          JOIN_ELEMENTS(layer,last_layer);
        }
        die->BottomLayer = layer;

        if(die->SourceLayer)
          die->SourceLayerOffset++;

        set_variable(layer_height,parameters_package,"die."+die_name+".layer."+layer_name+".height",0.0,true);
        set_variable(layer_material,parameters_package,"die."+die_name+".layer."+layer_name+".material","n/a",true);
        if(layer_height > 1.0)
          EI_ERROR("3DICE","incorrect "+die_name+".layer."+layer_name+".height unit (1um = 1e-6)");
        layer->Height = layer_height*1e6; // conver to um
        layer->Material = find_material_in_list(stkd.MaterialsList,(char*)layer_material.c_str());
        
        if(!layer->Material)
          EI_ERROR("3DICE","unknown die."+die_name+".layer."+layer_name+".material "+layer_material);
        else
          layer->Material->Used++;
          
        die->NLayers++;

        set_variable(is_source_layer,parameters_package,"die."+die_name+".layer."+layer_name+".is_source_layer",false);
        if(is_source_layer)
        {
          if(die->SourceLayer)
            EI_ERROR("3DICE","there cannot be multiple source layer in the die "+die_name);
          else
            die->SourceLayer = layer;
        }

        layer_name = parameters_package.get_option("die."+die_name+".layer",true);
      }

      if(!die->SourceLayer)
        EI_ERROR("3DICE","no source layer is declared in the die "+die_name);

      die_name = parameters_package.get_option("die",true);
    }
  }

  // stack description - TopStackElement, BottomStackElement
  string stack_name = parameters_package.get_option("stack",true);

  if(!stricmp(stack_name,"n/a"))
    EI_ERROR("3DICE","no stack description is found");
  else
  {
    int num_layers = 0;
    int num_channels = 0;
    volatile int num_dies = 0;
    
    string stack_type; // die, channel, or layer
    string stack_die, stack_material;
    double stack_height;

    while(stricmp(stack_name,"n/a"))
    {
      set_variable(stack_type,parameters_package,"stack."+stack_name+".type","n/a",true);
      
      StackElement *stack_element = alloc_and_init_stack_element();
      stack_element->Id = (char*)stack_name.c_str();
      
      if(!stricmp(stack_type,"layer"))
      {
        num_layers++;
              
        Layer *layer = alloc_and_init_layer();

        set_variable(stack_material,parameters_package,"stack."+stack_name+".material","n/a",true);        
        set_variable(stack_height,parameters_package,"stack."+stack_name+".height",0.0,true);
        if(stack_height > 1.0)
          EI_ERROR("3DICE","incorrect stack."+stack_name+".height unit (1um = 1e-6)");
          
        layer->Height = stack_height*1e6;
        layer->Material = find_material_in_list(stkd.MaterialsList,(char*)stack_material.c_str());
        
        if(!layer->Material)
          EI_ERROR("3DICE","unknown stack."+stack_name+".material "+material_name);
        
        layer->Material->Used++;
        
        stack_element->Type = TDICE_STACK_ELEMENT_LAYER;
        stack_element->Pointer.Layer = layer;
        stack_element->NLayers = 1;
      }
      else if(!stricmp(stack_type,"channel"))
      {
        num_channels++;
        
        if(!stkd.Channel)
          EI_ERROR("3DICE","no channel is declared for stack "+stack_name);
          
        stack_element->Type = TDICE_STACK_ELEMENT_CHANNEL;
        stack_element->Pointer.Channel = stkd.Channel;
        stack_element->NLayers = stkd.Channel->NLayers;
      }
      else if(!stricmp(stack_type,"die"))
      {
        num_dies++;
        
        stack_element->Type = TDICE_STACK_ELEMENT_DIE;
        
        set_variable(stack_die,parameters_package,"stack."+stack_name+".die","n/a",true);
        stack_element->Pointer.Die = find_die_in_list(stkd.DiesList,(char*)stack_die.c_str());
        
        if(!stack_element->Pointer.Die)
          EI_ERROR("3DICE","unknown die "+stack_die);
          
        stack_element->Pointer.Die->Used++;
        stack_element->NLayers = stack_element->Pointer.Die->NLayers;
        
        stack_element->Floorplan = alloc_and_init_floorplan();

        for(map<string,pseudo_partition_t>::iterator partition_it = energy_introspector->partition.begin();\
            partition_it != energy_introspector->partition.end(); partition_it++)
        {
          if(!stricmp(partition_it->second.package,parameters_package.name))
          {
            parameters_component_t &parameters_partition = parameters->get_partition(partition_it->first);

            string partition_die;
            set_variable(partition_die,parameters_partition,"die","n/a",true);

            if(!stricmp(partition_die,stack_name))
            {
              ICElement *icelement;

              dimension_t partition_dimension = partition_it->second.queue.pull<dimension_t>(0.0,"dimension");

              icelement = alloc_and_init_ic_element();
              icelement->SW_X = partition_dimension.x_left*1e6; // convert to um
              icelement->SW_Y = partition_dimension.y_bottom*1e6; // convert to um
              icelement->Length = partition_dimension.length*1e6;
              icelement->Width = partition_dimension.width*1e6;

              align_to_grid(stkd.Dimensions,icelement);

              if(check_location(stkd.Dimensions,icelement))
                EI_ERROR("3DICE","partition "+parameters_partition.name+" is out of chip dimension");

              FloorplanElement *floorplan_element = alloc_and_init_floorplan_element();
  
              floorplan_element->Id = (char*)parameters_partition.name.c_str();
              floorplan_element->NICElements = 1;
              floorplan_element->ICElementsList = icelement;
              floorplan_element->EffectiveSurface = icelement->EffectiveLength*icelement->EffectiveWidth;//partition_dimension.get_area()*1e12;
              floorplan_element->PowerValues = alloc_and_init_powers_queue();

              for(FloorplanElement *flp_element = stack_element->Floorplan->ElementsList;\
                  flp_element&&(flp_element != floorplan_element); flp_element = flp_element->Next)
              {
                for(ICElement *ic_element = flp_element->ICElementsList;\
                    ic_element&&(ic_element != icelement); ic_element = ic_element->Next)
                {
                  if(check_intersection(ic_element,icelement))
                  {
                    char message[256];
                    sprintf(message,"intersection (overlapping) between %s (%3.3lf, %3.3lf, %3.3lf, %3.3lf) and %s (%3.3lf, %3.3lf, %3.3lf, %3.3lf)",flp_element->Id,ic_element->SW_X,ic_element->SW_Y,ic_element->Length,ic_element->Width,floorplan_element->Id,icelement->SW_X,icelement->SW_Y,icelement->Length,icelement->Width);
                    EI_ERROR("3DICE",(string)message);
                  }
                }
              }

              if(!stack_element->Floorplan->ElementsList)
              {
                stack_element->Floorplan->ElementsList = floorplan_element;
                stack_element->Floorplan->NElements = 1;
              }
              else
              {
                FloorplanElement *last_floorplan_element = stack_element->Floorplan->ElementsList;
                while(last_floorplan_element->Next)
                  last_floorplan_element = last_floorplan_element->Next;
                last_floorplan_element->Next = floorplan_element;
                stack_element->Floorplan->NElements++;
              }
            }
          }
        }

        if(!stack_element->Floorplan->ElementsList)
          EI_ERROR("3DICE","no partition is found for die "+stack_name);
      }
      else
        EI_ERROR("3DICE","unknown stack.type "+stack_type+" - layer, channel, or die");

      if(!stkd.TopStackElement)
      {
        if(stack_element->Type == TDICE_STACK_ELEMENT_CHANNEL)
          EI_ERROR("3DICE","cannot declare a channel as the top-most stack element");
        
        stkd.TopStackElement = stack_element;
        stkd.BottomStackElement = stack_element;
      }
      else
      {
        if(find_stack_element_in_list(stkd.BottomStackElement,stack_element->Id))
          EI_ERROR("3DICE","stack "+stack_name+" is already declared");
          
        if((stkd.BottomStackElement->Type == TDICE_STACK_ELEMENT_CHANNEL)\
           &&(stack_element->Type == TDICE_STACK_ELEMENT_CHANNEL))
          EI_ERROR("3DICE","cannot declare two consecutive channels, "+(string)(stkd.BottomStackElement->Id)+" and "+(string)(stack_element->Id)+", in the stack "+stack_name);
        
        JOIN_ELEMENTS(stack_element,stkd.BottomStackElement);
        stkd.BottomStackElement = stack_element;
      }
  
      stack_name = parameters_package.get_option("stack",true);
    }
#if 1    
    if(num_dies == 0)
      EI_ERROR("3DICE","stack "+stack_name+" must contain at least one die");
#endif      
    if(stkd.BottomStackElement->Type == TDICE_STACK_ELEMENT_CHANNEL)
      EI_ERROR("3DICE","stack "+stack_name+" cannot declare channel "+(string)(stkd.BottomStackElement->Id)+" as the bottom-most stack element");
      
    if(!stkd.ConventionalHeatSink&&!stkd.Channel)
      EI_ERROR("3DICE","neither conventional_heatsink or microchannel was declared");
      
    for(Material *material = stkd.MaterialsList; material; material = material->Next)
      if(material->Used == 0)
        EI_ERROR("3DICE","Material "+(string)(material->Id)+" was declared but never used");
    for(Die *die = stkd.DiesList; die; die = die->Next)
      if(die->Used == 0)
        EI_ERROR("3DICE","Die "+(string)(die->Id)+" was declared but never used");
        
    if(stkd.ConventionalHeatSink)
    {
      if(stkd.TopStackElement->Type == TDICE_STACK_ELEMENT_LAYER)
        stkd.ConventionalHeatSink->TopLayer = stkd.TopStackElement->Pointer.Layer;
      else
        stkd.ConventionalHeatSink->TopLayer = stkd.TopStackElement->Pointer.Die->TopLayer;
    }
    
    CellIndex_t layer_index = 0;
    for(StackElement *stack_element = stkd.BottomStackElement; stack_element; stack_element = stack_element->Next)
    {
      stack_element->Offset = layer_index;
      layer_index += stack_element->NLayers;
    }
    
    stkd.Dimensions->Grid.NLayers = layer_index;
    stkd.Dimensions->Grid.NCells = get_number_of_layers(stkd.Dimensions)*get_number_of_rows(stkd.Dimensions)*get_number_of_columns(stkd.Dimensions);
    
    if(stkd.Dimensions->Grid.NCells > INT32_MAX)
      EI_ERROR("3DICE","too many grid cells");
      
    if(!stkd.Channel)
      compute_number_of_connections(stkd.Dimensions,num_channels,TDICE_CHANNEL_MODEL_NONE);
    else
      compute_number_of_connections(stkd.Dimensions,num_channels,stkd.Channel->ChannelModel);
  }
  
  // analysis description
  set_variable(analysis.InitialTemperature,parameters_package,"temperature",ambient_temperature);

  string thermal_analysis_type;
  set_variable(thermal_analysis_type,parameters_package,"thermal_analysis_type","transient");

  analysis.StepTime = 1.0;
  analysis.SlotTime = 1.0;
  analysis.SlotLength = 1;

  if(!stricmp(thermal_analysis_type,"transient"))
  {
    analysis.AnalysisType = TDICE_ANALYSIS_TYPE_TRANSIENT;
    emulate = &emulate_step;
  }
  else if(!stricmp(thermal_analysis_type,"steady"))
  {
    analysis.AnalysisType = TDICE_ANALYSIS_TYPE_STEADY;
    emulate = &emulate_steady;
  }
  else
    EI_ERROR("3DICE","unknown thermal_analysis_type "+thermal_analysis_type+" - transient or steady");

  // monitor die stack thermal map  
  StackElement *stack_element = stkd.BottomStackElement;
  while(stack_element)
  {
    if(stack_element->Type == TDICE_STACK_ELEMENT_DIE)
    {
      InspectionPoint *stack_inspection_point = alloc_and_init_inspection_point();
      stack_inspection_point->Type = TDICE_OUTPUT_TYPE_TMAP;
      stack_inspection_point->Instant = TDICE_OUTPUT_INSTANT_FINAL;
      stack_inspection_point->StackElement = stack_element;
      //inspection_point->FilName = "none";
      add_inspection_point_to_analysis(&analysis,stack_inspection_point);
      /*
      FloorplanElement *floorplan_element = stack_element->Floorplan->ElementsList;
      while(floorplan_element)
      {
        Tflpel *tflpel = alloc_and_init_tflpel();
        tflpel->FloorplanElement = floorplan_element;
        tflpel->Quantity = TDICE_OUTPUT_QUANTITY_AVERAGE;

        InspectionPoint *floorplan_inspection_point = alloc_and_init_inspection_point();
        floorplan_inspection_point->Type = TDICE_OUTPUT_TYPE_TFLPEL;
        floorplan_inspection_point->Instant = TDICE_OUTPUT_INSTANT_FINAL;
        floorplan_inspection_point->Pointer.Tflpel = tflpel;
        floorplan_inspection_point->StackElement = stack_element;
        //floorplan_inspection_point->FileName = "none";
        add_inspection_point_to_analysis(&analysis,floorplan_inspection_point);

        floorplan_element = floorplan_element->Next;
      }
      */
    }
    stack_element = stack_element->Next;
  }
  /*
  // Debug
  Material *m = stkd.MaterialsList;
  while(m)
  {
    printf("\n");
    printf("material.Id = %s\n",m->Id);
    printf("material.Used = %d\n",m->Used);
    printf("material.VolumetricHeatCapacity = %e\n",m->VolumetricHeatCapacity);
    printf("material.ThermalConductivity = %e\n",m->ThermalConductivity);
    m = m->Next;
  }

  ConventionalHeatSink *h = stkd.ConventionalHeatSink;
  if(h)
  {
    printf("\n");
    printf("conventional_heatsink.AmbientHTC = %e\n",h->AmbientHTC);
    printf("conventional_heatsink.AmbientTemperature = %lf\n",h->AmbientTemperature);
    
    Layer *l = h->TopLayer;
    while(l)
    {
      printf("\n");
      printf("layer.Height = %lf\n",l->Height);
      printf("layer.Material = %s\n",l->Material->Id);
      l = l->Prev;
    }
  }

  Channel *c = stkd.Channel;
  if(c)
  {
    printf("\n");
    printf("channel.ChannelModel  = %d\n",c->ChannelModel);
    printf("channel.Height = %lf\n",c->Height);
    printf("channel.Length = %lf\n",c->Length);
    printf("channel.Pitch = %lf\n",c->Pitch);
    printf("channel.Porosity = %lf\n",c->Porosity);
    printf("channel.NChannels = %d\n",c->NChannels);
    printf("channel.NLayers = %d\n",c->NLayers);
    printf("channel.SourceLayerOffset = %d\n",c->SourceLayerOffset);
    printf("channel.Coolant.HTCSide = %e\n",c->Coolant.HTCSide);
    printf("channel.Coolant.HTCTop = %e\n",c->Coolant.HTCTop);
    printf("channel.Coolant.HTCBottom = %e\n",c->Coolant.HTCBottom);
    printf("channel.Coolant.VHC = %e\n",c->Coolant.VHC);
    printf("channel.Coolant.FlowRate = %e\n",c->Coolant.FlowRate);
    printf("channel.Coolant.DarcyVelocity = %e\n",c->Coolant.DarcyVelocity);
    printf("channel.Coolant.TIn = %lf\n",c->Coolant.TIn);
    printf("channel.WallMaterial = %s\n",c->WallMaterial->Id);
  }

  Die *d = stkd.DiesList;
  while(d)
  {
    printf("\n");
    printf("die.Id = %s\n",d->Id);
    printf("die.Used = %d\n",d->Used);
    printf("die.NLayers = %d\n",d->NLayers);
    printf("die.SourceLayerOffset = %d\n",d->SourceLayerOffset);
    
    Layer *l = d->TopLayer;
    while(l)
    {
      printf("\n");
      printf("layer.Height = %lf\n",l->Height);
      printf("layer.Material = %s\n",l->Material->Id);
      l = l->Prev;
    }

    d = d->Next;
  }

  if(stkd.Dimensions)
  {
    printf("\n");
    printf("Dimensions.Cell.FirstWallLength = %lf\n",stkd.Dimensions->Cell.FirstWallLength);
    printf("Dimensions.Cell.WallLength = %lf\n",stkd.Dimensions->Cell.WallLength);
    printf("Dimensions.Cell.ChannelLength = %lf\n",stkd.Dimensions->Cell.ChannelLength);
    printf("Dimensions.Cell.LastWallLength = %lf\n",stkd.Dimensions->Cell.LastWallLength);
    printf("Dimensions.Cell.Width = %lf\n",stkd.Dimensions->Cell.Width);
    printf("\n");
    printf("Dimensions.Grid.NLayers = %d\n",stkd.Dimensions->Grid.NLayers);
    printf("Dimensions.Grid.NRows = %d\n",stkd.Dimensions->Grid.NRows);
    printf("Dimensions.Grid.NColumns = %d\n",stkd.Dimensions->Grid.NColumns);
    printf("Dimensions.Grid.NCells = %d\n",stkd.Dimensions->Grid.NCells);
    printf("Dimensions.Grid.NConnections = %d\n",stkd.Dimensions->Grid.NConnections);
    printf("\n");
    printf("Dimensions.Chip.Length = %lf\n",stkd.Dimensions->Chip.Length);
    printf("Dimensions.Chip.Width = %lf\n",stkd.Dimensions->Chip.Width);
  }

  StackElement *s = stkd.TopStackElement;
  while(s)
  {
    printf("\n");
    printf("stack.Id = %s\n",s->Id);
    printf("stack.Type = %d\n",s->Type);
    printf("stack.NLayers = %d\n",s->NLayers);
    printf("stack.Offset = %d\n",s->Offset);

    if(s->Type == TDICE_STACK_ELEMENT_DIE)
    {
      printf("stack.Floorplan.NElements = %d\n",s->Floorplan->NElements);

      FloorplanElement *f = s->Floorplan->ElementsList;
      while(f)
      {
        printf("\n");
        printf("floorplan.Id = %s\n",f->Id);
        printf("floorplan.NICElements = %d\n",f->NICElements);
        printf("floorplan.EffectiveSurface = %e\n",f->EffectiveSurface);
        printf("floorplan.PowerValues.Length = %d\n",f->PowerValues->Length);

        ICElement *i = f->ICElementsList;
        while(i)
        {
          printf("\n");
          printf("icelement.SW_X = %lf\n",i->SW_X);
          printf("icelement.SW_Y = %lf\n",i->SW_Y);
          printf("icelement.Length = %lf\n",i->Length);
          printf("icelement.Width = %lf\n",i->Width);
          printf("icelement.EffectiveLength = %lf\n",i->EffectiveLength);
          printf("icelement.EffectiveWidth = %lf\n",i->EffectiveWidth);
          printf("icelement.SW_Row = %d\n",i->SW_Row);
          printf("icelement.SW_Column = %d\n",i->SW_Column);
          printf("icelement.NE_Row = %d\n",i->NE_Row);
          printf("icelement.NE_Column = %d\n",i->NE_Column);
 
          i = i->Next;
        }

        PowerNode *p = f->PowerValues->Head;
        while(p)
        {
          printf("\n");
          printf("powervalue.Value = %lf\n",p->Value);
          p = p->Next;
        }

        f = f->Next;
      }
    }
    s = s->Prev;
  }

  printf("\n");
  */
  /*
  char prefix[3] = "% ";
  error = generate_analysis_headers(&analysis,stkd.Dimensions,prefix);
  
  if(error != TDICE_SUCCESS)
    EI_ERROR("3DICE","initializing analysis points failed");
  */

  init_thermal_data(&tdata);
/*
  error = fill_thermal_data(&tdata,&stkd,&analysis);

  if(error != TDICE_SUCCESS)
    EI_ERROR("3DICE","initializing thermal data failed");
*/
  error = fill_thermal_data(&tdata,&stkd,&analysis);

  if(error != TDICE_SUCCESS)
  {
    EI_ERROR("3DICE","initializing thermal data failed");
    return;
  }
/*
  tdata.Size = get_number_of_cells(stkd.Dimensions);

  tdata.Temperatures = (Temperature_t*)malloc(sizeof(*tdata.Temperatures)*tdata.Size);
  tdata.ThermalCells = (ThermalCell*)malloc(sizeof(ThermalCell)*get_number_of_layers(stkd.Dimensions)*get_number_of_columns(stkd.Dimensions));
  tdata.Sources = (Source_t*)malloc(sizeof(*tdata.Sources)*tdata.Size);
  tdata.SLU_PermutationMatrixR = (int*)malloc(sizeof(int)*tdata.Size);
  tdata.SLU_PermutationMatrixC = (int*)malloc(sizeof(int)*tdata.Size);
  tdata.SLU_Etree = (int*)malloc(sizeof(int)*tdata.Size);

  dCreate_Dense_Matrix(&tdata.SLUMatrix_B,tdata.Size,1,tdata.Temperatures,tdata.Size,SLU_DN,SLU_D,SLU_GE);

  fill_thermal_cell_stack_description(tdata.ThermalCells,&analysis,&stkd);

  if(alloc_system_matrix(&tdata.SM_A,tdata.Size,get_number_of_connections(stkd.Dimensions))\
     == TDICE_FAILURE)
    EI_ERROR("3DICE","system matrix allocation failed");

  fill_system_matrix_stack_description(tdata.SM_A,tdata.ThermalCells,&stkd);

  dCreate_CompCol_Matrix(&tdata.SLUMatrix_A,tdata.Size,tdata.Size,tdata.SM_A.NNz,tdata.SM_A.Values,\
  (int*)tdata.SM_A.RowIndices,(int*)tdata.SM_A.ColumnPointers,SLU_NC,SLU_D,SLU_GE);
  get_perm_c(tdata.SLU_Options.ColPerm,&tdata.SLUMatrix_A,tdata.SLU_PermutationMatrixC);
  sp_preorder(&tdata.SLU_Options,&tdata.SLUMatrix_A,tdata.SLU_PermutationMatrixC,tdata.SLU_Etree,&tdata.SLUMatrix_A_Permuted);
  dgstrf(&tdata.SLU_Options,&tdata.SLUMatrix_A_Permuted,sp_ienv(2),sp_ienv(1),tdata.SLU_Etree,NULL,0,\
  tdata.SLU_PermutationMatrixC,tdata.SLU_PermutationMatrixR,&tdata.SLUMatrix_L,&tdata.SLUMatrix_U,\
  &tdata.SLU_Stat,&tdata.SLU_Info);

  if(tdata.SLU_Info)
    EI_ERROR("3DICE","Matrix LU factorization failed");
*/
}


void THERMALLIB_3DICE::deliver_power(string partition_name, power_t partition_power)
{
  bool power_delivered = false;
  StackElement *stack_element = stkd.BottomStackElement;

  while(stack_element)
  {
    if(stack_element->Floorplan) // die stack
    {
      FloorplanElement *floorplan_element =\
      find_floorplan_element_in_list(stack_element->Floorplan->ElementsList,(char*)partition_name.c_str());

      if(floorplan_element)
      {
        // keep single-entry power queue
        if(floorplan_element->PowerValues->Head)
        {
          EI_WARNING("3DICE","power value already exists for pseudo partition "+partition_name+" in deliver_power()");
          return;
        }
        put_into_powers_queue(floorplan_element->PowerValues,partition_power.total);
        power_delivered = true;
        break;
      }
    }
    stack_element = stack_element->Next;
  }

  if(!power_delivered)
    EI_WARNING("3DICE","no matching floorplan is found for pseudo partition "+partition_name+" in deliver_power()");
}


grid_t<double> THERMALLIB_3DICE::get_thermal_grid(void)
{
  grid_t<double> thermal_grid;
  int layer_index = 0;
  InspectionPoint *inspection_point = analysis.InspectionPointListFinal;

  while(inspection_point)
  {
    if(inspection_point->Type == TDICE_OUTPUT_TYPE_TMAP) // thermal maps from bottom to top stacks
    {
      StackElement *stack_element = inspection_point->StackElement;
      double *temperatures = tdata.Temperatures;
      temperatures += get_cell_offset_in_stack(stkd.Dimensions,get_source_layer_offset(stack_element),0,0);

      thermal_grid.cell_length = stkd.Dimensions->Chip.Length/stkd.Dimensions->Grid.NColumns*1e-6;
      thermal_grid.cell_width = stkd.Dimensions->Chip.Width/stkd.Dimensions->Grid.NRows*1e-6;

      for(int row_index = FIRST_ROW_INDEX; row_index <= LAST_ROW_INDEX(stkd.Dimensions); row_index++)
        for(int column_index = FIRST_COLUMN_INDEX; column_index <= LAST_COLUMN_INDEX(stkd.Dimensions); column_index++)
          thermal_grid.push(column_index,row_index,layer_index,*temperatures++);

      layer_index++;
    }

    inspection_point = inspection_point->Next;
  }

  return thermal_grid;
}


double THERMALLIB_3DICE::get_partition_temperature(string partition_name, int temp_type)
{
  double temperature = 0.0;

  StackElement *stack_element = stkd.BottomStackElement;

  while(stack_element)
  {
    if(stack_element->Floorplan)
    {
      FloorplanElement *floorplan_element =\
      find_floorplan_element_in_list(stack_element->Floorplan->ElementsList,(char*)partition_name.c_str());
      if(floorplan_element)
      {
        Temperature_t *temperatures = tdata.Temperatures;
        temperatures += get_cell_offset_in_stack(stkd.Dimensions,get_source_layer_offset(stack_element),0,0);
        //switch(thermal_grid_mapping)
        switch(temp_type)
        {
          case MAX_TEMP:
          {
            temperature = get_max_temperature_floorplan_element(floorplan_element,stkd.Dimensions,temperatures);
            break;
          }
          case MIN_TEMP:
          {
            temperature = get_min_temperature_floorplan_element(floorplan_element,stkd.Dimensions,temperatures);
            break;
          }
          case CENTER_TEMP:
          {
            dimension_t dimension = energy_introspector->partition.find(partition_name)->second.queue.pull<dimension_t>(0.0,"dimension");
            Tcell *tcell = alloc_and_init_tcell();
            align_tcell(tcell,dimension.get_x_center()*1e6,dimension.get_y_center()*1e6,stkd.Dimensions);
            temperature = *(temperatures + get_cell_offset_in_stack(stkd.Dimensions,get_source_layer_offset(stack_element),tcell->RowIndex,tcell->ColumnIndex));
            break;
          }
          default: // AVG_TEMP
          {
            temperature = get_avg_temperature_floorplan_element(floorplan_element,stkd.Dimensions,temperatures);
            break;
          }
        }
        break;
      }
    }
    stack_element = stack_element->Next;
  }

  return temperature;
}


double THERMALLIB_3DICE::get_point_temperature(double x, double y, int layer)
{
  double temperature = 0;
  int layer_index = 0;

  StackElement *stack_element = stkd.BottomStackElement;
  while(stack_element)
  {
    if(stack_element->Floorplan)
    {
      if(layer == layer_index)
      {
        Temperature_t *temperatures = tdata.Temperatures;

        Tcell *tcell = alloc_and_init_tcell();
        align_tcell(tcell,x*1e6,y*1e6,stkd.Dimensions);
        temperature = *(temperatures + get_cell_offset_in_stack(stkd.Dimensions,get_source_layer_offset(stack_element),tcell->RowIndex,tcell->ColumnIndex));
        break;
      }
      else
        layer_index++;
    }
    stack_element = stack_element->Next;
  }
  return temperature;
}


int THERMALLIB_3DICE::get_partition_layer_index(string partition_name)
{
  int layer_index = 0;
  FloorplanElement *floorplan_element = NULL;
  StackElement *stack_element = stkd.BottomStackElement;

  while(stack_element)
  {
    if(stack_element->Floorplan)
    {
      floorplan_element = find_floorplan_element_in_list(stack_element->Floorplan->ElementsList,(char*)partition_name.c_str());
      if(floorplan_element)
        break;
      else
        layer_index++;
    }
    stack_element = stack_element->Next;
  }

  if(!floorplan_element)
    EI_WARNING("3DICE","get_partition_layer_index() failed - cannot find matching floorplan for pseudo partition "+partition_name);

  return layer_index;
}


void THERMALLIB_3DICE::compute_temperature(double time_tick, double period)
{
  if(!tdata.Temperatures)
  {
    EI_WARNING("3DICE","cannot compute temperature - thermal simulation terminated");
    return;
  }

  if(analysis.StepTime != period) // Matrices must be reconstructed
  {
    char ModuleID[128];
    //sprintf(ModuleID,"*****************StepTime:%f, period:%f****************************", analysis.StepTime, period);   

    //EI_WARNING("3DICE", "StepTime does not match" + string(ModuleID));

#if 1
    analysis.SlotTime = time_tick;
    analysis.SlotLength = period;
    analysis.StepTime = period;

    Destroy_SuperMatrix_Store(&tdata.SLUMatrix_A);
    Destroy_SuperMatrix_Store(&tdata.SLUMatrix_B);
    Destroy_CompCol_Permuted(&tdata.SLUMatrix_A_Permuted);
    Destroy_SuperNode_Matrix(&tdata.SLUMatrix_L);
    Destroy_CompCol_Matrix(&tdata.SLUMatrix_U);
    free_system_matrix(&tdata.SM_A);
    init_system_matrix(&tdata.SM_A);

    dCreate_Dense_Matrix(&tdata.SLUMatrix_B,tdata.Size,1,tdata.Temperatures,tdata.Size,SLU_DN,SLU_D,SLU_GE);
    fill_thermal_cell_stack_description(tdata.ThermalCells,&analysis,&stkd);
    if(alloc_system_matrix(&tdata.SM_A,tdata.Size,get_number_of_connections(stkd.Dimensions)) == TDICE_FAILURE)
    {
      EI_WARNING("3DICE","allocating system matrix failed - terminating thermal simulation");
      return;
    }
    else
      fill_system_matrix_stack_description(tdata.SM_A,tdata.ThermalCells,&stkd);

    dCreate_CompCol_Matrix(&tdata.SLUMatrix_A,tdata.Size,tdata.Size,tdata.SM_A.NNz,tdata.SM_A.Values,\
        (int*)tdata.SM_A.RowIndices,(int*)tdata.SM_A.ColumnPointers,SLU_NC,SLU_D,SLU_GE);

    get_perm_c(tdata.SLU_Options.ColPerm,&tdata.SLUMatrix_A,tdata.SLU_PermutationMatrixC);
    sp_preorder(&tdata.SLU_Options,&tdata.SLUMatrix_A,tdata.SLU_PermutationMatrixC,tdata.SLU_Etree,\
        &tdata.SLUMatrix_A_Permuted);
    dgstrf(&tdata.SLU_Options,&tdata.SLUMatrix_A_Permuted,sp_ienv(2),sp_ienv(1),\
        tdata.SLU_Etree,NULL,0,tdata.SLU_PermutationMatrixC,tdata.SLU_PermutationMatrixR,\
        &tdata.SLUMatrix_L,&tdata.SLUMatrix_U,&tdata.SLU_Stat,&tdata.SLU_Info);
#endif
  }
  else
  {
    analysis.SlotTime = time_tick;
    analysis.SlotLength = period;
    analysis.StepTime = period;
  }

  error = fill_sources_stack_description(tdata.Sources,tdata.ThermalCells,&stkd);
  
  if(error == TDICE_FAILURE)
  {
    EI_WARNING("3DICE","fill_source_stack_description() failed - terminating thermal simulation");
    free_thermal_data(&tdata);
    return;
  }

  if(analysis.AnalysisType == TDICE_ANALYSIS_TYPE_TRANSIENT)
    fill_system_vector(stkd.Dimensions,tdata.Temperatures,tdata.Sources,tdata.ThermalCells,tdata.Temperatures);
  else if(analysis.AnalysisType == TDICE_ANALYSIS_TYPE_STEADY)
    fill_system_vector_steady(stkd.Dimensions,tdata.Temperatures,tdata.Sources);
  else
  {
    EI_WARNING("3DICE","unknown thermal_analysis_type");
    return;
  }

  dgstrs(NOTRANS,&tdata.SLUMatrix_L,&tdata.SLUMatrix_U,tdata.SLU_PermutationMatrixC,\
         tdata.SLU_PermutationMatrixR,&tdata.SLUMatrix_B,&tdata.SLU_Stat,&tdata.SLU_Info);

  if(tdata.SLU_Info < 0)
  {
    EI_WARNING("3DICE","solving linear system failed - terminating thermal simulation");
    free_thermal_data(&tdata);
    return;
  }
}

