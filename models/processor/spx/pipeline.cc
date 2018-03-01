#include "pipeline.h"
#include "core.h"

#include "distorm.h"

using namespace std;
using namespace manifold;
using namespace manifold::spx;

pipeline_t::pipeline_t() :
next_inst(NULL),
idle_loop(false),
Qsim_cb_status(SPX_QSIM_INIT_CB),
Mop_count(0),
finished(0),
runout_flag(0),
trans_flag(0),
uop_count(0)
//EI_client(NULL)
{
}

void pipeline_t::Qsim_inst_cb(int core_id, uint64_t vaddr, uint64_t paddr, uint8_t len, const uint8_t *bytes, enum inst_type type)
{

  if(!next_inst||idle_loop)
    return ;

#if 0
  if(core_id == 15) {

    _DecodedInst inst[15];
    unsigned int shouldBeOne;
    distorm_decode(0, bytes, len, Decode32Bits, inst, 15, &shouldBeOne);

    //cerr << "core " << core_id << " [idle]:";
    cerr << "@ " << manifold::kernel::Clock::Master().NowTicks() << " " << 
      ((Qsim_osd->get_prot(core_id) == Qsim::OSDomain::PROT_USER)?"USR":"KRN")
          << " " << (Qsim_osd->idle(core_id)?"[IDLE]":"[ACTIVE]");

    if (shouldBeOne != 1) cerr << " [Decoding Error]\n";
    else cerr << " " << inst[0].mnemonic.p << ' ' << inst[0].operands.p << ' ';

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

    fprintf(stderr,"\n\n"); 

    return;
  }
#endif
  
  #if 0
  fprintf(stderr,"SPX_QSIM_DEBUG(%d) inst_cb (core %d): uop %lu (Mop %lu) (v:%lx, p:%lx) | ",Qsim_cb_status,core_id,next_inst->uop_sequence,next_inst->Mop_sequence, vaddr, paddr);
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

  _DecodedInst inst[15];
  unsigned int shouldBeOne;
  distorm_decode(0, bytes, len, Decode32Bits, inst, 15, &shouldBeOne);

  if (shouldBeOne != 1) cerr << "\t[Decoding Error]\n";
  else cerr << "\t" << inst[0].mnemonic.p << ' ' << inst[0].operands.p << endl;


  fprintf(stderr,"\n");
  #endif
  
  next_inst->opcode = type;
  next_inst->memcode = SPX_MEM_NONE;
  Qsim_cb_status = SPX_QSIM_INST_CB;
}

int pipeline_t::Qsim_mem_cb(int core_id, uint64_t vaddr, uint64_t paddr, uint8_t size, int type)
{
  if(!next_inst||idle_loop)
    return 0;

  #ifdef SPX_QSIM_DEBUG
  fprintf(stderr,"SPX_QSIM_DEBUG(%d) mem_cb (core %d): uop %lu (Mop %lu) | ",Qsim_cb_status,core_id,next_inst->uop_sequence,next_inst->Mop_sequence);
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
  fprintf(stderr,"\n");
  #endif
  
  if(type) // store
  {
    // there can't be mem_cb after mem_cb or dest_reg/flag_cb
    if(Qsim_cb_status > SPX_QSIM_MEM_CB)
    {
      Qsim_post_cb(next_inst);
      fetch(next_inst);
      
      //stats.uop_count++;
      stats.interval.uop_count++;
      //stats.interval.eff_uop_count++;
      next_inst = new inst_t(next_inst,core,Mop_count,++uop_count,SPX_MEM_ST);
      
      #ifdef SPX_QSIM_DEBUG
      fprintf(stderr,"SPX_QSIM_DEBUG mem_cb (core %d): uop %lu (Mop %lu) | ",core_id,next_inst->uop_sequence,next_inst->Mop_sequence);
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
      fprintf(stderr,"\n");
      #endif
    }
    else
      next_inst->memcode = SPX_MEM_ST;
    
    #ifdef SPX_QSIM_DEBUG
    fprintf(stderr,"SPX_QSIM_DEBUG (core %d):      mem STORE | 0x%x\n",core_id,paddr);
    #endif
    
    next_inst->data.vaddr = vaddr;
    next_inst->data.paddr = paddr;
    Qsim_cb_status = SPX_QSIM_ST_CB;
  }
  else // load
  {
    // ther can't be mem_cb after mem_cb or dest_reg/flag_cb
    if(Qsim_cb_status > SPX_QSIM_MEM_CB)
    {
      Qsim_post_cb(next_inst);
      fetch(next_inst);
      
      //stats.uop_count++;
      stats.interval.uop_count++;
      //stats.interval.eff_uop_count++;
      next_inst = new inst_t(next_inst,core,Mop_count,++uop_count,SPX_MEM_LD);
      
      #ifdef SPX_QSIM_DEBUG
      fprintf(stderr,"SPX_QSIM_DEBUG mem_cb (core %d): uop %lu (Mop %lu) | ",core_id,next_inst->uop_sequence,next_inst->Mop_sequence);
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
      fprintf(stderr,"\n");
      #endif
    }
    else
      next_inst->memcode = SPX_MEM_LD;
    
    #ifdef SPX_QSIM_DEBUG
    fprintf(stderr,"SPX_QSIM_DEBUG (core %d):      mem LOAD | 0x%x\n",core_id,paddr);
    #endif
    
    next_inst->data.vaddr = vaddr;
    next_inst->data.paddr = paddr;
    Qsim_cb_status = SPX_QSIM_LD_CB;
  }

  return 0;
}

void pipeline_t::Qsim_reg_cb(int core_id, int reg, uint8_t size, int type)
{
  if(!next_inst||idle_loop)
    return;

  #ifdef SPX_QSIM_DEBUG_
  fprintf(stderr,"SPX_QSIM_DEBUG(%d) reg_cb (core %d): uop %lu (Mop %lu) | ",Qsim_cb_status,core_id,next_inst->uop_sequence,next_inst->Mop_sequence);
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
  fprintf(stderr,"\n");
  #endif
  
  if(type) // dest
  {
    if(size > 0) // regs
    {
      // there can be multiple dest regs (e.g., div/mul) - don't use >= sign
      if(Qsim_cb_status > SPX_QSIM_DEST_REG_CB)
      {
        Qsim_post_cb(next_inst);
        fetch(next_inst);
        
        //stats.uop_count++;
        stats.interval.uop_count++;
        //stats.interval.eff_uop_count++;
        next_inst = new inst_t(next_inst,core,Mop_count,++uop_count,SPX_MEM_NONE);
        
        #ifdef SPX_QSIM_DEBUG_
        fprintf(stderr,"SPX_QSIM_DEBUG reg_cb (core %d): uop %lu (Mop %lu) | ",core_id,next_inst->uop_sequence,next_inst->Mop_sequence);
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
        fprintf(stderr,"\n");
        #endif
      }
      else if(Qsim_cb_status == SPX_MEM_ST)
        next_inst->split_store = true;
      else if(Qsim_cb_status == SPX_MEM_LD)
        next_inst->split_load = true;
     
      
      #ifdef SPX_QSIM_DEBUG_
      fprintf(stderr,"SPX_QSIM_DEBUG (core %d):      reg dest %d\n",core_id,reg);
      #endif

      next_inst->dest_reg |= (0x01<<reg); // bit mask the dest reg position
      Qsim_cb_status = SPX_QSIM_DEST_REG_CB;
    }
    else // flags
    {
      if(Qsim_cb_status >= SPX_QSIM_DEST_FLAG_CB)
      {
        Qsim_post_cb(next_inst);
        fetch(next_inst);
        
        //stats.uop_count++;
        stats.interval.uop_count++;
	//stats.interval.eff_uop_count++;
        next_inst = new inst_t(next_inst,core,Mop_count,++uop_count,SPX_MEM_NONE);
        
        #ifdef SPX_QSIM_DEBUG_
        fprintf(stderr,"SPX_QSIM_DEBUG reg_cb (core %d): uop %lu (Mop %lu) | ",core_id,next_inst->uop_sequence,next_inst->Mop_sequence);
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
        fprintf(stderr,"\n");
        #endif
      }

      #ifdef SPX_QSIM_DEBUG_
      fprintf(stderr,"SPX_QSIM_DEBUG (core %d):      flag dest 0x%x\n",core_id,reg);
      #endif
     
      next_inst->dest_flag = (0x3f&reg); // bit maks the dest flags position
      Qsim_cb_status = SPX_QSIM_DEST_FLAG_CB;
    }
  }
  else // src
  {
    if(size > 0) // regs
    {
      // there can be multiple src regs - don't use >= sign
      if(Qsim_cb_status > SPX_QSIM_SRC_REG_CB)
      {
        Qsim_post_cb(next_inst);
        fetch(next_inst);
        
        //stats.uop_count++;
        stats.interval.uop_count++;
	//stats.interval.eff_uop_count++;
        next_inst = new inst_t(next_inst,core,Mop_count,++uop_count,SPX_MEM_NONE);

        #ifdef SPX_QSIM_DEBUG_
        fprintf(stderr,"SPX_QSIM_DEBUG reg_cb (core %d): uop %lu (Mop %lu) | ",core_id,next_inst->uop_sequence,next_inst->Mop_sequence);
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
        fprintf(stderr,"\n");
        #endif
      }
      
      #ifdef SPX_QSIM_DEBUG_
      fprintf(stderr,"SPX_QSIM_DEBUG (core %d):      reg src %d\n",core_id,reg);
      #endif
      
      next_inst->src_reg |= (0x01<<reg); // bit mask the src reg position
      Qsim_cb_status = SPX_QSIM_SRC_REG_CB;
    }
    else // flags
    {
      if(Qsim_cb_status >= SPX_QSIM_SRC_FLAG_CB)
      {
        Qsim_post_cb(next_inst);
        fetch(next_inst);
        
        //stats.uop_count++;
        stats.interval.uop_count++;
	//stats.interval.eff_uop_count++;
        next_inst = new inst_t(next_inst,core,Mop_count,++uop_count,SPX_MEM_NONE);
        
        #ifdef SPX_QSIM_DEBUG_
        fprintf(stderr,"SPX_QSIM_DEBUG reg_cb (core %d): uop %lu (Mop %lu) | ",core_id,next_inst->uop_sequence,next_inst->Mop_sequence);
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
        fprintf(stderr,"\n");
        #endif
      }
      
      #ifdef SPX_QSIM_DEBUG_
      fprintf(stderr,"SPX_QSIM_DEBUG (core %d):      flag src 0x%x\n",core_id,reg);
      #endif
      
      next_inst->src_flag = (0x3f&reg); // bit mask the src flags position
      Qsim_cb_status = SPX_QSIM_SRC_FLAG_CB;
    }
  }
}

void pipeline_t::Qsim_post_cb(inst_t* inst)
{
  switch(inst->opcode)
  {
    case QSIM_INST_NULL: // do not split mov
      inst->excode = SPX_FU_MOV;
      inst->split_store = false;
      inst->split_load = false;
      break;
    case QSIM_INST_INTBASIC:
      inst->excode = SPX_FU_INT;
      break;
    case QSIM_INST_INTMUL:
      inst->excode = SPX_FU_MUL;
      break;
    case QSIM_INST_INTDIV:
      inst->excode = SPX_FU_MUL;
      break;
    case QSIM_INST_STACK: // do not split stack
      inst->excode = SPX_FU_INT;
      inst->split_store = false;
      inst->split_load = false;
      break;
    case QSIM_INST_BR: // do not split branch
      inst->excode = SPX_FU_BR;
      inst->split_store = false;
      inst->split_load = false;
      break;
    case QSIM_INST_CALL: // do not split call
      inst->excode = SPX_FU_INT;
      inst->split_store = false;
      inst->split_load = false;
      break;
    case QSIM_INST_RET: // do not split return
      inst->excode = SPX_FU_INT;
      inst->split_store = false;
      inst->split_load = false;
      break;
    case QSIM_INST_TRAP: // do not split trap
      inst->excode = SPX_FU_INT;
      inst->split_store = false;
      inst->split_load = false;
      break;
    case QSIM_INST_FPBASIC: 
      inst->excode = SPX_FU_FP;

      if(inst->memcode == SPX_MEM_LD)
        inst->split_load = true;
      else if(inst->memcode == SPX_MEM_ST)
        inst->split_store = true;

      break;
    case QSIM_INST_FPMUL:
      inst->excode = SPX_FU_MUL;

      if(inst->memcode == SPX_MEM_LD)
        inst->split_load = true;
      else if(inst->memcode == SPX_MEM_ST)
        inst->split_store = true;

      break;
    case QSIM_INST_FPDIV:
      inst->excode = SPX_FU_FP;

      if(inst->memcode == SPX_MEM_LD)
        inst->split_load = true;
      else if(inst->memcode == SPX_MEM_ST)
        inst->split_store = true;

      break;
    default:
      inst->excode = SPX_FU_INT;
      break;
  }

  switch(inst->memcode)
  {
    case SPX_MEM_LD:
      if(inst->split_load)
      {
#ifdef SPX_QSIM_DEBUG
  fprintf(stderr,"SPX_QSIM_DEBUG (core %d): uop %lu (Mop %lu) LS| ",inst->core->core_id,inst->uop_sequence,inst->Mop_sequence);
#endif
        // create another inst that computes with a memory operand
        next_inst = new inst_t(next_inst,core,Mop_count,++uop_count,SPX_MEM_NONE);
        next_inst->excode = inst->excode;
        next_inst->src_dep.insert(pair<uint64_t,inst_t*>(inst->uop_sequence,inst));
        next_inst->data.vaddr = 0;
        next_inst->data.paddr = 0;

        // previous inst is a load inst
        inst->excode = SPX_FU_LD;
        inst->dest_dep.insert(pair<uint64_t,inst_t*>(next_inst->uop_sequence,next_inst));
        fetch(inst);
      }
      else
        inst->excode = SPX_FU_LD;
      break;
    case SPX_MEM_ST:
      if(inst->split_store)
      {
#ifdef SPX_QSIM_DEBUG
  fprintf(stderr,"SPX_QSIM_DEBUG (core %d): uop %lu (Mop %lu) SS| ",inst->core->core_id,inst->uop_sequence,inst->Mop_sequence);
#endif
        // create another inst that stores the result
        next_inst = new inst_t(next_inst,core,Mop_count,++uop_count,SPX_MEM_ST);
        next_inst->excode = SPX_FU_ST;
        next_inst->src_dep.insert(pair<uint64_t,inst_t*>(inst->uop_sequence,inst));
        next_inst->src_reg |= inst->dest_reg; // this store depends on the result of a previous inst
        // previous inst is a compute inst
        inst->memcode = SPX_MEM_NONE;
        inst->dest_dep.insert(pair<uint64_t,inst_t*>(next_inst->uop_sequence,next_inst));
        inst->data.vaddr = 0;
        inst->data.paddr = 0;
        fetch(inst);
      }
      else
        inst->excode = SPX_FU_ST;
      break;
    default:
      break;
  }
}

int pipeline_t::Qsim_app_end_cb(int core_id)
{
  finished = 1;
  return 1;
}
void pipeline_t::handle_long_inst(inst_t *inst)
{
  inst_t *uop = inst->Mop_head;
  while(uop)
  {
    uop->Mop_head = uop;
    uop->Mop_length = 1;
    uop->is_long_inst = true;

    // Cut out the connection between uops that belong to the same Mop
    // so that they can independently be scheduled.        
    if(uop->prev_inst) uop->prev_inst->next_inst = NULL;
    uop->prev_inst = NULL;
    uop = uop->next_inst;
  }
}

void pipeline_t::handle_idle_inst(inst_t *inst)
{
    /*inst_t *idle_inst = inst->Mop_head;
    while(idle_inst) {
        stats.eff_uop_count--;
        stats.interval.eff_uop_count--;
        idle_inst = idle_inst->next_inst;
    }
    stats.eff_Mop_count--;
    stats.interval.eff_Mop_count--;*/
}

bool pipeline_t::is_nop(inst_t *inst)
{
    if((inst->opcode == QSIM_INST_NULL)&&
       (inst->memcode == SPX_MEM_NONE)&&
       (inst->Mop_head == inst)&&
       (inst->src_reg == 0)&&
       (inst->src_flag == 0)&&
       (inst->dest_reg == 0)&&
       (inst->dest_flag == 0)) { 
#ifdef SPX_QSIM_DEBUG
        fprintf(stdout,"SPX_QSIM_DEBUG (core %d):      nop\n",core->core_id);
#endif
        return true; 
    }
    return false;
}
