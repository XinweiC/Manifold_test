#include <string.h>
#include <sys/time.h>

#include "ei_wrapper.h"
#include "sram_perf.h"
#include "sram_volt.h"

//ei_wrapper.cc works only for sequential impelmentation right now, also assume every core starts at the same clock frequency initially

using namespace manifold::ei_wrapper;

#define EI_COMPUTE
#define IVR_SLOPE 5e-6 
vector<double> ad; 
vector<double> bd; 
vector<double> cd; 
vector<double> dd; 
vector<double> ed; 
vector<double> fd;

int synced = 0;
int NUM_CORES;
tick_t SAMPLING_CYCLE;

ei_wrapper_t::ei_wrapper_t(manifold::kernel::Clock* clk, EI::energy_introspector_t *energy_introspector, manifold::spx::pipeline_counter_t* proc_cnt, manifold::spx::ipa_t* proc_ipa, manifold::mcp_cache_namespace::L1_counter_t* c1_cnt, manifold::mcp_cache_namespace::L2_counter_t* c2_cnt, double sampling_period, int num_nodes, int uid)
{
  clock = clk;
  id = uid; 
  char ModuleID[64];
  manifold::kernel::Clock::Register<ei_wrapper_t>(manifold::kernel::Clock::Master(),this,&ei_wrapper_t::tick, (void(ei_wrapper_t::*)(void))0);
  ei = energy_introspector;

  sprintf(ModuleID,"CORE_DIE:FRT%d",id);
  ei->update_variable_partition(string(ModuleID),string("vdd"),0.8);
  ei->update_variable_partition(string(ModuleID),string("temperature"),300.0);

  sprintf(ModuleID,"CORE_DIE:SCH%d",id);
  ei->update_variable_partition(string(ModuleID),string("vdd"),0.8);
  ei->update_variable_partition(string(ModuleID),string("temperature"),300.0);
 
  sprintf(ModuleID,"CORE_DIE:INT%d",id);
  ei->update_variable_partition(string(ModuleID),string("vdd"),0.8);
  ei->update_variable_partition(string(ModuleID),string("temperature"),300.0);
  
  sprintf(ModuleID,"CORE_DIE:FPU%d",id);
  ei->update_variable_partition(string(ModuleID),string("vdd"),0.8);
  ei->update_variable_partition(string(ModuleID),string("temperature"),300.0);
  
  sprintf(ModuleID,"CORE_DIE:MEM%d",id);
  ei->update_variable_partition(string(ModuleID),string("vdd"),0.8);
  ei->update_variable_partition(string(ModuleID),string("temperature"),300.0);

  sprintf(ModuleID,"DL2_DIE:SPOT%d",id);
  ei->update_variable_partition(string(ModuleID),string("vdd"),0.8);
  ei->update_variable_partition(string(ModuleID),string("temperature"),300.0);

  SAMPLING_CYCLE = (tick_t)(sampling_period * manifold::kernel::Clock::Master().freq); 

  p_cnt = proc_cnt;
  p_ipa = proc_ipa;
  l1_cnt = c1_cnt;
  l2_cnt = c2_cnt;
  p_cnt->period = sampling_period;
  p_cnt->time_tick = 0;

  last_vdd = 0.8;
  slack_cycle = 0;

  sam_cycle = 0;

  if (id == num_nodes - 1) {
    NUM_CORES = num_nodes;
    synced = 0;

    ad.reserve(num_nodes);
    bd.reserve(num_nodes);
    cd.reserve(num_nodes);
    dd.reserve(num_nodes);
    ed.reserve(num_nodes);
    fd.reserve(num_nodes);
 }
}

ei_wrapper_t::~ei_wrapper_t()
{

}

double core_leakage = 0;
double sram_leakage = 0;
double total_leakage = 0;
double core_energy = 0;
double sram_energy = 0;
double total_energy = 0;
double factor = 5;
double factor2 = 0.64;
double factor3 = 2.65;
double total_mips = 0;
void ei_wrapper_t::tick()
{
  sam_cycle += 1;

  if(sam_cycle == (SAMPLING_CYCLE + 2)){
    sam_cycle = 2;
    total_mips = 0.0;

    double vdd = 0.8 + 0.1*(p_ipa->new_freq - 3e9)/1e9;
    cerr << "id: " << id << " tick: " << manifold::kernel::Clock::Master().NowTicks() << " new_freq: " << p_ipa->new_freq << " old_freq: " << clock->freq << " new_vdd: " << vdd << " old_vdd: " << last_vdd << endl;
    clock->set_frequency(p_ipa->new_freq);
    next_frq = p_ipa->new_freq;

    char ModuleID[64];

    sprintf(ModuleID,"CORE_DIE:FRT%d",id);
    ei->update_variable_partition(string(ModuleID),string("vdd"),vdd);

    sprintf(ModuleID,"CORE_DIE:SCH%d",id);
    ei->update_variable_partition(string(ModuleID),string("vdd"),vdd);
 
    sprintf(ModuleID,"CORE_DIE:INT%d",id);
    ei->update_variable_partition(string(ModuleID),string("vdd"),vdd);
  
    sprintf(ModuleID,"CORE_DIE:FPU%d",id);
    ei->update_variable_partition(string(ModuleID),string("vdd"),vdd);
  
    sprintf(ModuleID,"CORE_DIE:MEM%d",id);
    ei->update_variable_partition(string(ModuleID),string("vdd"),vdd);

  }



#ifdef EI_COMPUTE
  if(sam_cycle == (SAMPLING_CYCLE + 1)){
    char ModuleID[64];
    clock->SetNowTime(p_cnt->time_tick);

    //***Nehalem:Memory***
    //core:DTLB
    sprintf(ModuleID,"core:DTLB:%d",id);
    ei->compute_power(p_cnt->time_tick, p_cnt->period,string(ModuleID),l1_cnt->DTLB * factor);
    //core:DL1
    sprintf(ModuleID,"core:DL1:%d",id);
    ei->compute_power(p_cnt->time_tick, p_cnt->period,string(ModuleID),l1_cnt->DL1 * factor);

    //core:DL2
    sprintf(ModuleID,"core:DL2:%d",id);
    ei->compute_power(p_cnt->time_tick, p_cnt->period,string(ModuleID),l2_cnt->DL2 * 2.2 * factor);

    //core:DL1:MissBuffer
    sprintf(ModuleID,"core:DL1:MissBuffer:%d",id);
    ei->compute_power(p_cnt->time_tick, p_cnt->period,string(ModuleID),l1_cnt->DL1.missbuf * factor);
  
    //core:DL1:FillBuffer
    sprintf(ModuleID,"core:DL1:FillBuffer:%d",id);
    ei->compute_power(p_cnt->time_tick, p_cnt->period,string(ModuleID),l1_cnt->DL1.linefill * factor);

    //core:DL1:PrefetchBuffer
    sprintf(ModuleID,"core:DL1:PrefetchBuffer:%d",id);
    ei->compute_power(p_cnt->time_tick, p_cnt->period,string(ModuleID),l1_cnt->DL1.prefetch * factor);

    //core:DL1:WBB
    sprintf(ModuleID,"core:DL1:WBB:%d",id);
    ei->compute_power(p_cnt->time_tick, p_cnt->period,string(ModuleID),l1_cnt->DL1.writeback * factor);

    l1_cnt->reset();
    l2_cnt->reset();

    synced += 1;

    if(synced == NUM_CORES) {

      synced = 0;

      double ip = 0.0;
      double il = 0.0;

      double ipp = 0.0;
      double ilp = 0.0;

      double ips = 0.0;
      double ils = 0.0;
      double T_MAX = 298.0;

      for(int i=0;i<NUM_CORES;i++) {
        EI::power_t P;
 
	    sprintf(ModuleID,"CORE_DIE:FRT%d",i);
	    P = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
	    ad[i] = P.get_total() - P.leakage;
        P.leakage = P.leakage * factor2;
        P.total = P.leakage + ad[i];
		ei->push_data<EI::power_t>(p_cnt->time_tick,p_cnt->period,string("partition"),string(ModuleID),string("power"),P);
	  
	    sprintf(ModuleID,"CORE_DIE:SCH%d",i);
	    P = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
	    bd[i] = P.get_total() - P.leakage;
	    P.leakage = P.leakage * factor2;
        P.total = P.leakage + bd[i];
		ei->push_data<EI::power_t>(p_cnt->time_tick,p_cnt->period,string("partition"),string(ModuleID),string("power"),P);
	 
	    sprintf(ModuleID,"CORE_DIE:INT%d",i);
	    P = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
	    cd[i] = P.get_total() - P.leakage;
	    P.leakage = P.leakage * factor2;
        P.total = P.leakage + cd[i];
		ei->push_data<EI::power_t>(p_cnt->time_tick,p_cnt->period,string("partition"),string(ModuleID),string("power"),P);
	 
	    sprintf(ModuleID,"CORE_DIE:FPU%d",i);
	    P = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
	    dd[i] = P.get_total() - P.leakage;
	    P.leakage = P.leakage * factor2;
        P.total = P.leakage + dd[i];
		ei->push_data<EI::power_t>(p_cnt->time_tick,p_cnt->period,string("partition"),string(ModuleID),string("power"),P);
	  
	    sprintf(ModuleID,"CORE_DIE:MEM%d",i);
	    P = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
	    ed[i] = P.get_total() - P.leakage;
	    P.leakage = P.leakage * factor2;
        P.total = P.leakage + ed[i];
		ei->push_data<EI::power_t>(p_cnt->time_tick,p_cnt->period,string("partition"),string(ModuleID),string("power"),P);
	
	    sprintf(ModuleID,"DL2_DIE:SPOT%d",i);
	    P = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
	    fd[i] = P.get_total() - P.leakage;
        P.leakage = P.leakage / factor3;
        P.total = P.leakage + fd[i];
		ei->push_data<EI::power_t>(p_cnt->time_tick,p_cnt->period,string("partition"),string(ModuleID),string("power"),P);
	  }

      double T;
      ei->compute_temperature(p_cnt->time_tick, p_cnt->period, "16CORE");

      for(int i=0;i<NUM_CORES;i++) {
        double s = 0.0;
        double t = 0.0;
        double a,b,c,d,e,f;
        double al, bl, cl, dl, el, fl;
 
        EI::power_t update_p;

        sprintf(ModuleID,"CORE_DIE:FRT%d",i);
	      update_p = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
        al = update_p.leakage * factor2;
        a = al + ad[i];
        s += a;
        t += al;

		sprintf(ModuleID,"CORE_DIE:SCH%d",i);
	    update_p = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
        bl = update_p.leakage * factor2;
        b = bl + bd[i];
        s += b;
        t += bl;

		sprintf(ModuleID,"CORE_DIE:SCH%d",i);
	    update_p = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
        cl = update_p.leakage * factor2;
        c = cl + cd[i];
        s += c;
        t += cl;

		sprintf(ModuleID,"CORE_DIE:FPU%d",i);
    	update_p = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
        dl = update_p.leakage * factor2;
        d = dl + dd[i];
        s += d;
        t += dl;

		sprintf(ModuleID,"CORE_DIE:MEM%d",i);
	    update_p = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
        el = update_p.leakage * factor2;
	    e = el + ed[i];
        s += e;
        t += el;
			
		sprintf(ModuleID,"DL2_DIE:SPOT%d",i);
		update_p = ei->pull_data<EI::power_t>(p_cnt->time_tick,string("partition"),string(ModuleID),string("power"));
        fl = update_p.leakage / factor3;
        f = fl + fd[i];
        T = ei->pull_data<double>(p_cnt->time_tick,string("partition"),string(ModuleID),string("temperature"));

        total_energy += (s + f) * p_cnt->period;
        core_energy += s * p_cnt->period;
        sram_energy += f * p_cnt->period;
  
        total_leakage += (t + fl) * p_cnt->period;
        core_leakage += t * p_cnt->period;
        sram_leakage += fl * p_cnt->period;

        ip += s + f;
        il += t + fl;
        ipp += s;
        ilp += t;
        ips += f;
        ils += fl;

        cerr<<i<<"(power)  \t"<<a<<","<<b<<","<<c<<","<<d<<","<<e<<", "<<f<<"\t"<<s<<"+"<<f<<"="<<s+f<<": "<<T<<endl<<flush;
        cerr<<i<<"(leakage)\t"<<al<<","<<bl<<","<<cl<<","<<dl<<","<<el<<", "<<fl<<"\t"<<t<<"+"<<fl<<"="<<t+fl<<endl<<flush;
         //cerr<<i<<"(dynamic)\t"<<ad[i]<<","<<bd[i]<<","<<cd[i]<<","<<dd[i]<<","<<ed[i]<<", "<<fd[i]<<endl<<flush;
      }
            
#if 0
      EI::grid_t<double> thermal_grid = ei->pull_data<EI::grid_t<double> >(p_cnt->time_tick,"package","16CORE","thermal_grid");
      for(int col_index = 0; col_index < 50; col_index++) {
        for(int row_index = 0; row_index < 50; row_index++)
          cerr << thermal_grid.pull(row_index,col_index,0) << ",";
          cerr << endl;
         }
      cerr << "**************************************************" << endl;
#endif
#if 0
      EI::grid_t<double> thermal_grid = ei->pull_data<EI::grid_t<double> >(p_cnt->time_tick,"package","16CORE","thermal_grid");
      for(int col_index = 0; col_index < 50; col_index++) {
        for(int row_index = 0; row_index < 50; row_index++)
          cerr << thermal_grid.pull(row_index,col_index,1) << ",";
          cerr << endl;
        }
      cerr << "**************************************************" << endl;
#endif
      double time = manifold::kernel::Clock::Master().NowTime();

      cerr<<"instantaneous power: "<<ip<<", instantaneous leakage: "<<il<<endl<<flush;
      cerr<<"instantaneous power(core): "<<ipp<<", instantaneous leakage(core): "<<ilp<<endl<<flush;
      cerr<<"instantaneous power(sram): "<<ips<<", instantaneous leakage(sram): "<<ils<<endl<<flush;

      cerr<<"total energy: "<<total_energy<<", total leakage energy: "<<total_leakage<<endl<<flush;
      cerr<<"total energy(core): "<<core_energy<<", total leakage energy(core): "<<core_leakage<<endl<<flush;
      cerr<<"total energy(sram): "<<sram_energy<<", total leakage energy(sram): "<<sram_leakage<<endl<<flush;

      cerr<<"average power: "<<total_energy/time<<", average leakage: "<<total_leakage/time<<endl<<flush;
      cerr<<"average power(core): "<<core_energy/time<<", average leakage(core): "<<core_leakage/time<<endl<<flush;
      cerr<<"average power(sram): "<<sram_energy/time<<", average leakage(sram): "<<sram_leakage/time<<endl<<flush;
    } 
  }
#endif
  
  if(sam_cycle == SAMPLING_CYCLE)
  {
    char ModuleID[64];

    p_cnt->time_tick += p_cnt->period; // update time_tick
#ifdef EI_COMPUTE
    //***Nehalem:Frontend***
    //core:instruction_buffer
    sprintf(ModuleID,"core:instruction_buffer:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->inst_buffer * factor);

    //core:BTB
    sprintf(ModuleID,"core:BTB:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),(p_cnt->l1_btb + p_cnt->l2_btb) * factor);

    //core:chooser
    sprintf(ModuleID,"core:chooser:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->predictor_chooser * factor);

    //core:global_predictor
    sprintf(ModuleID,"core:global_predictor:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->global_predictor * factor);

    //core:L1_local_predictor
    sprintf(ModuleID,"core:L1_local_predictor:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->l1_local_predictor * factor);

    //core:L2_local_predictor
    sprintf(ModuleID,"core:L2_local_predictor:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->l2_local_predictor * factor);

    //core:ITLB
    sprintf(ModuleID,"core:ITLB:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->inst_tlb * factor);

    //core:IL1
    sprintf(ModuleID,"core:IL1:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->inst_cache * factor);

    //core:IL1:MissBuffer
    sprintf(ModuleID,"core:IL1:MissBuffer:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->inst_cache_miss_buffer * factor);

    //core:IL1:FillBuffer //FIXME
    //sprintf(ModuleID,"core:IL1:FillBuffer:%d",id);
    //p_cnt->IL1.linefill = p_cnt->IL1.linefill * 2;
    //ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->IL1.linefill * factor);

    //core:IL1:PrefetchBuffer
    sprintf(ModuleID,"core:IL1:PrefetchBuffer:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->inst_cache_prefetch_buffer * factor);
 
    //core:instruction_decoder
    sprintf(ModuleID,"core:instruction_decoder:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->inst_decoder * factor);

    //core:operand_decoder
    sprintf(ModuleID,"core:operand_decoder:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->operand_decoder * factor);

    //core:uop_sequencer
    sprintf(ModuleID,"core:uop_sequencer:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->uop_sequencer * factor);

    //core:uop_queue
    sprintf(ModuleID,"core:uop_queue:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->uop_queue * factor);

    //core:RAS
    sprintf(ModuleID,"core:RAS:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->ras * factor);

    //***Nehalem:Schedule***
    //core:RS
    sprintf(ModuleID,"core:RS:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->rs * factor);

    //core:ROB
    sprintf(ModuleID,"core:ROB:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->rob * factor);

    //core:issue_select
    sprintf(ModuleID,"core:issue_select:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->issue_select * factor);

    //core:GPREG
    sprintf(ModuleID,"core:GPREG:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->reg_int * factor);

    //core:SCFREG //FIXME
    //sprintf(ModuleID,"core:SCFREG:%d",id);
    //ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->registers.segment 
      //                 + p_cnt->registers.control);

    //core:FPREG
    sprintf(ModuleID,"core:FPREG:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->reg_fp * factor);

    //core:RAT
    sprintf(ModuleID,"core:RAT:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->rat * factor);

    //core:freelist
    sprintf(ModuleID,"core:freelist:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->freelist * factor);

    //core:dependency_check
    sprintf(ModuleID,"core:dependency_check:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->dependency_check * factor);

    //***Nehalem:Execute:INT***
    //core:ALU
    sprintf(ModuleID,"core:ALU:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->alu * factor);

    //core:ALU_bypass
    sprintf(ModuleID,"core:ALU_bypass:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->int_bypass * factor);

    //***Nehalem:Execute:FP***
    //core:FPU
    sprintf(ModuleID,"core:FPU:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->fpu * factor);

    //core:MUL
    sprintf(ModuleID,"core:MUL:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->mul * factor);

    //core:FPU_bypass
    sprintf(ModuleID,"core:FPU_bypass:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->fp_bypass * factor);

    //core:MUL_bypass //FIXME
    //sprintf(ModuleID,"core:MUL_bypass:%d",id);
    //p_cnt->mul_bypass = p_cnt->mul_bypass * 5;
    //ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->mul_bypass);

    //core:StoreQueue
    sprintf(ModuleID,"core:StoreQueue:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->stq * factor);

    //core:LoadQueue
    sprintf(ModuleID,"core:LoadQueue:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->ldq * factor);

    //core:LD_bypass //FIXME
    //sprintf(ModuleID,"core:LD_bypass:%d",id);
    //p_cnt->load_bypass = p_cnt->load_bypass * 5;
    //ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->load_bypass);

    //***Wiring & Latch
    //core:PC
    sprintf(ModuleID,"core:PC:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->pc * factor);

    //core:pipe:IB2ID
    sprintf(ModuleID,"core:pipe:IB2ID:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->latch_ib2id * factor);

    //core:pipe:ID2uQ
    sprintf(ModuleID,"core:pipe:ID2uQ:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->latch_id2uq * factor);

    //core:pipe:uQ2RR
    sprintf(ModuleID,"core:pipe:uQ2RR:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->latch_uq2rat * factor);

    //core:pipe:RR2SCH
    sprintf(ModuleID,"core:pipe:RR2SCH:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->latch_rat2rs * factor);

    //core:pipe:SCH2EXP
    sprintf(ModuleID,"core:pipe:SCH2EXP:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->latch_rs2ex * factor);

    //core:pipe:EXP2ALU
    sprintf(ModuleID,"core:pipe:EXP2ALU:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->latch_ex_int* factor);

    //core:pipe:EXP2FPU
    sprintf(ModuleID,"core:pipe:EXP2FPU:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->latch_ex_fp * factor);

    //core:pipe:ALU2ROB
    sprintf(ModuleID,"core:pipe:ALU2ROB:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),p_cnt->alu * factor);

    //core:pipe:FPU2ROB
    sprintf(ModuleID,"core:pipe:FPU2ROB:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),(p_cnt->fpu + p_cnt->mul)* factor);

    //core:pipe:ROB2CM
    sprintf(ModuleID,"core:pipe:ROB2CM:%d",id);
    ei->compute_power(p_cnt->time_tick,p_cnt->period,string(ModuleID),(p_cnt->latch_rob2rs + p_cnt->latch_rob2arf + p_cnt->latch_ldq2rs) * factor);
#endif
    total_mips += p_cnt->retire_inst.read/p_cnt->period/1e6;
    cerr << "id: " << id <<" cycle: "<<clock->NowTicks()<<"|| fetched inst: "<<p_cnt->fetch_inst.read<<" committed inst: "<<p_cnt->retire_inst.read<<", IPC_inst: "<<((float)p_cnt->retire_inst.read)/SAMPLING_CYCLE<<", MIPS: "<<p_cnt->retire_inst.read/p_cnt->period/1e6<<" total_MIPS: "<<total_mips<<endl<<flush;

    p_cnt->reset();
    
  }
}

