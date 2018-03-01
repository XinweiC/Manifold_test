#include <iostream>
#include <assert.h>

#include "../energy_introspector.h"

using namespace EI;

int main(void)
{
  energy_introspector_t *energy_introspector = new energy_introspector_t((char*)"CONFIG/HOTSPOT/hotspot.config");
  
  power_t L2_left, L2, L2_right, Icache, Dcache, Bpred_0, Bpred_1, Bpred_2, DTB_0, DTB_1, DTB_2, FPAdd_0, FPAdd_1, FPReg_0, FPReg_1, FPReg_2, FPReg_3, FPMul_0, FPMul_1, FPMap_0, FPMap_1, IntMap, IntQ, IntReg_0, IntReg_1, IntExec, FPQ, LdStQ, ITB_0, ITB_1;
  
  L2_left.total = 1.44;
  L2.total = 7.37;
  L2_right.total = 1.44;
  Icache.total = 8.27;
  Dcache.total = 14.3;
  Bpred_0.total = 1.51666666666667;
  Bpred_1.total = 1.51666666666667;
  Bpred_2.total = 1.51666666666667;
  DTB_0.total = 0.0596666666666667;
  DTB_1.total = 0.0596666666666667;
  DTB_2.total = 0.0596666666666667;
  FPAdd_0.total = 0.62;
  FPAdd_1.total = 0.62;
  FPReg_0.total = 0.19375;
  FPReg_1.total = 0.19375;
  FPReg_2.total = 0.19375;
  FPReg_3.total = 0.19375;
  FPMul_0.total = 0.665;
  FPMul_1.total = 0.665;
  FPMap_0.total = 0.02355;
  FPMap_1.total = 0.02355;
  IntMap.total = 1.07;
  IntQ.total = 0.365;
  IntReg_0.total = 2.585;
  IntReg_1.total = 2.585;
  IntExec.total = 7.7;
  FPQ.total = 0.0354;
  LdStQ.total = 3.46;
  ITB_0.total = 0.2;
  ITB_1.total = 0.2;
  
  double sample_period = 3.333e-6;

  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","L2_left","power",L2_left);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","L2","power",L2);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","L2_right","power",L2_right);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","Icache","power",Icache);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","Dcache","power",Dcache);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","Bpred_0","power",Bpred_0);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","Bpred_1","power",Bpred_1);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","Bpred_2","power",Bpred_2);              
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","DTB_0","power",DTB_0);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","DTB_1","power",DTB_1);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","DTB_2","power",DTB_2);      
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPAdd_0","power",FPAdd_0);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPAdd_1","power",FPAdd_1);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPReg_0","power",FPReg_0);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPReg_1","power",FPReg_1);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPReg_2","power",FPReg_2);      
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPReg_3","power",FPReg_3);      
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPMul_0","power",FPMul_0);      
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPMul_1","power",FPMul_1);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPMap_0","power",FPMap_0);      
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPMap_1","power",FPMap_1);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","IntMap","power",IntMap);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","IntQ","power",IntQ);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","IntReg_0","power",IntReg_0);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","IntReg_1","power",IntReg_1);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","IntExec","power",IntExec);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","FPQ","power",FPQ);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","LdStQ","power",LdStQ);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","ITB_0","power",ITB_0);
  energy_introspector->push_data<power_t>(sample_period,sample_period,"partition","ITB_1","power",ITB_1);

  energy_introspector->compute_temperature(sample_period,sample_period,"HotSpot");
  return 0;
}
