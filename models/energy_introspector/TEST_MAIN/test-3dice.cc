#include <iostream>
#include <assert.h>

#include "../energy_introspector.h"

using namespace EI;

int main(void)
{
  energy_introspector_t *energy_introspector = new energy_introspector_t((char*)"CONFIG/3DICE/3d-ice.config");

  power_t mem_background[5], mem_hotspot[5];
  power_t core[5], cache[5], CLK[5], FPU[5], crossbar[5];

  mem_background[0].total = 12.5;
  mem_background[1].total = 14.0;
  mem_background[2].total = 10.5;
  mem_background[3].total = 13.5;
  mem_background[4].total = 12.5;

  mem_hotspot[0].total = 25.0;
  mem_hotspot[1].total = 22.0;
  mem_hotspot[2].total = 24.0;
  mem_hotspot[3].total = 28.5;
  mem_hotspot[4].total = 25.0;

  core[0].total = 12.5;
  core[1].total = 14.0;
  core[2].total = 10.5;
  core[3].total = 13.5;
  core[4].total = 12.5;

  cache[0].total = 12.5;
  cache[1].total = 14.0;
  cache[2].total = 10.5;
  cache[3].total = 13.5;
  cache[4].total = 12.5;

  CLK[0].total = 12.5;
  CLK[1].total = 14.0;
  CLK[2].total = 10.5;
  CLK[3].total = 13.5;
  CLK[4].total = 12.5;

  FPU[0].total = 12.5;
  FPU[1].total = 14.0;
  FPU[2].total = 10.5;
  FPU[3].total = 13.5;
  FPU[4].total = 12.5;

  crossbar[0].total = 12.5;
  crossbar[1].total = 14.0;
  crossbar[2].total = 10.5;
  crossbar[3].total = 13.5;
  crossbar[4].total = 12.5;

  for(int i = 0; i < 5; i++)
  {
    // MEMORY_DIE
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","MEMORY_DIE:Background1","power",mem_background[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","MEMORY_DIE:Background2","power",mem_background[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","MEMORY_DIE:HotSpot1","power",mem_hotspot[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","MEMORY_DIE:HotSpot2","power",mem_hotspot[i]);

    // CORE_DIE
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Core0","power",core[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Core1","power",core[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Core2","power",core[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Core3","power",core[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Core4","power",core[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Core5","power",core[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Core6","power",core[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Core7","power",core[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Cache0","power",cache[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:Cache1","power",cache[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:CLK","power",CLK[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:FPU","power",FPU[i]);
    energy_introspector->push_data<power_t>((double)i*0.02+0.02,0.02,"partition","CORE_DIE:CrossBar","power",crossbar[i]);

    energy_introspector->compute_temperature((double)i*0.02+0.02,0.02,"3DICE");

    grid_t<double> thermal_grid = energy_introspector->pull_data<grid_t<double> >((double)i*0.02+0.02,"package","3DICE","thermal_grid");

    /*for(int row_index = 0; row_index < 100; row_index++)
    {
      for(int col_index = 0; col_index < 100; col_index++)
        cout << thermal_grid.pull(col_index,row_index,0) << "  ";

      cout << endl;
    }*/

    cout << energy_introspector->pull_data<double>((double)i*0.02+0.02,"partition","MEMORY_DIE:Background1","temperature") << "\t";
    cout << energy_introspector->pull_data<double>((double)i*0.02+0.02,"partition","MEMORY_DIE:HotSpot1","temperature") << "\t";
    cout << energy_introspector->pull_data<double>((double)i*0.02+0.02,"partition","MEMORY_DIE:HotSpot2","temperature") << "\t";
    cout << energy_introspector->pull_data<double>((double)i*0.02+0.02,"partition","MEMORY_DIE:Background2","temperature") << "\n";    
  }
  return 0;
}
