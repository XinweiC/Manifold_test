#include "outorder.h"
#include "core.h"

using namespace std;
using namespace libconfig;
using namespace manifold;
using namespace manifold::spx;

outorder_t::outorder_t(spx_core_t *spx_core, libconfig::Config *parser)
{
  type = SPX_PIPELINE_OUTORDER;
  core = spx_core;

  // set config
  set_config(parser);
  
  // initialize core components
  instQ = new instQ_t((pipeline_t*)this,config.instQ_size,config.IL1.offset_mask+config.IL1.index_mask);
  ROB = new ROB_t((pipeline_t*)this,config.ROB_size);
  RS = new RS_t((pipeline_t*)this,config.RS_size,config.exec_width,config.FU_port);
  STQ = new STQ_t((pipeline_t*)this,config.STQ_size);
  LDQ = new LDQ_t((pipeline_t*)this,config.LDQ_size);
  RF = new RF_t((pipeline_t*)this);
  for(int i = 0; i < config.exec_width; i++)
    EX.push_back(new EX_t((pipeline_t*)this,i,config.FU_port,config.FU_delay,config.FU_issue_rate));
}

outorder_t::~outorder_t()
{
  delete instQ;
  delete ROB;
  delete RS;
  delete STQ;
  delete LDQ;
  delete RF;
  
  for(vector<EX_t*>::iterator it = EX.begin(); it != EX.end(); it++)
    delete *it;
  EX.clear();
}

void outorder_t::set_config(libconfig::Config *parser)
{
  try
  {
    config.fetch_width = parser->lookup("fetch_width");
    config.alloc_width = parser->lookup("allocate_width");
    config.exec_width = parser->lookup("execute_width");
    config.commit_width = parser->lookup("commit_width");
    
    config.instQ_size = parser->lookup("instQ_size");
    config.RS_size = parser->lookup("RS_size");
    config.LDQ_size = parser->lookup("LDQ_size");
    config.STQ_size = parser->lookup("STQ_size");
    config.ROB_size = parser->lookup("ROB_size");
    
    config.FU_delay[SPX_FU_INT] = parser->lookup("FU_INT.delay");
    config.FU_delay[SPX_FU_MUL] = parser->lookup("FU_MUL.delay");
    config.FU_delay[SPX_FU_FP] = parser->lookup("FU_FP.delay");
    config.FU_delay[SPX_FU_MOV] = parser->lookup("FU_MOV.delay");
    config.FU_delay[SPX_FU_BR] = parser->lookup("FU_BR.delay");
    config.FU_delay[SPX_FU_LD] = parser->lookup("FU_LD.delay");
    config.FU_delay[SPX_FU_ST] = parser->lookup("FU_ST.delay");
    
    config.FU_issue_rate[SPX_FU_INT] = parser->lookup("FU_INT.issue_rate");
    config.FU_issue_rate[SPX_FU_MUL] = parser->lookup("FU_MUL.issue_rate");
    config.FU_issue_rate[SPX_FU_FP] = parser->lookup("FU_FP.issue_rate");
    config.FU_issue_rate[SPX_FU_MOV] = parser->lookup("FU_MOV.issue_rate");
    config.FU_issue_rate[SPX_FU_BR] = parser->lookup("FU_BR.issue_rate");
    config.FU_issue_rate[SPX_FU_LD] = parser->lookup("FU_LD.issue_rate");
    config.FU_issue_rate[SPX_FU_ST] = parser->lookup("FU_ST.issue_rate");

    Setting *setting;
    setting = &parser->lookup("FU_INT.port");
    for(int i = 0; i < setting->getLength(); i++)
      config.FU_port[SPX_FU_INT].push_back((*setting)[i]);
    setting = &parser->lookup("FU_MUL.port");
    for(int i = 0; i < setting->getLength(); i++)
      config.FU_port[SPX_FU_MUL].push_back((*setting)[i]);
    setting = &parser->lookup("FU_FP.port");
    for(int i = 0; i < setting->getLength(); i++)
      config.FU_port[SPX_FU_FP].push_back((*setting)[i]);
    setting = &parser->lookup("FU_MOV.port");
    for(int i = 0; i < setting->getLength(); i++)
      config.FU_port[SPX_FU_MOV].push_back((*setting)[i]);
    setting = &parser->lookup("FU_BR.port");
    for(int i = 0; i < setting->getLength(); i++)
      config.FU_port[SPX_FU_BR].push_back((*setting)[i]);
    setting = &parser->lookup("FU_LD.port");
    for(int i = 0; i < setting->getLength(); i++)
      config.FU_port[SPX_FU_LD].push_back((*setting)[i]);
    setting = &parser->lookup("FU_ST.port");
    for(int i = 0; i < setting->getLength(); i++)
      config.FU_port[SPX_FU_ST].push_back((*setting)[i]);

    config.IL1.size = parser->lookup("IL1.size");
    config.IL1.line_width = parser->lookup("IL1.line_width");
    config.IL1.assoc = parser->lookup("IL1.assoc");
    
    config.DL1.size = parser->lookup("DL1.size");
    config.DL1.line_width = parser->lookup("DL1.line_width");
    config.DL1.assoc = parser->lookup("DL1.assoc");

    int bit_mask;

    bit_mask = 0x01;
    config.IL1.offset_mask = 0;
    while(bit_mask < config.IL1.line_width)
    {
      config.IL1.offset_mask++;
      bit_mask = bit_mask << 1;
    }
    bit_mask = 0x01;
    config.IL1.index_mask = 0;
    while(bit_mask < config.IL1.assoc)
    {
      config.IL1.index_mask++;
      bit_mask = bit_mask << 1;
    }
    bit_mask = 0x01;
    config.DL1.offset_mask = 0;
    while(bit_mask < config.DL1.line_width)
    {
      config.DL1.offset_mask++;
      bit_mask = bit_mask << 1;
    }
    bit_mask = 0x01;
    config.DL1.index_mask = 0;
    while(bit_mask < config.DL1.assoc)
    {
      config.DL1.index_mask++;
      bit_mask = bit_mask << 1;
    }
  }
  catch(SettingNotFoundException e)
  {
    fprintf(stderr,"%s not defined\n",e.getPath());
    exit(1);
  }
  catch(SettingTypeException e)
  {
    fprintf(stderr,"%s has incorrect type\n",e.getPath());
    exit(1);
  }
}

void outorder_t::commit()
{
  for(int committed = 0; committed < config.commit_width; committed++)
  {
    inst_t *inst = ROB->get_front();
    
    if(inst)
    {
      /*stats.eff_Mop_count++;
      stats.eff_uop_count++;
      stats.interval.eff_Mop_count++;
      stats.interval.eff_uop_count++;*/
      stats.uop_count++;
      stats.interval.uop_count++;

  #if 0
  //#ifdef SPX_QSIM_DEBUG_
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: Committed uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence);
  #endif


      //if(inst->is_idle) handle_idle_inst(inst);
 
      ROB->pop_front();
      RF->writeback(inst);
      stats.last_commit_cycle = core->clock_cycle;

      counters->retire_inst.read++;

      if(inst->uop_sequence % core->ipa->window_m == 0) {
        double y_dev = ((double)core->ipa->window_m) /
            (inst->ipa.depart_dev - core->ipa->update_last);//IPA derivative 
    
   //          double y_dev = (1/core->ipa->window_m)*(core->ipa->window_m/((manifold::kernel::Clock::Master().NowTicks() - core->ipa->update_cycle_last)*manifold::kernel::Clock::Master().freq))*(core->ipa->window_m/((manifold::kernel::Clock::Master().NowTicks() - core->ipa->update_cycle_last)*manifold::kernel::Clock::Master().freq))* inst->ipa.depart_dev;      //wendy

        double new_freq = core->clock->freq +
                    0.2*(core->ipa->target - core->ipa->window_m * manifold::kernel::Clock::Master().freq
                       / (manifold::kernel::Clock::Master().NowTicks() - core->ipa->update_cycle_last)) / y_dev;   //constant change here   
      //  fprintf(stderr,"SPX_DEBUG (core %d) | %lu [%lu,%lu] : real: %f avr: %f (%lu, %lu) ::",core->core_id,core->clock_cycle,core->clock->NowTicks(),core->ipa->update_cycle_last,((double)core->ipa->window_m) * manifold::kernel::Clock::Master().freq / (manifold::kernel::Clock::Master().NowTicks() - core->ipa->update_cycle_last), ((double)inst->uop_sequence) * manifold::kernel::Clock::Master().freq / manifold::kernel::Clock::Master().NowTicks(), inst->uop_sequence, manifold::kernel::Clock::Master().NowTicks());
        //fprintf(stderr," y_dev: %f d: %lu l: %lu (target %f) [new clock %f]\n", y_dev, inst->ipa.depart_dev, core->ipa->update_last, core->ipa->target,new_freq);
       // fprintf(stderr,"seq: %ld, absoluttime: %f, master: %f\n",inst->uop_sequence,(double)core->clock->NextTickTime(), manifold::kernel::Clock::Master().freq);   //wendy


        core->ipa->update_cycle_last = manifold::kernel::Clock::Master().NowTicks();
        if (new_freq > 5.0e9) new_freq = 5.0e9;
        if (new_freq < 0.5e9) new_freq = 0.5e9;
        core->ipa->new_freq = ( 0.7 * new_freq + 0.3 * core->ipa->new_freq );  //comment to get origin
        core->ipa->update_last = inst->ipa.depart_dev;
      }
      
      if(inst->memcode == SPX_MEM_ST) // store
      {  STQ->schedule(inst); // update this inst ready to cache
         //STQ->pop(inst);
         //delete inst;          
      } else
        delete inst;
    }
    else // empty ROB or ROB head not completed
      break;
  }

  if((core->clock_cycle - stats.last_commit_cycle > 2500000) && (idle_loop == false))
  {
    fprintf(stderr,"SPX_WARNING (core %d) @ %lu : possible pipeline deadlock at %lu\n",core->core_id,core->clock_cycle,stats.last_commit_cycle);
    exit(1);
  }

#ifdef libEI
  //counters->latch_rob2arf.switching++;
  counters->latch_rob2arf.read++;
#endif
}

void outorder_t::memory()
{
  // process memory queues -- send cache requests to L1 if any
  // note that execute() follows commit(),
  // so if any store inst as ROB head was handled by STQ->schedule();
  // the cache_request is sent out to cache in the same clock cycle
  STQ->handle_cache();
  LDQ->handle_cache();

#ifdef LIBEI
  // Latches switch all the time.
  //counters->latch_stq2mem.switching++;
  //counters->latch_stq2ldq.switching++;
  //counters->latch_ldq2mem.switching++;
  //counters->latch_ldq2rs.switching++;
  counters->latch_stq2mem.read++;
  counters->latch_stq2ldq.read++;
  counters->latch_ldq2mem.read++;
  counters->latch_ldq2rs.read++;
#endif
}

void outorder_t::execute()
{
  // process execution units
  for(int port = 0; port < config.exec_width; port++)
  {
    // EX to ROB scheduling
    inst_t *inst = EX[port]->get_front();
    if(inst)
    {
      EX[port]->pop_front(inst->excode); // remove this inst from EX pipe
#ifdef IPA_CTRL
      inst->ipa.compl_dev = core->clock->NowTicks();
      inst->ipa.depart_dev = ((inst->ipa.compl_dev + 1) > core->ipa->depart_last) ? (inst->ipa.compl_dev + 1) : (core->ipa->depart_last + 1);
      core->ipa->depart_last = inst->ipa.depart_dev;
#endif
      if(inst->memcode == SPX_MEM_LD) // load
        LDQ->schedule(inst); // schedule this inst to cache
      else
      {
        if(inst->memcode == SPX_MEM_ST) // store
          STQ->store_forward(inst); // wake up loads that are blocked by this store due to memory disambiguation
        RS->update(inst); // wake up all dependent insts
        ROB->update(inst); // mark this inst completed
      }
    }
  }

  for(int port = 0; port < config.exec_width; port++)
  {
    // RS to EX scheduling
    inst_t *inst = RS->get_front(port);
    if(inst) // there is a ready instruction to start execution
    {
      if(EX[inst->port]->is_available(inst->excode))
      {
        RS->pop_front(port);
        EX[inst->port]->push_back(inst);
      }
    }
  }

#ifdef LIBEI
  // Latches switch all the time.
  //counters->latch_ex_int.switching++;
  //counters->latch_ex_fp.switching++;
  //counters->ex_int_undiff.switching++;
  //counters->ex_fp_undiff.switching++;
  counters->latch_ex_int.read++;
  counters->latch_ex_fp.read++;
  counters->ex_int_undiff.read++;
  counters->ex_fp_undiff.read++;
#endif
}

void outorder_t::allocate()
{
  for(int alloced = 0; alloced < config.alloc_width; alloced++)
  {
    inst_t *inst = instQ->get_front();
    if(inst)
    {
      if(ROB->is_available())
      {
        if(RS->is_available())
        {
          if(inst->memcode == SPX_MEM_ST) // store
          {
            if(STQ->is_available())
            {
              STQ->push_back(inst);
              stats.st_count++;
            }
            else // STQ full
              { break; }

          }
          else if(inst->memcode == SPX_MEM_LD) // load
          {
            if(LDQ->is_available())
            {
              LDQ->push_back(inst);
              STQ->mem_disamb_check(inst);
              stats.ld_count++;
            }
            else // LDQ full
              { break; }

          } else {
            stats.nm_count++;
          }
          
          RF->resolve_dependency(inst);
          RS->port_binding(inst);
          RS->push_back(inst);
        }
        else // RS full
          { break; }
        
        instQ->pop_front();
        ROB->push_back(inst);
      }
      else // ROB full
        { break; }
    }
    else // empty instQ
      { break; }
  }

#ifdef LIBEI
  // Latches switch all the time.
  //counters->latch_uq2rat.switching++; // latch from uop queue to renaming logic
  //counters->latch_rat2rs.switching++; // latch from renaming logic to RS (scheduler)
  //counters->latch_rob2rs.switching++; // latch from ROB (operands in physical register) to RS
  //counters->latch_rs2ex.switching++; // latch from RS to execute pipeline
  //counters->scheduler_undiff.switching++; // undifferentiated core (i.e., control logics)
  counters->latch_uq2rat.read++; // latch from uop queue to renaming logic
  counters->latch_rat2rs.read++; // latch from renaming logic to RS (scheduler)
  counters->latch_rob2rs.read++; // latch from ROB (operands in physical register) to RS
  counters->latch_rs2ex.read++; // latch from RS to execute pipeline
  counters->scheduler_undiff.read++; // undifferentiated core (i.e., control logics)
#endif
}

void outorder_t::frontend()
{
  if(core->ipa->is_transient == true) { 
    if (trans_flag == 0)
        cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : enters transient" << endl;

    trans_flag = 2;
    return;
  } else if (trans_flag == 2) {
    trans_flag = 0;
    cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : exits transient" << endl;
    core->clock_cycle,stats.last_commit_cycle = core->clock_cycle;
  }

  if(idle_loop) {
    int rc = Qsim_osd->run(core->core_id,1);
    if (!rc&&!Qsim_osd->booted(core->core_id)) {
      if(runout_flag == 0) {
        cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : run out of insts" << endl;
        runout_flag = 1;
      }
		  //manifold :: kernel :: Manifold :: Terminate();      
    } else if(Qsim_osd->idle(core->core_id)) {
      return ;
    } else { 
      //cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : recover from idle" << endl;
      idle_loop = false;
      runout_flag = 0;
      stats.last_commit_cycle = core->clock_cycle;
    }
  }

//#ifdef SPX_QSIM_DEBUG_
#if 0
if (core->core_id == 1) {
  cerr << "Core ID: " << core->core_id << ", Clock: " << core->clock_cycle << ", Fetched: " << stats.Mop_count << ", Committed: " << stats.uop_count
       << " || instQ (Avb): " << instQ->size - instQ->occupancy << ", ROB (Avb): " << ROB->size - ROB->occupancy  << ", RS (Avb): " 
       << RS->size - RS->occupancy  << ", STQ (Avb): " << STQ->size - STQ->occupancy  << ", LDQ (Avb): " << LDQ->size - LDQ->occupancy  << endl;
  cerr << endl;
}
#endif

  for(int fetched = 0; (fetched < config.fetch_width)&&!idle_loop; fetched++)
  {
    if(instQ->is_available())
    {
      stats.Mop_count++;
      stats.interval.Mop_count++;
      
      if(Qsim_osd->get_prot(core->core_id) == Qsim::OSDomain::PROT_KERN) {
        stats.KERN_count++;
        //cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : in KERN state" << endl;
      } else {
        stats.USER_count++;
        //cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : in USER state" << endl;
     }

      next_inst = new inst_t(core,++Mop_count,++uop_count);

      if(config.qsim == SPX_QSIM_LIB) {
        int rc = Qsim_osd->run(core->core_id,1);
        if (!rc&&!Qsim_osd->booted(core->core_id)) {
          if(runout_flag == 0) {
            cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : run out of insts" << endl;
            runout_flag = 1;
          }
		      //manifold :: kernel :: Manifold :: Terminate();      
        } else if(Qsim_osd->idle(core->core_id)) {
          //cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : enters idle" << endl;
          idle_loop = true;
        }
      } 
      else {} // SPX_QSIM_CLIENT
        //Qsim_client->run(core->core_id,1);
      
      if(!next_inst->inflight)
      {
        Qsim_post_cb(next_inst);
        fetch(next_inst);
      }

      if(next_inst->Mop_head->Mop_length > 4)
        handle_long_inst(next_inst);

      next_inst = NULL;
    }
    else // instQ full
    { 
     // cerr << "@ " << dec << core->clock_cycle << " Core " << core->core_id << " : instQ full" << endl;
      break; 
    }
  }

#ifdef LIBEI
  // Latches switch all the time.
  //counters->pc.switching++; // PC update
  //counters->latch_ic2ib.switching++; // latch from inst cache to inst buffer
  //counters->latch_ib2id.switching++; // latch from inst buffer to inst decoder
  //counters->latch_id2uq.switching++; // latch from inst decoder to uop queue
  //counters->frontend_undiff.switching++; // undifferentiated core (i.e., control logics)
  counters->pc.read++; // PC update
  counters->latch_ic2ib.read++; // latch from inst cache to inst buffer
  counters->latch_ib2id.read++; // latch from inst buffer to inst decoder
  counters->latch_id2uq.read++; // latch from inst decoder to uop queue
  counters->frontend_undiff.read++; // undifferentiated core (i.e., control logics)
#endif
}

void outorder_t::fetch(inst_t *inst)
{
//#ifdef SPX_QSIM_DEBUG_
#if 0
if (inst->core->core_id == 1) {
  fprintf(stderr,"^SPX_QSIM_DEBUG (core %d): uop %lu (Mop %lu) | Len:%d | ",inst->core->core_id,inst->uop_sequence,inst->Mop_sequence,inst->Mop_length);

  switch(type)
  {
    case QSIM_INST_NULL: fprintf(stderr,"QSIM_INST_NULL"); break;
    case QSIM_INST_INTBASIC: fprintf(stderr,"QSIM_INST_INTBASIC"); break;
    case QSIM_INST_INTMUL: fprintf(stderr,"QSIM_INST_INTMUL"); break;
    case QSIM_INST_INTDIV: fprintf(stderr,"QSIM_INST_INTDIV"); break;
    case QSIM_INST_STACK: fprintf(stderr,"QSIM_INST_STACK"); break;
    case QSIM_INST_BR: fprintf(stderr,"QSIM_INST_BR"); break;
    case QSIM_INST_CALL: fprintf(stderr,"QSIM_INST_CALL"); break;
    case QSIM_INST_RET: fprintf(stderr,"QSIM_INST_RET"); break;
    case QSIM_INST_TRAP: fprintf(stderr,"QSIM_INST_TRAP"); break;
    case QSIM_INST_FPBASIC: fprintf(stderr,"QSIM_INST_FPBASIC"); break;
    case QSIM_INST_FPMUL: fprintf(stderr,"QSIM_INST_FPMUL"); break;
    case QSIM_INST_FPDIV: fprintf(stderr,"QSIM_INST_FPDIV"); break;
    default: break;
  }
  fprintf(stderr," ");


  switch(inst->excode)
  {
    case SPX_FU_INT: fprintf(stderr,"FU_INT"); break;
    case SPX_FU_MUL: fprintf(stderr,"FU_MUL"); break;
    case SPX_FU_FP: fprintf(stderr,"FU_FP"); break;
    case SPX_FU_MOV: fprintf(stderr,"FU_MOV"); break;
    case SPX_FU_BR: fprintf(stderr,"FU_BR"); break;
    case SPX_FU_LD: fprintf(stderr,"FU_LD Addr = %lx", inst->data.paddr); break;
    case SPX_FU_ST: fprintf(stderr,"FU_ST Addr = %lx", inst->data.paddr); break;
    default: break;
  }
  fprintf(stderr,"\n");
}
#endif
  instQ->push_back(inst);

  if(is_nop(inst)) {
    stats.nop_count++;
    //stats.Mop_count--;
    //stats.uop_count--;
    stats.interval.Mop_count--;
    stats.interval.uop_count--;
  }

  counters->fetch_inst.read++;
}

void outorder_t::handle_cache_response(int temp, cache_request_t *cache_request)
{
  inst_t *inst = cache_request->inst;

  //#ifdef SPX_DEBUG
  //#ifdef SPX_QSIM_DEBUG_
  #if 0
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: cache_response uop %lu (Mop %lu) (Addr: %lx)",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence,cache_request->addr);
  #endif

  switch(cache_request->op_type)
  {
    case SPX_MEM_LD:
      LDQ->pop(inst);
      RS->update(inst);
      ROB->update(inst);
//#ifdef SPX_QSIM_DEBUG_
#if 0
      fprintf(stderr, " LD\n");
#endif
      break;
    case SPX_MEM_ST:
      //STQ->pop(inst);
      //delete inst;
//#ifdef SPX_QSIM_DEBUG_
#if 0
      fprintf(stderr, " ST\n");
#endif
      break;
    default:
      fprintf(stderr,"SPX_ERROR (core %d): strange cache response %d received\n",core->core_id,cache_request->op_type);
      exit(1);
  }

  delete cache_request;
}

