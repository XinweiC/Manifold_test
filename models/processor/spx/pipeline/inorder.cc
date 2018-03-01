#include "inorder.h"
#include "core.h"

using namespace std;
using namespace libconfig;
using namespace manifold;
using namespace manifold::spx;

inorder_t::inorder_t(spx_core_t *spx_core, libconfig::Config *parser)
{
  type = SPX_PIPELINE_INORDER;
  core = spx_core;

  // set config
  set_config(parser);
  
  // initialize core components
  instQ = new instQ_t((pipeline_t*)this,config.instQ_size,config.IL1.offset_mask+config.IL1.index_mask);
  threadQ = new RS_t((pipeline_t*)this,1,config.exec_width,config.FU_port);
  RF = new RF_t((pipeline_t*)this);
  for(int i = 0; i < config.exec_width; i++)
    EX.push_back(new EX_t((pipeline_t*)this,i,config.FU_port,config.FU_delay,config.FU_issue_rate));
}

inorder_t::~inorder_t()
{
  delete instQ;
  delete threadQ;
  delete RF;
  
  for(vector<EX_t*>::iterator it = EX.begin(); it != EX.end(); it++)
    delete *it;
  EX.clear();
}

void inorder_t::set_config(libconfig::Config *parser)
{
  try
  {
    config.exec_width = parser->lookup("execute_width");
    
    config.instQ_size = parser->lookup("instQ_size");
    
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
    
    config.IL1.size = parser->lookup("DL1.size");
    config.IL1.line_width = parser->lookup("DL1.line_width");
    config.IL1.assoc = parser->lookup("DL1.assoc");

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
    fprintf(stdout,"%s not defined\n",e.getPath());
    exit(1);
  }
  catch(SettingTypeException e)
  {
    fprintf(stdout,"%s has incorrect type\n",e.getPath());
    exit(1);
  }
}

void inorder_t::commit()
{
  // nothing to do
}

void inorder_t::memory()
{
  // nothing to do
}

void inorder_t::execute()
{
  // process execution units
  for(int port = 0; port < config.exec_width; port++)
  {
    // EX to commit scheduling
    inst_t *inst = EX[port]->get_front();
    if(inst)
    {
      EX[port]->pop_front(inst->excode); // remove this inst from EX pipe
      
      switch(inst->memcode)
      {
        case SPX_MEM_LD: // load
        {
          // send memory load request to cache
          cache_request_t *cache_request = new cache_request_t(inst,inst->core->core_id,inst->core->node_id,inst->data.paddr,SPX_MEM_LD);
          inst->core->Send(inst->core->OUT_TO_CACHE,cache_request);
          break;
        }
        case SPX_MEM_ST:
        {
          // send memory store request to cache
          cache_request_t *cache_request = new cache_request_t(inst,inst->core->core_id,inst->core->node_id,inst->data.paddr,SPX_MEM_ST);
          inst->core->Send(inst->core->OUT_TO_CACHE,cache_request);
          threadQ->update(inst); // update all dependent insts
          RF->writeback(inst); // commit result
          break;
        }
        default:
        {
          threadQ->update(inst); // wake up all dependent insts
          RF->writeback(inst); // commit result
          delete inst;
          stats.last_commit_cycle = core->clock_cycle;
          break;
        }
      }
    }
  }

  if(core->clock_cycle - stats.last_commit_cycle > 5000)
  {
    fprintf(stdout,"SPX_ERROR (core %d): possible pipeline deadlock at %lu\n",core->core_id,stats.last_commit_cycle);
    exit(1);
  }

  for(int port = 0; port < config.exec_width; port++)
  {
    // threadQ to EX scheduling
    inst_t *inst = threadQ->get_front(port);
    if(inst) // there is a ready instruction to start execution
    {
      if(EX[inst->port]->is_available(inst->excode))
      {
        threadQ->pop_front(port);
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

void inorder_t::allocate()
{
  inst_t *inst = instQ->get_front();
  if(inst)
  {
    if(threadQ->is_available())
    {
      RF->resolve_dependency(inst);
      threadQ->port_binding(inst);
      threadQ->push_back(inst);

      instQ->pop_front();
    }
  }

#ifdef LIBEI
  // Latches switch all the time.
  //counters->latch_uq2rat.switching++; // latch from uop queue to renaming logic
  counters->latch_uq2rat.read++; // latch from uop queue to renaming logic
  // Note: inorder core doesn't have RS or ROB, but but call the scheduler as RS for naming convenience.
  //counters->latch_rat2rs.switching++; // latch from renaming logic to scheduler (i.e., RS for outorder)
  counters->latch_rat2rs.read++; // latch from renaming logic to scheduler (i.e., RS for outorder)
  //counters->latch_rob2rs.switching++; // latch from ROB (operands in physical register) to RS
  //counters->latch_rs2ex.switching++; // latch from RS to execute pipeline
  //counters->scheduler_undiff.switching++; // undifferentiated core (i.e., control logics)
  counters->latch_rs2ex.read++; // latch from RS to execute pipeline
  counters->scheduler_undiff.read++; // undifferentiated core (i.e., control logics)
#endif
}

void inorder_t::frontend()
{
  if(instQ->is_available())
  {
    stats.Mop_count++;
    stats.uop_count++;
    stats.interval.Mop_count++;
    stats.interval.uop_count++;
      
    next_inst = new inst_t(core,++Mop_count,++uop_count);
    
    if(config.qsim == SPX_QSIM_LIB)
      Qsim_osd->run(core->core_id,1);
    else {}// SPX_QSIM_CLIENT
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

void inorder_t::fetch(inst_t *inst)
{
  instQ->push_back(inst);
}

void inorder_t::handle_cache_response(int temp, cache_request_t *cache_request)
{
  inst_t *inst = cache_request->inst;

  #ifdef SPX_DEBUG
  fprintf(stdout,"SPX_DEBUG (core %d) | %lu: cache_response uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence);
  #endif

  switch(cache_request->op_type)
  {
    case SPX_MEM_LD:
      threadQ->update(inst); // update all dependent insts
      RF->writeback(inst); // writeback result
      break;
    case SPX_MEM_ST:
      break;
    default:
      fprintf(stdout,"SPX_ERROR (core %d): strange cache response %d received\n",core->core_id,cache_request->op_type);
      exit(1);
  }

  stats.last_commit_cycle = core->clock_cycle;
  delete inst;
  delete cache_request;
}

