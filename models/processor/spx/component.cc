#include "component.h"
#include "core.h"

using namespace std;
using namespace manifold::spx;

instQ_t::instQ_t(pipeline_t *pl, int instQ_size, int IL1_bit_mask) :
size(instQ_size), occupancy(0), bit_mask(IL1_bit_mask), fetch_line(0), pipeline(pl)
{
  queue.reserve(size*6);
}
instQ_t::~instQ_t()
{
  queue.clear();
}

void instQ_t::push_back(inst_t *inst)
{
  inst->inflight = true;
  queue.push_back(inst);

  //if(!inst->prev_inst)
  if(inst->is_head)
    occupancy++;

#ifdef SPX_QSIM_DEBUG
  fprintf(stderr,"SPX_QSIM_DEBUG (core %d) | %lu: instQ.push_back (%d/%d) uop %lu Mop %lu  | ",inst->core->core_id,inst->core->clock_cycle,occupancy,size,inst->uop_sequence,inst->Mop_sequence);
  switch(inst->opcode)
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
  fprintf(stderr,"\n");
  uint64_t src_reg_mask = inst->src_reg;
  for(int i = 0; src_reg_mask > 0; src_reg_mask = src_reg_mask>>1, i++)
  {
    if(src_reg_mask&0x01)
      fprintf(stderr,"SPX_QSIM_DEBUG (core %d) | %lu:       src reg %d\n",inst->core->core_id,inst->core->clock_cycle,i);
  }
  uint8_t src_flag_mask = inst->src_flag;
  for(int i = 0; src_flag_mask >0; src_flag_mask = src_flag_mask>>1, i++)
  {
    if(src_flag_mask&0x01)
      fprintf(stderr,"SPX_QSIM_DEBUG (core %d) | %lu:       src flag %d\n",inst->core->core_id,inst->core->clock_cycle,i);
  }
  for(map<uint64_t,inst_t*>::iterator it = inst->src_dep.begin(); it != inst->src_dep.end(); it++)
    fprintf(stderr,"SPX_QSIM_DEBUG (core %d) | %lu:       mem dep 0x%x\n",inst->core->core_id,inst->core->clock_cycle,it->second->data.paddr);
  if(inst->memcode != SPX_MEM_NONE)
    fprintf(stderr,"SPX_QSIM_DEBUG (core %d) | %lu:       mem %s 0x%x\n",inst->core->core_id,inst->core->clock_cycle,(inst->memcode==SPX_MEM_ST)?"STORE":"LOAD",inst->data.paddr);
  uint64_t dest_reg_mask = inst->dest_reg;
  for(int i = 0; dest_reg_mask > 0; dest_reg_mask = dest_reg_mask>>1, i++)
  {
    if(dest_reg_mask&0x01)
      fprintf(stderr,"SPX_QSIM_DEBUG (core %d) | %lu:       dest reg %d\n",inst->core->core_id,inst->core->clock_cycle,i);
  }
  uint8_t dest_flag_mask = inst->dest_flag;
  for(int i = 0; dest_flag_mask > 0; dest_flag_mask = dest_flag_mask>>1, i++)
  {
    if(dest_flag_mask&0x01)
      fprintf(stderr,"SPX_QSIM_DEBUG (core %d) | %lu:       src flag %d\n",inst->core->core_id,inst->core->clock_cycle,i);
  }
#else
#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: instQ.push_back (%d/%d) uop %lu Mop %lu  | ",inst->core->core_id,inst->core->clock_cycle,occupancy,size,inst->uop_sequence,inst->Mop_sequence);
  switch(inst->opcode)
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
  fprintf(stderr,"\n");
  uint64_t src_reg_mask = inst->src_reg;
  for(int i = 0; src_reg_mask > 0; src_reg_mask = src_reg_mask>>1, i++)
  {
    if(src_reg_mask&0x01)
      fprintf(stderr,"SPX_DEBUG (core %d) | %lu:       src reg %d\n",inst->core->core_id,inst->core->clock_cycle,i);
  }
  uint8_t src_flag_mask = inst->src_flag;
  for(int i = 0; src_flag_mask >0; src_flag_mask = src_flag_mask>>1, i++)
  {
    if(src_flag_mask&0x01)
      fprintf(stderr,"SPX_DEBUG (core %d) | %lu:       src flag %d\n",inst->core->core_id,inst->core->clock_cycle,i);
  }
  for(map<uint64_t,inst_t*>::iterator it = inst->src_dep.begin(); it != inst->src_dep.end(); it++)
    fprintf(stderr,"SPX_DEBUG (core %d) | %lu:       mem dep 0x%x\n",inst->core->core_id,inst->core->clock_cycle,it->second->data.paddr);
  if(inst->memcode != SPX_MEM_NONE)
    fprintf(stderr,"SPX_DEBUG (core %d) | %lu:       mem %s 0x%x\n",inst->core->core_id,inst->core->clock_cycle,(inst->memcode==SPX_MEM_ST)?"STORE":"LOAD",inst->data.paddr);
  uint64_t dest_reg_mask = inst->dest_reg;
  for(int i = 0; dest_reg_mask > 0; dest_reg_mask = dest_reg_mask>>1, i++)
  {
    if(dest_reg_mask&0x01)
      fprintf(stderr,"SPX_DEBUG (core %d) | %lu:       dest reg %d\n",inst->core->core_id,inst->core->clock_cycle,i);
  }
  uint8_t dest_flag_mask = inst->dest_flag;
  for(int i = 0; dest_flag_mask > 0; dest_flag_mask = dest_flag_mask>>1, i++)
  {
    if(dest_flag_mask&0x01)
      fprintf(stderr,"SPX_DEBUG (core %d) | %lu:       src flag %d\n",inst->core->core_id,inst->core->clock_cycle,i);
  }
#endif
#endif

#ifdef LIBEI
  // Assume different Mops are separate instruction cache access.
  if(fetch_line != inst->Mop_sequence)
    pipeline->counters->inst_cache.read++; // inst_cache read
  pipeline->counters->inst_buffer.write++; // inst_buffer write

  // Branch predictors are read in parallel before insts are decoded.
  pipeline->counters->predictor_chooser.read++;
  pipeline->counters->global_predictor.read++;
  pipeline->counters->l1_local_predictor.read++;
  pipeline->counters->l2_local_predictor.read++;

  fetch_line = inst->Mop_sequence;
#endif
}

inst_t* instQ_t::get_front()
{
  vector<inst_t*>::iterator it = queue.begin();
  //if(it != queue.end())
  while(it != queue.end())
  {
    inst_t *inst = *it;
    return inst;
  }
  //else // Empty instQ
  return NULL;
}

void instQ_t::pop_front() // It must be called after get_front().
{
  inst_t *inst = *queue.begin();

#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: instQ.pop_front (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy-1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

  queue.erase(queue.begin());
  //if(!inst->next_inst) // Mop tail
  if(inst->is_tail) // Mop tail
    occupancy--;

#ifdef LIBEI
  // There isn't a decoder in the SPX model. Assume an instruction decode when instQ dequeue happens. inst_decoder is a combination logic, but assume its activation is muxed by instQ output so that the inst_decoder switches only when there is an actual inst coming out from the instQ.
  //pipeline->counters->inst_decoder.switching++;
  pipeline->counters->inst_decoder.read++;

  // Register operands decode
  uint64_t src_reg_mask = inst->src_reg;
  for(int i = 0; src_reg_mask > 0; src_reg_mask = src_reg_mask>>1, i++) {
    //pipeline->counters->operand_decoder.switching++;
    pipeline->counters->operand_decoder.read++;
  }
  uint64_t dest_reg_mask = inst->dest_reg;
  for(int i = 0; dest_reg_mask > 0; dest_reg_mask = dest_reg_mask>>1, i++) {
    //pipeline->counters->operand_decoder.switching++;
    pipeline->counters->operand_decoder.read++;
  }

  if(inst->is_long_inst) {
    //pipeline->counters->uop_sequencer.switching++;
    pipeline->counters->uop_sequencer.read++;
  }

  // There isn't a uop queue in the SPX model. Assume a uop enqueue/dequeue when instQ dequeue happens.
  pipeline->counters->uop_queue.write++;
  pipeline->counters->uop_queue.read++;
#endif
}

bool instQ_t::is_available()
{
  return (occupancy < size);
}





ROB_t::ROB_t(pipeline_t *pl, int ROB_size) :
size(ROB_size), occupancy(0), pipeline(pl)
{
  queue.reserve(size);
}

ROB_t::~ROB_t()
{
  while(queue.size()) { queue.erase(queue.begin()); }
  queue.clear();
}

void ROB_t::push_back(inst_t *inst)
{
#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: ROB.push_back (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy+1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

  queue.push_back(inst);
  occupancy++;

#ifdef LIBEI
  pipeline->counters->rob.write++;
#endif
}

inst_t* ROB_t::get_front()
{
  vector<inst_t*>::iterator it = queue.begin();
  if(it != queue.end())
  {
    inst_t *inst = *it;
    // Only Mop_head inst carries Mop_length information,
    // and can stall ROB until other uops are completed.
    if((inst->Mop_length == 0)&&!inst->inflight)
      return inst;
  }

  return NULL;
}

void ROB_t::update(inst_t *inst)
{
  inst->inflight = false; // Mark this inst completed.
  inst->Mop_head->Mop_length--; // Decrease the number of inflight insts in the same Mop

#ifdef SPX_QSIM_DEBUG
//#ifdef SPX_QSIM_DEBUG_
  fprintf(stderr,"SPX_QSIM_DEBUG (core %d): update uop %lu (Mop %lu)\n",inst->core->core_id,inst->uop_sequence,inst->Mop_sequence);
#endif

#ifdef LIBEI
  // This access should update only physical register (dest operand) part of ROB, so it's an overestimation of power.
  pipeline->counters->rob.write++;
#endif
}

void ROB_t::pop_front() // It must be called after get_front().
{
#ifdef SPX_DEBUG
  inst_t *inst = *queue.begin();
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: ROB.pop_front (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy-1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

  queue.erase(queue.begin());
  occupancy--;

#ifdef LIBEI
  pipeline->counters->rob.read++;
#endif
}

bool ROB_t::is_available()
{
#ifdef SPX_DEBUG
  if(!(occupancy < size))
  {
    inst_t *head_inst = *queue.begin();
    inst_t *tail_inst = *queue.rbegin();
    fprintf(stderr,"SPX_DEBUG (core %d) | %lu: ROB full | head uop %lu (Mop %lu) | tail uop %lu (Mop %lu)\n",head_inst->core->core_id,head_inst->core->clock_cycle,head_inst->uop_sequence,head_inst->Mop_sequence,tail_inst->uop_sequence,tail_inst->Mop_sequence);
  }
#endif

  return (occupancy < size);
}





RS_t::RS_t(pipeline_t *pl, int RS_size, int exec_port, std::vector<int> *FU_port_binding) :
size(RS_size), occupancy(0), port(exec_port), pipeline(pl)
{
  ready = new vector<inst_t*>[port];
  for(int i = 0; i < port; i++)
    ready[i].reserve(size);

  port_loads = new int[port];
  memset(port_loads,0,sizeof(int)*port);
  memset(FU_port,0,SPX_NUM_FU_TYPES*sizeof(uint16_t));

  // Copy FU port binding.
  for(int fu = 0; fu < SPX_NUM_FU_TYPES; fu++)
  {
    for(vector<int>::iterator it = FU_port_binding[fu].begin();
        it != FU_port_binding[fu].end(); it++)
      FU_port[fu] |= (0x01<<(*it));
  }
}
RS_t::~RS_t()
{
  for(int i = 0; i < port; i++)
    ready[port].clear();
  delete [] ready;
  delete [] port_loads;
}

void RS_t::push_back(inst_t *inst)
{
  // Inst has no dependency. Schedule in ready queue to execute.
  if(inst->src_dep.size() == 0)
  {
#ifdef SPX_QSIM_DEBUG_
//#ifdef SPX_QSIM_DEBUG
//#ifdef SPX_DEBUG
    fprintf(stderr,"SPX_DEBUG (core %d) | %lu: RS.ready[%d].push_back (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,inst->port,occupancy+1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

    ready[inst->port].push_back(inst);
  }
  else // Dependency exists. Wait in the pool.
  {
//#ifdef SPX_DEBUG
#ifdef SPX_QSIM_DEBUG_
//#ifdef SPX_QSIM_DEBUG
    fprintf(stderr,"SPX_DEBUG (core %d) | %lu: RS.pool.push_back (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy+1,size,inst->uop_sequence,inst->Mop_sequence);
#endif
  }

  occupancy++;

#ifdef LIBEI
  // Search an empty entry
  pipeline->counters->rs.search++;
  pipeline->counters->rs.write++;
#endif
}

inst_t* RS_t::get_front(int port)
{
  vector<inst_t*>::iterator it = ready[port].begin();
  if(it != ready[port].end())
    return *it;
  else
    return NULL;
}

void RS_t::pop_front(int port) // It must be called after get_front().
{

#ifdef SPX_QSIM_DEBUG_
//#ifdef SPX_QSIM_DEBUG
//#ifdef SPX_DEBUG
  inst_t *inst = *(ready[port].begin());
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: RS.pop_front (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy-1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

  ready[port].erase(ready[port].begin());
  occupancy--;
  port_loads[port]--;

#ifdef LIBEI
  // Entry of issuing inst is known. No search access.
  pipeline->counters->rs.read++;
#endif
}

void RS_t::update(inst_t *inst)
{
  for(map<uint64_t,inst_t*>::iterator it = inst->dest_dep.begin(); it != inst->dest_dep.end(); it++)
  {
    inst_t *dep_inst = it->second; // Dependent inst
    dep_inst->src_dep.erase(dep_inst->src_dep.find(inst->uop_sequence)); // Remove dependency.

    if((dep_inst->src_dep.size() == 0)&&(dep_inst->port > -1)) // Dependent inst is now ready to execute.
    {
//#ifdef SPX_DEBUG
#ifdef SPX_QSIM_DEBUG_
//#ifdef SPX_QSIM_DEBUG
      fprintf(stderr,"SPX_DEBUG (core %d) | %lu: RS.ready uop %lu (Mop %lu) free from uop %lu (Mop %lu)\n",dep_inst->core->core_id,dep_inst->core->clock_cycle,dep_inst->uop_sequence,dep_inst->Mop_sequence,inst->uop_sequence,inst->Mop_sequence);
#endif

      // Move dep_inst to ready queue.
      ready[dep_inst->port].push_back(dep_inst);

#ifdef LIBEI
      //pipeline->counters->issue_select.switching++;
      pipeline->counters->issue_select.read++;
#endif
    }

#ifdef LIBEI
    // This access should update only the source operands of dependent insts, so it's an overestimation of power.
    pipeline->counters->rs.search++;
    pipeline->counters->rs.write++;
#endif
  }

  inst->dest_dep.clear();
}

void RS_t::port_binding(inst_t *inst)
{
  int min_load = size+1;
  uint16_t port_mask = FU_port[inst->excode];
  for(int port = 0; port_mask > 0; port_mask = port_mask>>1, port++)
  {
    if((port_mask&0x01)&&(port_loads[port] < min_load))
    {
      min_load = port_loads[port];
      inst->port = port;
    }
  }

#ifdef SPX_QSIM_DEBUG_
//#ifdef SPX_QSIM_DEBUG
//#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: RS.port_binding %d to uop %lu (Mop %lu) | ",inst->core->core_id,inst->core->clock_cycle,inst->port,inst->uop_sequence,inst->Mop_sequence);
  switch(inst->excode)
  {
    case SPX_FU_INT: fprintf(stderr,"FU_INT"); break;
    case SPX_FU_MUL: fprintf(stderr,"FU_MUL"); break;
    case SPX_FU_FP: fprintf(stderr,"FU_FP"); break;
    case SPX_FU_MOV: fprintf(stderr,"FU_MOV"); break;
    case SPX_FU_BR: fprintf(stderr,"FU_BR"); break;
    case SPX_FU_LD: fprintf(stderr,"FU_LD"); break;
    case SPX_FU_ST: fprintf(stderr,"FU_ST"); break;
    default: break;
  }
  fprintf(stderr,"\n");
#endif
  port_loads[inst->port]++;
}

bool RS_t::is_available()
{
  return (occupancy < size);
}





RF_t::RF_t(pipeline_t *pl) :
pipeline(pl)
{
  for(int i = 0; i < QSIM_N_REGS; i++)
    regs[i] = NULL;
  for(int i = 0; i < SPX_N_FLAGS; i++)
    flags[i] = NULL;
  for(int i = 0; i < SPX_N_FREGS; i++)
    fregs[i] = NULL;
  fregs_stack_ptr = 0;
}

RF_t::~RF_t()
{
}

void RF_t::resolve_dependency(inst_t *inst)
{
  // reg src dependency
  uint64_t src_reg_mask = inst->src_reg;
  for(int i = 0; src_reg_mask > 0; src_reg_mask = src_reg_mask>>1, i++)
  {
#ifdef LIBEI
    if(src_reg_mask&0x01) // inst has source operand(s).
    {
      //pipeline->counters->dependency_check.switching++;
      pipeline->counters->dependency_check.read++;
      pipeline->counters->rat.search++;
      if(regs[i]) // Dependency exists.
      {
        pipeline->counters->rat.read++;
        if(!regs[i]->inflight) // Source inst has completed execution but not committed yet.
        {
          // The source operand is available in the physical register (ROB).
          pipeline->counters->rob.read++;
        }
      }
      else
        pipeline->counters->reg_int.read++;
    }
#endif
    if((src_reg_mask&0x01)&&regs[i]&&regs[i]->inflight) // Dependecy exists.
    {
//#ifdef SPX_DEBUG
#ifdef SPX_QSIM_DEBUG_
      fprintf(stderr,"SPX_DEBUG (core %d) | %lu: RF.check_dependency reg %d | uop %lu (Mop %lu) depends on uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,i,inst->uop_sequence,inst->Mop_sequence,regs[i]->uop_sequence,regs[i]->Mop_sequence);
#endif

      // Source inst knows which subsequent insts are dependent on its dest regs.
      regs[i]->dest_dep.insert(pair<int,inst_t*>(inst->uop_sequence,inst));
      // Depenedent inst know which precendent insts it is dependent on.
      inst->src_dep.insert(pair<int,inst_t*>(regs[i]->uop_sequence,regs[i]));
    }
  }

  // flag src dependency
  uint8_t src_flag_mask = inst->src_flag;
  for(int i = 0; src_flag_mask > 0; src_flag_mask = src_flag_mask>>1, i++)
  {
    if((src_flag_mask&0x01)&&flags[i]&&flags[i]->inflight) // dependecy exists
    {
//#ifdef SPX_DEBUG
#ifdef SPX_QSIM_DEBUG_
      fprintf(stderr,"SPX_DEBUG (core %d) | %lu: RF.check_dependency flag %d | uop %lu (Mop %lu) depends on uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,i,inst->uop_sequence,inst->Mop_sequence,flags[i]->uop_sequence,flags[i]->Mop_sequence);
#endif

      // Source inst knows which subsequent insts are dependent on its dest regs.
      flags[i]->dest_dep.insert(pair<int,inst_t*>(inst->uop_sequence,inst));
      // Depenedent inst know which precendent insts it is dependent on.
      inst->src_dep.insert(pair<int,inst_t*>(flags[i]->uop_sequence,flags[i]));
    }
  }

  // reg dest dependency
  uint64_t dest_reg_mask = inst->dest_reg;
  for(int i = 0; dest_reg_mask > 0; dest_reg_mask = dest_reg_mask>>1, i++)
  {
    if(dest_reg_mask&0x01)
    {
#ifdef LIBEI
      pipeline->counters->rat.write++;
      pipeline->counters->freelist.read++;
#endif
      regs[i] = inst; // Inst writes reg.
    }
  }

  // flag dest dependency
  uint8_t dest_flag_mask = inst->dest_flag;
  for(int i = 0; dest_flag_mask > 0; dest_flag_mask = dest_flag_mask>>1, i++)
  {
    if(dest_flag_mask&0x01)
      flags[i] = inst; // Inst writes flag.
  }

  // freg dependency
  if(inst->opcode >= QSIM_INST_FPBASIC)
  {
    switch(inst->memcode)
    {
      case SPX_MEM_LD: // FLD
      {
#ifdef LIBEI
        pipeline->counters->rat.write++;
        pipeline->counters->freelist.read++;
#endif
        fregs_stack_ptr = (++fregs_stack_ptr)%(int)SPX_NUM_FP_REGS;
        fregs[fregs_stack_ptr] = inst;
        inst->dest_freg |= (0x01<<fregs_stack_ptr); // ST0
        break;
      }
      case SPX_MEM_ST: // FST
      {
#ifdef LIBEI
        //pipeline->counters->dependency_check.switching++;
        pipeline->counters->dependency_check.read++;
        pipeline->counters->rat.search++;
        if(fregs[fregs_stack_ptr]) // Dependency exists.
        {
          pipeline->counters->rat.read++;
          if(!fregs[fregs_stack_ptr]->inflight) // Source inst has completed execution but not committed yet.
          {
            // The source operand is available in the physical register (ROB).
            pipeline->counters->rob.read++;
          }
        }
        else
          pipeline->counters->reg_fp.read++;
#endif
        if(fregs[fregs_stack_ptr]&&fregs[fregs_stack_ptr]->inflight)
        {
          fregs[fregs_stack_ptr]->dest_dep.insert(pair<uint64_t,inst_t*>(inst->uop_sequence,inst));
          inst->src_dep.insert(pair<uint64_t,inst_t*>(fregs[fregs_stack_ptr]->uop_sequence,fregs[fregs_stack_ptr]));
        }
        fregs[fregs_stack_ptr] = NULL;
        inst->src_freg |= (0x01<<fregs_stack_ptr); // ST0
        if(--fregs_stack_ptr < 0)
          fregs_stack_ptr = SPX_NUM_FP_REGS-1;
        break;
      }
      default: // Regular FP op
      {
        // ST0 src dependency
#ifdef LIBEI
        //pipeline->counters->dependency_check.switching++;
        pipeline->counters->dependency_check.read++;
        pipeline->counters->rat.search++;
        if(fregs[fregs_stack_ptr]) // Dependency exists.
        {
          pipeline->counters->rat.read++;
          if(!fregs[fregs_stack_ptr]->inflight) // Source inst has completed execution but not committed yet.
          {
            // The source operand is available in the physical register (ROB).
            pipeline->counters->rob.read++;
          }
        }
        else
          pipeline->counters->reg_fp.read++;

        // ST0 is also a dest reg
        pipeline->counters->rat.write++;
        pipeline->counters->freelist.read++;
#endif

        if(fregs[fregs_stack_ptr]&&fregs[fregs_stack_ptr]->inflight)
        {
          fregs[fregs_stack_ptr]->dest_dep.insert(pair<uint64_t,inst_t*>(inst->uop_sequence,inst));
          inst->src_dep.insert(pair<uint64_t,inst_t*>(fregs[fregs_stack_ptr]->uop_sequence,fregs[fregs_stack_ptr]));
        }
        inst->src_freg |= (0x01<<fregs_stack_ptr); // ST0

        // ST1 src dependency
        int ST1 = fregs_stack_ptr-1;
        if(ST1 < 0)
          ST1 = SPX_NUM_FP_REGS-1;
#ifdef LIBEI
        //pipeline->counters->dependency_check.switching++;
        pipeline->counters->dependency_check.read++;
        pipeline->counters->rat.search++;
        if(fregs[ST1]) // Dependency exists
        {
          pipeline->counters->rat.read++;
          if(!fregs[ST1]->inflight) // Source inst has completed execution but not committed yet.
          {
            // The source operand is available in the physical register (ROB)
            pipeline->counters->rob.read++;
          }
        }
        else
          pipeline->counters->reg_fp.read++;
#endif
        if(fregs[ST1]&&fregs[ST1]->inflight)
        {
          fregs[ST1]->dest_dep.insert(pair<uint64_t,inst_t*>(inst->uop_sequence,inst));
          inst->src_dep.insert(pair<uint64_t,inst_t*>(fregs[ST1]->uop_sequence,fregs[ST1]));
        }
        inst->src_freg |= (0x01<<ST1); // ST0

        fregs[fregs_stack_ptr] = inst; // This inst is the latest producer of ST0.
        inst->dest_freg |= (0x01<<fregs_stack_ptr); // ST0
        break;
      }
    }
  }
}

void RF_t::writeback(inst_t *inst)
{
  // Inst writes reg.
  uint64_t dest_reg_mask = inst->dest_reg;
  for(int i = 0; dest_reg_mask > 0; dest_reg_mask = dest_reg_mask>>1, i++)
  {
#ifdef LIBEI
    if(dest_reg_mask&0x01)
    {
      pipeline->counters->rat.read++;
      pipeline->counters->freelist.write++;
      pipeline->counters->reg_int.write++;
    }
#endif
    // this reg is now free, no dependency exists
    if((dest_reg_mask&0x01)&&(regs[i] == inst))
      regs[i] = NULL;
  }

  // Inst writes flag.
  uint8_t dest_flag_mask = inst->dest_flag;
  for(int i = 0; dest_flag_mask > 0; dest_flag_mask = dest_flag_mask>>1, i++)
  {
    // this flag is now free, no dependency exists
    if((dest_flag_mask&0x01)&&(flags[i] == inst))
      flags[i] = NULL;
  }

  // Inst writes freg.
  uint8_t dest_freg_mask = inst->dest_freg;
  for(int i = 0; dest_freg_mask > 0; dest_freg_mask = dest_freg_mask>>1, i++)
  {
#ifdef LIBEI
    if(dest_freg_mask&0x01)
    {
      pipeline->counters->rat.read++;
      pipeline->counters->freelist.write++;
      pipeline->counters->reg_fp.write++;
    }
#endif
    // This fp reg is now free, no dependency exits.
    if((dest_freg_mask&0x01)&&(fregs[i] == inst))
      fregs[i] = NULL;
  }
}




FU_t::FU_t(int FU_delay, int FU_issue_rate) :
delay(FU_delay), issue_rate(FU_issue_rate)
{
  pipeline.reserve(FU_delay);
}

FU_t::~FU_t()
{
  pipeline.clear();
}

void FU_t::push_back(inst_t *inst)
{
  inst->completed_cycle = inst->core->clock_cycle+delay;
  pipeline.push_back(inst);
}

inst_t* FU_t::get_front()
{
  vector<inst_t*>::iterator it = pipeline.begin();
  if(it != pipeline.end())
  {
     inst_t *inst = (*it);
     if(inst->completed_cycle <= inst->core->clock_cycle)
       return inst;
     else // Inst still in execution
       return NULL;
  }
  else // FU is empty
    return NULL;
}

void FU_t::pop_front()
{
  if(pipeline.begin() != pipeline.end())
    pipeline.erase(pipeline.begin());
}

bool FU_t::is_available()
{
  vector<inst_t*>::reverse_iterator it = pipeline.rbegin();
  if(it != pipeline.rend())
  {
    inst_t *inst = (*it);
    return ((inst->completed_cycle-delay) <= (inst->core->clock_cycle - issue_rate));
  }
  else
    return true;
}

void FU_t::stall()
{
  for(vector<inst_t*>::iterator it = pipeline.begin();
      it != pipeline.end(); it++)
    (*it)->completed_cycle++;
}





EX_t::EX_t(pipeline_t *pl, int FU_port, vector<int> *FU_port_binding, int *FU_delay, int *FU_issue_rate) :
num_FUs(0),
pipeline(pl)
{
  for(int fu = 0; fu < SPX_NUM_FU_TYPES; fu++)
  {
    for(vector<int>::iterator it = FU_port_binding[fu].begin();
        it != FU_port_binding[fu].end(); it++)
    {
      if(FU_port == (*it)) // An FU binds to this port
      {
        FUs[fu] = new FU_t(FU_delay[fu],FU_issue_rate[fu]);
        FU_index[num_FUs++] = fu;
      }
    }
  }
}

EX_t::~EX_t()
{
  for(int fu = 0; fu < SPX_NUM_FU_TYPES; fu++)
    delete FUs[fu];
}

void EX_t::push_back(inst_t *inst)
{
#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: EX[%d].push_back uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,inst->port,inst->uop_sequence,inst->Mop_sequence);
#endif
  FUs[inst->excode]->push_back(inst);
}

inst_t* EX_t::get_front()
{
  inst_t *oldest_inst = NULL;

  for(int fu = 0; fu < num_FUs; fu++)
  {
    inst_t *inst = FUs[FU_index[fu]]->get_front();

    if(inst)
    {
      if(!oldest_inst)
        oldest_inst = inst;
      else if(inst->uop_sequence < oldest_inst->uop_sequence)
      {
        FUs[oldest_inst->excode]->stall();
        oldest_inst = inst;
      }
    }
  }
  return oldest_inst;
}

void EX_t::pop_front(int excode)
{
#ifdef LIBEI
  inst_t *inst = FUs[excode]->get_front();

 switch(inst->opcode)
  {
    case QSIM_INST_INTMUL:
    case QSIM_INST_FPMUL:
    {
      pipeline->counters->mul.read++;
      if(inst->dest_reg) {
        pipeline->counters->fp_bypass.read++;
      }
      break;
    }
    case QSIM_INST_INTDIV:
    case QSIM_INST_FPDIV:
    case QSIM_INST_FPBASIC:
    {
      pipeline->counters->fpu.read++;
      if(inst->dest_freg) {
        pipeline->counters->fp_bypass.read++;
      }
      break;
    }
    default:
    {
      pipeline->counters->alu.read++;
      if(inst->dest_reg) {
        pipeline->counters->int_bypass.read++;
      }
      break;
    }
  }
#if 0
  switch(inst->excode)
  {
    case SPX_FU_FP:
    {
      //pipeline->counters->fpu.switching++;
      pipeline->counters->fpu.read++;
      if(inst->dest_freg) {
        //pipeline->counters->fp_bypass.switching++;
        pipeline->counters->fp_bypass.read++;
      }
      break;
    }
    case SPX_FU_MUL:
    {
      //pipeline->counters->mul.switching++;
      pipeline->counters->mul.read++;
      if(inst->dest_reg) {
        //pipeline->counters->int_bypass.switching++;
        pipeline->counters->fp_bypass.read++;
      }
      break;
    }
    default:
    {
      //pipeline->counters->alu.switching++;
      pipeline->counters->alu.read++;
      if(inst->dest_reg) {
        //pipeline->counters->int_bypass.switching++;
        pipeline->counters->int_bypass.read++;
      }
      break;
    }
  }
#endif
#endif

  FUs[excode]->pop_front();
}

bool EX_t::is_available(int excode)
{
  return FUs[excode]->is_available();
}





LDQ_t::LDQ_t(pipeline_t *pl, int LDQ_size) :
size(LDQ_size), occupancy(0), pipeline(pl)
{
  outgoing.reserve(size);
}

LDQ_t::~LDQ_t()
{
  outgoing.clear();
}

void LDQ_t::push_back(inst_t *inst)
{
#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: LDQ.push_back (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy+1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

  occupancy++;

#ifdef LIBEI
  // Search an empty entry
  pipeline->counters->ldq.search++;
  pipeline->counters->ldq.write++;
#endif
}

void LDQ_t::pop(inst_t *inst)
{
#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: LDQ.pop (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy-1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

  occupancy--;

#ifdef LIBEI
  pipeline->counters->ldq.read++;
#endif
}

void LDQ_t::schedule(inst_t *inst)
{
  if(inst->mem_disamb_status == SPX_MEM_DISAMB_NONE)
  {
    // No memory disambiguation, schedule memory request.
    outgoing.push_back(inst);
  }
  else if(inst->mem_disamb_status == SPX_MEM_DISAMB_CLEAR)
  {
    // Memory disambiguation exists but is already cleared.
    // Store was executed already, so data should have been forwarded.
    // Treat this load as if a memory request is immediately returned.
#ifdef SPX_DEBUG
    fprintf(stderr,"SPX_DEBUG (core %d) | %lu: uop %lu (Mop %lu) bypasses handle_cache() by store forwarding\n",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence);
#endif

    cache_request_t *cache_request = new cache_request_t(inst,inst->core->core_id,inst->core->node_id,inst->data.paddr,SPX_MEM_LD);
    pipeline->handle_cache_response(0,cache_request);

#ifdef LIBEI
    // Data forwards from STQ to LDQ
    pipeline->counters->stq.read++; 
    pipeline->counters->ldq.write++;
#endif
  }
  else
  {
    // This load has memory order violation.
    // Wait for store until its address is calculated so that data can be forwarded.
#ifdef SPX_DEBUG
    fprintf(stderr,"\n\n\n\n\nSPX_DEBUG (core %d) | %lu: uop %lu (Mop %lu) is blocked by memory disambiguation\n",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence);
#endif

    inst->mem_disamb_status = SPX_MEM_DISAMB_WAIT;
  }

#ifdef LIBEI
  // This access should update the address field of the entry. It's an overestimation of power.
  pipeline->counters->ldq.write++;
#endif
}

void LDQ_t::handle_cache()
{
  vector<inst_t*>::iterator it = outgoing.begin();
  if(it != outgoing.end())
  {
    inst_t *inst = *it;

//#ifdef SPX_DEBUG
//#ifdef SPX_QSIM_DEBUG_
#if 0
    fprintf(stderr,"SPX_DEBUG (core %d) | %lu: LDQ.handle_cache uop %lu (Mop %lu) (Addr: %lx)\n",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence,inst->data.paddr);
#endif

    // Send cache request.
    cache_request_t *cache_request = new cache_request_t(inst,inst->core->core_id,inst->core->node_id,inst->data.paddr,SPX_MEM_LD);
    inst->core->Send(inst->core->OUT_TO_CACHE,cache_request);

    outgoing.erase(it);

#ifdef LIBEI
    pipeline->counters->ldq.read++;
#endif
  }
}

bool LDQ_t::is_available()
{
  return (occupancy < size);
}





STQ_t::STQ_t(pipeline_t *pl, int STQ_size) :
size(STQ_size), occupancy(0), pipeline(pl)
{
  outgoing.reserve(size);
}

STQ_t::~STQ_t()
{
  outgoing.clear();
}

void STQ_t::push_back(inst_t *inst)
{
#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: STQ.push_back (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy+1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

  occupancy++;

  // Memory disambiguation map for store forwarding.
  // Since store inst is not yet executed, it is like a perfect prediction for memory disambiguation.
  map<uint64_t,inst_t*>::iterator it = mem_disamb.find(inst->data.paddr);
  if(it != mem_disamb.end())
    it->second = inst; // Replace the latest producer of this address.
  else
    mem_disamb.insert(pair<uint64_t,inst_t*>(inst->data.paddr,inst));

#ifdef LIBEI
  // Search an empty entry
  pipeline->counters->stq.search++;
  pipeline->counters->stq.write++;
#endif
}

void STQ_t::pop(inst_t *inst)
{
#ifdef SPX_DEBUG
  fprintf(stderr,"SPX_DEBUG (core %d) | %lu: STQ.pop (%d/%d) uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,occupancy-1,size,inst->uop_sequence,inst->Mop_sequence);
#endif

  occupancy--;

#ifdef LIBEI
  pipeline->counters->stq.read++;
#endif
}

void STQ_t::schedule(inst_t *inst)
{
  outgoing.push_back(inst);

#ifdef LIBEI
  // This access should update the address/data field of the entry. It's an overestimation of power.
  pipeline->counters->stq.write++;
#endif
}

void STQ_t::handle_cache()
{
  vector<inst_t*>::iterator outgoing_it = outgoing.begin();
  if(outgoing_it != outgoing.end())
  {
    inst_t *inst = *outgoing_it;

//#ifdef SPX_DEBUG
//#ifdef SPX_QSIM_DEBUG_
#if 0
    fprintf(stderr,"SPX_DEBUG (core %d) | %lu: STQ.handle_cache uop %lu (Mop %lu) (Addr: %lx)\n",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence,inst->data.paddr);
#endif

    // Since store is written back when the ROB commits, clear memory disambiguation map.
    map<uint64_t,inst_t*>::iterator mem_disamb_it = mem_disamb.find(inst->data.paddr);
    if((mem_disamb_it != mem_disamb.end())&&(mem_disamb_it->second == inst))
      mem_disamb.erase(mem_disamb_it);

    // Send cache request.
    cache_request_t *cache_request = new cache_request_t(inst,inst->core->core_id,inst->core->node_id,inst->data.paddr,SPX_MEM_ST);
    inst->core->Send(inst->core->OUT_TO_CACHE,cache_request);

    outgoing.erase(outgoing_it);
    
    // Remove from STQ
    pop(inst);
    delete inst;
#ifdef LIBEI
    pipeline->counters->stq.read++;
#endif
  }
}

bool STQ_t::is_available()
{
  return (occupancy < size);
}

void STQ_t::store_forward(inst_t *inst)
{
  // If there is any loads after this store, forward data.
  for(vector<inst_t*>::iterator it = inst->mem_disamb.begin(); it != inst->mem_disamb.end(); it++)
  {
    inst_t *ld_inst = (*it);
    // There is a load waiting on this store to forward.
    if(ld_inst->mem_disamb_status == SPX_MEM_DISAMB_WAIT)
    {
#ifdef SPX_DEBUG
      fprintf(stderr,"\n\n\n\n\nSPX_DEBUG (core %d) | %lu: uop %lu (Mop %lu) does store forwarding to uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence,ld_inst->uop_sequence,ld_inst->Mop_sequence);
#endif

      // Handle pending load as if a memory request is immediately returned.
      cache_request_t *cache_request = new cache_request_t(ld_inst,ld_inst->core->core_id,ld_inst->core->node_id,ld_inst->data.paddr,SPX_MEM_LD);
      pipeline->handle_cache_response(0,cache_request);

#ifdef LIBEI
    // Data forwards from STQ to LDQ
    pipeline->counters->stq.read++; 
    pipeline->counters->ldq.write++;
#endif
    }
    else // This load is not yet executed; handled by LDQ_t::schedule.
      ld_inst->mem_disamb_status = SPX_MEM_DISAMB_CLEAR;
  }
}

void STQ_t::mem_disamb_check(inst_t *inst)
{
  // Check if memory disambiguation exists; perfect prediction for memory disambiguation.
  map<uint64_t,inst_t*>::iterator it = mem_disamb.find(inst->data.paddr);
  if(it != mem_disamb.end())
  {
#ifdef SPX_DEBUG
    fprintf(stderr,"\n\n\n\n\nSPX_DEBUG (core %d) | %lu: uop %lu (Mop %lu) has memory disambiguation with uop %lu (Mop %lu)\n",inst->core->core_id,inst->core->clock_cycle,inst->uop_sequence,inst->Mop_sequence,it->second->uop_sequence,it->second->Mop_sequence);
#endif

    if(it->second->inflight) // Store is not yet executed, mark load inst as memory disambiguation detected.
    {
      it->second->mem_disamb.push_back(inst);
      inst->mem_disamb_status = SPX_MEM_DISAMB_DETECTED;
    }
    else // Store is executed, mark load inst as cleared.
      inst->mem_disamb_status = SPX_MEM_DISAMB_CLEAR;
  }

#ifdef LIBEI
  // Search STQ entries and compare tags
  pipeline->counters->stq.search++;
#endif
}

