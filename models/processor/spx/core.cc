#include <string.h>
#include <libconfig.h++>
#include <sys/time.h>

#include "core.h"
#include "pipeline.h"
#include "outorder.h"
#include "inorder.h"

using namespace std;
using namespace libconfig;
using namespace manifold;
using namespace manifold::kernel;
using namespace manifold::spx;

spx_core_t::spx_core_t(Clock *clk, const int nodeID, Qsim::OSDomain *osd, const char *configFileName, int coreID) :
node_id(nodeID),
core_id(coreID),
clock(clk)
{
  // Register a Manifold clock.
  Clock::Register<spx_core_t>(*clock,this,&spx_core_t::tick, (void(spx_core_t::*)(void))0);

  Config parser;
  try
  {
    parser.readFile(configFileName);
  }
  catch(FileIOException e)
  {
    fprintf(stdout,"cannot read configuration file %s\n",configFileName);
    exit(1);
  }
  catch(ParseException e)
  {
    fprintf(stdout,"cannot parse configuration file %s\n",configFileName);
    exit(1);
  }
  
  try
  {
    // Pipeline model
    const char *pipeline_model = parser.lookup("pipeline");
    if(!strcasecmp(pipeline_model,"outorder"))
      pipeline = new outorder_t(this,&parser);
    else if(!strcasecmp(pipeline_model,"inorder"))
      pipeline = new inorder_t(this,&parser);
    else
    {
      fprintf(stdout,"unknown core type %s\n",pipeline_model);
      exit(1);
    }

    if(osd) // If Qsim_osd given, use Qsim library.
    {
      pipeline->config.qsim = SPX_QSIM_LIB;

      if(!osd)
      {
        fprintf(stdout,"Qsim_lib without Qsim_osd\n");
        exit(1);
      }

#ifdef IPA_CTRL
      ipa = new ipa_t();
#endif

      pipeline->Qsim_osd = osd;
      pipeline->Qsim_osd->set_inst_cb(pipeline,&pipeline_t::Qsim_inst_cb);
      pipeline->Qsim_osd->set_mem_cb(pipeline,&pipeline_t::Qsim_mem_cb);
      pipeline->Qsim_osd->set_reg_cb(pipeline,&pipeline_t::Qsim_reg_cb);
      pipeline->Qsim_osd->set_app_end_cb(pipeline,&pipeline_t::Qsim_app_end_cb);
      fprintf(stdout,"=== core %d with qsim_lib : ",core_id);
    }
    else // Otherwise, assume Qsim client.
    {
#if 0
      pipeline->config.qsim = SPX_QSIM_CLIENT;

      const char *server = parser.lookup("qsim_server");
      const char *port = parser.lookup("qsim_port");
      pipeline->Qsim_client = new Qsim::Client(client_socket(server,port));
      pipeline->Qsim_client->set_inst_cb(pipeline,&pipeline_t::Qsim_inst_cb);
      pipeline->Qsim_client->set_mem_cb(pipeline,&pipeline_t::Qsim_mem_cb);
      pipeline->Qsim_client->set_reg_cb(pipeline,&pipeline_t::Qsim_reg_cb);

      fprintf(stdout,"=== core %d with qsim_client : ",core_id);
#endif
    }

    // Fastforward instructions.
    uint64_t fastfwd = parser.lookup("fastfwd");
    while(fastfwd-- > 0)
    {
      if(pipeline->config.qsim == SPX_QSIM_LIB)
        pipeline->Qsim_osd->run(core_id,1);
      else {} // SPX_QSIM_CLIENT
        //pipeline->Qsim_client->run(core_id,1);
    }
    fprintf(stdout,"fastfwd completed ===\n");
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

#ifdef LIBEI
  pipeline->counters = new pipeline_counter_t();
#endif

}

spx_core_t::~spx_core_t()
{
  delete pipeline->counters;
  delete pipeline;
}

void spx_core_t::tick()
{
  clock_cycle = clock->NowTicks();
  
  if(pipeline->finished == 1) {
    fprintf(stderr,"core: %d cycle: %lu app end\n",core_id,clock_cycle);
		manifold :: kernel :: Manifold :: Terminate();      
  }

  //fprintf(stdout,"core%d.%llu\n",core_id,clock_cycle);

  if(clock_cycle == 1)
    gettimeofday(&pipeline->stats.interval.start_time,0);

  timeval start, end;
  gettimeofday(&start,0);

#ifdef SPX_SPEED_DEBUG
  if(clock_cycle == 1)
    gettimeofday(&pipeline->stats.interval.start_time,0);

  timeval start, end;
  gettimeofday(&start,0);
#endif

  pipeline->commit();
  pipeline->memory();
  pipeline->execute();
  pipeline->allocate();
  pipeline->frontend();
  //pipeline->stats.last_commit_cycle = clock_cycle;

  //cerr << core_id << "," << clock_cycle << "\t" <<  pipeline->counters->fpu.read << ", " << pipeline->counters->mul.read << endl;

  gettimeofday(&end,0);

  double tick_time = (end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec)*1e-6;
  pipeline->stats.core_time += tick_time;
  pipeline->stats.interval.core_time += tick_time;

#ifdef SPX_SPEED_DEBUG
  gettimeofday(&end,0);

  double tick_time = (end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec)*1e-6;
  pipeline->stats.core_time += tick_time;
  pipeline->stats.interval.core_time += tick_time;

  uint64_t sample_cycle = 100000;
  if((clock_cycle % sample_cycle == 0)&&(clock_cycle > 0))
  {
    fprintf(stdout,"%3.1lfM (per-core stat) core %d: avg ticks/sec = %lf Mops = %lu uops = %lu Mops/sec = %lf uops/sec = %lf Mops/cycle = %lf uops/cycle = %lf | tmp ticks/src = %lf Mop = %lu uop = %lu Mops/sec = %lf uops/sec = %lf Mops/cycle = %lf uops/cycle = %lf\n",
    (double)clock_cycle/1000000,
    core_id,
    (double)clock_cycle/pipeline->stats.core_time,
    pipeline->stats.Mop_count,
    pipeline->stats.uop_count,
    (double)pipeline->stats.Mop_count/pipeline->stats.core_time,
    (double)pipeline->stats.uop_count/pipeline->stats.core_time,
    (double)pipeline->stats.Mop_count/clock_cycle,
    (double)pipeline->stats.uop_count/clock_cycle,
    (double)sample_cycle/pipeline->stats.interval.core_time,
    pipeline->stats.interval.Mop_count,
    pipeline->stats.interval.uop_count,
    (double)pipeline->stats.interval.Mop_count/pipeline->stats.interval.core_time,
    (double)pipeline->stats.interval.uop_count/pipeline->stats.interval.core_time,
    (double)pipeline->stats.interval.Mop_count/sample_cycle,
    (double)pipeline->stats.interval.uop_count/sample_cycle);

    timeval current_checkpoint;
    gettimeofday(&current_checkpoint,0);

    double total_time = (current_checkpoint.tv_sec-pipeline->stats.interval.start_time.tv_sec)+(current_checkpoint.tv_usec-pipeline->stats.interval.start_time.tv_usec)*1e-6;
    pipeline->stats.total_time += total_time;
    pipeline->stats.interval.start_time = current_checkpoint;

    fprintf(stdout,"%3.1lfM (core+others included stat) core %d: avg ticks/sec = %lf Mops/sec = %lf uops/sec = %lf | tmp ticks/sec = %lf Mops/sec = %lf uops/sec = %lf\n",
    (double)clock_cycle/1000000,
    core_id,
    (double)clock_cycle/pipeline->stats.total_time,
    (double)pipeline->stats.Mop_count/pipeline->stats.total_time,
    (double)pipeline->stats.uop_count/pipeline->stats.total_time,
    (double)sample_cycle/total_time,
    (double)pipeline->stats.interval.Mop_count/total_time,
    (double)pipeline->stats.interval.uop_count/total_time);

    pipeline->stats.interval.Mop_count = 0;
    pipeline->stats.interval.uop_count = 0;
    pipeline->stats.interval.core_time = 0.0;
  }
#endif
}

void spx_core_t::handle_cache_response(int temp, cache_request_t *cache_request)
{
  pipeline->handle_cache_response(temp,cache_request);
}

double uops_total = 0;
double mops_total = 0;
double aggr_ipc = 0;
void spx_core_t::print_stats(void)
{
    uops_total += (double)pipeline->stats.uop_count;
    mops_total += (double)pipeline->stats.Mop_count;
    aggr_ipc += (double)pipeline->stats.uop_count/clock_cycle;

    if (core_id == 0) {
      fprintf(stderr,"\n\nMaster Clock: %lf, Cycle:%lu\n", manifold::kernel::Clock::Master().freq, manifold::kernel::Clock::Master().NowTicks());
      fprintf(stderr,"Time:%lf\n\n",  manifold::kernel::Clock::Master().NowTime());
      fprintf(stderr,"Time:%lf\n\n",  (double)manifold::kernel::Clock::Master().NowTicks() / manifold::kernel::Clock::Master().freq);
    }    

    timeval current_checkpoint;
    gettimeofday(&current_checkpoint,0);

    double total_time = (current_checkpoint.tv_sec-pipeline->stats.interval.start_time.tv_sec)+(current_checkpoint.tv_usec-pipeline->stats.interval.start_time.tv_usec)*1e-6;
    pipeline->stats.total_time += total_time;
    pipeline->stats.interval.start_time = current_checkpoint;

# if 0
    fprintf(stderr,"***%3.2lfM (per-core stat) core %d: avg ticks/sec = %lf Mops = %lu uops = %lu Mops/sec = %lf uops/sec = %lf Mops/cycle = %lf uops/cycle = %lf | tmp ticks/src = %lf Mop = %lu uop = %lu Mops/sec = %lf uops/sec = %lf Mops/cycle = %lf uops/cycle = %lf Mops_total = %lf uops_total = %lf Aggr IPC = %lf\n",
    (double)clock_cycle/1000000,
    core_id,
    (double)clock_cycle/pipeline->stats.core_time,
    pipeline->stats.Mop_count,
    pipeline->stats.uop_count,
    (double)pipeline->stats.Mop_count/pipeline->stats.core_time,
    (double)pipeline->stats.uop_count/pipeline->stats.core_time,
    (double)pipeline->stats.Mop_count/clock_cycle,
    (double)pipeline->stats.uop_count/clock_cycle,
    (double)clock_cycle/pipeline->stats.interval.core_time,
    pipeline->stats.interval.Mop_count,
    pipeline->stats.interval.uop_count,
    (double)pipeline->stats.interval.Mop_count/pipeline->stats.interval.core_time,
    (double)pipeline->stats.interval.uop_count/pipeline->stats.interval.core_time,
    (double)pipeline->stats.interval.Mop_count/clock_cycle,
    (double)pipeline->stats.interval.uop_count/clock_cycle,
     mops_total,
     uops_total,
     aggr_ipc);
#endif

    fprintf(stderr,"***%3.2lfM (per-core stat) core %d: Mops = %lu K%lu U%lu uops = %lu %lu L%lu S%lu I%lu ops/cycle = %lf Mops_total = %lf uops_total = %lf Aggr IPC = %lf MIPS = %lf %lf\n",
    (double)clock_cycle/1000000,
    core_id,
    pipeline->stats.Mop_count,
    pipeline->stats.KERN_count,
    pipeline->stats.USER_count,
    pipeline->stats.uop_count,
    pipeline->stats.nm_count,
    pipeline->stats.ld_count,
    pipeline->stats.st_count,
    pipeline->stats.nop_count,
    (double)pipeline->stats.uop_count/clock_cycle,
     mops_total,
     uops_total,
     aggr_ipc,
     uops_total * manifold::kernel::Clock::Master().NowTime(),
     mops_total * manifold::kernel::Clock::Master().NowTime());

}

#if 0
void spx_core_t::print_stats(std::ostream& out)
{
    fprintf(stdout,"%3.1lfM (per-core stat) core %d: avg ticks/sec = %lf Mops = %lu uops = %lu Mops/sec = %lf uops/sec = %lf Mops/cycle = %lf uops/cycle = %lf | tmp ticks/src = %lf Mop = %lu uop = %lu Mops/sec = %lf uops/sec = %lf Mops/cycle = %lf uops/cycle = %lf\n",
    (double)clock_cycle/1000000,
    core_id,
    (double)clock_cycle/pipeline->stats.core_time,
    pipeline->stats.Mop_count,
    pipeline->stats.uop_count,
    (double)pipeline->stats.Mop_count/pipeline->stats.core_time,
    (double)pipeline->stats.uop_count/pipeline->stats.core_time,
    (double)pipeline->stats.Mop_count/clock_cycle,
    (double)pipeline->stats.uop_count/clock_cycle,
    (double)sample_cycle/pipeline->stats.interval.core_time,
    pipeline->stats.interval.Mop_count,
    pipeline->stats.interval.uop_count,
    (double)pipeline->stats.interval.Mop_count/pipeline->stats.interval.core_time,
    (double)pipeline->stats.interval.uop_count/pipeline->stats.interval.core_time,
    (double)pipeline->stats.interval.Mop_count/sample_cycle,
    (double)pipeline->stats.interval.uop_count/sample_cycle);
}
#endif
