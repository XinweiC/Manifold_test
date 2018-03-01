#ifndef __SPX_PIPELINE_H__
#define __SPX_PIPELINE_H__

#include <libconfig.h++>
#include "qsim.h"
//#include "qsim-client.h"
#include "instruction.h"
#include "component.h"

#ifdef LIBEI
//#include "ei-client.h"
//#include "ei-defs.h"
#include "../../energy_introspector/definitions.h"
#endif

namespace manifold {
namespace spx {

enum SPX_PIPELINE_TYPES
{
  SPX_PIPELINE_OUTORDER = 0,
  SPX_PIPELINE_INORDER,
  SPX_NUM_PIPELINE_TYPES
};
  
enum SPX_QSIM_CB_TYPES
{
  SPX_QSIM_INIT_CB = 0,
  SPX_QSIM_INST_CB,
  SPX_QSIM_SRC_REG_CB,
  SPX_QSIM_SRC_FLAG_CB,
  SPX_QSIM_MEM_CB,
  SPX_QSIM_LD_CB,
  SPX_QSIM_ST_CB,
  SPX_QSIM_DEST_REG_CB,
  SPX_QSIM_DEST_FLAG_CB,
  NUM_SPX_QSIM_CB_TYPES
};

enum SPX_QSIM_FRONTEND_TYPES
{
  SPX_QSIM_LIB = 0,
  SPX_QSIM_CLIENT,
  NUM_SPX_QSIM_FRONTEND_TYPES
};

class pipeline_config_t
{
public:
  pipeline_config_t() {}
  ~pipeline_config_t() {}

  int qsim;
  
  int fetch_width;
  int alloc_width;
  int exec_width;
  int commit_width;
  
  int instQ_size;
  int RS_size;
  int LDQ_size;
  int STQ_size;
  int ROB_size;

  int FU_delay[SPX_NUM_FU_TYPES]; // delays for each FU
  int FU_issue_rate[SPX_NUM_FU_TYPES]; // issue rate for each FU
  std::vector<int> FU_port[SPX_NUM_FU_TYPES]; // ports for each FU

  struct
  {
    uint32_t size;
    int line_width;
    int assoc;
    int offset_mask;
    int index_mask;
  }IL1, DL1;
};

class pipeline_stats_t
{
public:
  pipeline_stats_t()
  {
    //eff_uop_count = 0;
    uop_count = 0;
    st_count = 0;
    ld_count = 0;
    nm_count = 0;
    //eff_Mop_count = 0;
    Mop_count = 0;
    KERN_count = 0;
    USER_count = 0;
    nop_count = 0;
    last_commit_cycle = 0;
    core_time = 0.0;
    total_time = 0.0;
    
    //interval.eff_uop_count = 0;
    interval.uop_count = 0;
    //interval.eff_Mop_count = 0;
    interval.Mop_count = 0;
    interval.core_time = 0.0;
  }
  ~pipeline_stats_t() {}
  
  //uint64_t eff_uop_count,eff_Mop_count;
  uint64_t uop_count;
  uint64_t st_count;
  uint64_t ld_count;
  uint64_t nm_count;
  uint64_t Mop_count;
  uint64_t KERN_count;
  uint64_t USER_count;
  uint64_t nop_count;
  uint64_t last_commit_cycle;
  double core_time;
  double total_time;
  
  struct {
    //uint64_t eff_uop_count,eff_Mop_count;
    uint64_t uop_count;
    uint64_t Mop_count;
    double core_time;
    timeval start_time;
  }interval;
};

#ifdef LIBEI
class pipeline_counter_t
{
public:
  double time_tick, period;
  pipeline_counter_t()
  {
    reset();
  }
  ~pipeline_counter_t() {}

  void reset()
  {
    l1_btb.reset(); l2_btb.reset(); ras.reset(); inst_tlb.reset();
    predictor_chooser.reset(); global_predictor.reset(); 
    l1_local_predictor.reset(); l2_local_predictor.reset();
    inst_cache.reset(); inst_cache_miss_buffer.reset(); inst_cache_prefetch_buffer.reset();
    latch_ic2ib.reset(); pc.reset(); inst_buffer.reset(); latch_ib2id.reset();
    inst_decoder.reset(); operand_decoder.reset(); uop_sequencer.reset();
    latch_id2uq.reset(); uop_queue.reset(); latch_uq2rat.reset(); frontend_undiff.reset();

    rat.reset(); freelist.reset(); dependency_check.reset(); latch_rat2rs.reset();
    rs.reset(); issue_select.reset(); latch_rs2ex.reset(); reg_int.reset(); reg_fp.reset();
    rob.reset(); latch_rob2rs.reset(); latch_rob2arf.reset(); scheduler_undiff.reset();

    alu.reset(); mul.reset(); int_bypass.reset(); latch_ex_int.reset(); ex_int_undiff.reset();
    fpu.reset(); fp_bypass.reset(); latch_ex_fp.reset(); ex_fp_undiff.reset();

    stq.reset(); latch_stq2mem.reset(); latch_stq2ldq.reset();
    ldq.reset(); latch_ldq2mem.reset(); latch_ldq2rs.reset();
    data_cache.reset(); data_cache_miss_buffer.reset(); data_cache_prefetch_buffer.reset();
    data_cache_writeback_buffer.reset(); data_tlb.reset(); l2_tlb.reset();

    fetch_inst.reset();
    retire_inst.reset();
  }
  // Frontend components (including inst cache)
  EI::counters_t l1_btb, l2_btb, ras, inst_tlb;
  EI::counters_t predictor_chooser, global_predictor;
  EI::counters_t l1_local_predictor, l2_local_predictor;
  EI::counters_t inst_cache, inst_cache_miss_buffer, inst_cache_prefetch_buffer;
  EI::counters_t latch_ic2ib, pc, inst_buffer, latch_ib2id;
  EI::counters_t inst_decoder, operand_decoder, uop_sequencer;
  EI::counters_t latch_id2uq, uop_queue, latch_uq2rat, frontend_undiff;

  // Scheduler components
  EI::counters_t rat, freelist, dependency_check, latch_rat2rs;
  EI::counters_t rs, issue_select, latch_rs2ex, reg_int, reg_fp;
  EI::counters_t rob, latch_rob2rs, latch_rob2arf, scheduler_undiff;

  // Integer execution components
  EI::counters_t alu, mul, int_bypass, latch_ex_int, ex_int_undiff;

  // Floating-point execution components
  EI::counters_t fpu, fp_bypass, latch_ex_fp, ex_fp_undiff;

  // Memory unit components (including data cache not used)
  EI::counters_t stq, latch_stq2mem, latch_stq2ldq;
  EI::counters_t ldq, latch_ldq2mem, latch_ldq2rs;
  EI::counters_t data_cache, data_cache_miss_buffer, data_cache_prefetch_buffer;
  EI::counters_t data_cache_writeback_buffer, data_tlb, l2_tlb;

  // Count fetched/committed insts
  EI::counters_t fetch_inst;
  EI::counters_t retire_inst;
/*
// Frontend components (including inst cache)
  libEI::counter_t l1_btb, l2_btb, ras, inst_tlb;
  libEI::counter_t predictor_chooser, global_predictor;
  libEI::counter_t l1_local_predictor, l2_local_predictor;
  libEI::counter_t inst_cache, inst_cache_miss_buffer, inst_cache_prefetch_buffer;
  libEI::counter_t latch_ic2ib, pc, inst_buffer, latch_ib2id;
  libEI::counter_t inst_decoder, operand_decoder, uop_sequencer;
  libEI::counter_t latch_id2uq, uop_queue, latch_uq2rat, frontend_undiff;

  // Scheduler components
  libEI::counter_t rat, freelist, dependency_check, latch_rat2rs;
  libEI::counter_t rs, issue_select, latch_rs2ex, reg_int, reg_fp;
  libEI::counter_t rob, latch_rob2rs, latch_rob2arf, scheduler_undiff;

  // Integer execution components
  libEI::counter_t alu, mul, int_bypass, latch_ex_int, ex_int_undiff;

  // Floating-point execution components
  libEI::counter_t fpu, fp_bypass, latch_ex_fp, ex_fp_undiff;

  // Memory unit components (including data cache not used)
  libEI::counter_t stq, latch_stq2mem, latch_stq2ldq;
  libEI::counter_t ldq, latch_ldq2mem, latch_ldq2rs;
  libEI::counter_t data_cache, data_cache_miss_buffer, data_cache_prefetch_buffer;
  libEI::counter_t data_cache_writeback_buffer, data_tlb, l2_tlb;
*/
};
#endif

class pipeline_t
{
public:
  pipeline_t();
  //virtual ~pipeline_t() { delete Qsim_client; }
  virtual ~pipeline_t() {  }
  
  // pipeline functions
  virtual void commit() = 0;
  virtual void memory() = 0;
  virtual void execute() = 0;
  virtual void allocate() = 0;
  virtual void frontend() = 0;
  virtual void fetch(inst_t *inst) = 0;
  virtual void handle_cache_response(int temp, cache_request_t *cache_request) = 0;
  virtual void set_config(libconfig::Config *config) = 0;
  void handle_long_inst(inst_t *inst);
  void handle_idle_inst(inst_t *inst);
  bool is_nop(inst_t *inst);

  // Qsim callback functions
  void Qsim_inst_cb(int core_id, uint64_t vaddr, uint64_t paddr, uint8_t len, const uint8_t *bytes, enum inst_type type);
  int Qsim_mem_cb(int core_id, uint64_t vaddr, uint64_t paddr, uint8_t size, int type);
  void Qsim_reg_cb(int core_id, int reg, uint8_t size, int type);
  void Qsim_post_cb(inst_t *inst); // modify inst information after Qsim callbacks are completed
  int Qsim_app_end_cb(int core_id);
  Qsim::OSDomain *Qsim_osd; // Qsim OS domain
  //Qsim::Client *Qsim_client; // Qsim client

  pipeline_config_t config;
  pipeline_stats_t stats;
  int finished;
  int runout_flag;
  int trans_flag;
#ifdef LIBEI
  //libEI::client_t *EI_client;
  pipeline_counter_t *counters;
#endif

protected:
  int type;
  spx_core_t *core;
  inst_t *next_inst; // next_inst from Qsim_cb
  
  uint64_t Mop_count;
  uint64_t uop_count;
  int Qsim_cb_status;
  bool idle_loop; // Qsim is in idle loop
};

} //namespace manifold
} //namespace spx
#endif
