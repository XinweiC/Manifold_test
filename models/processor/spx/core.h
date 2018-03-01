#ifndef __SPX_CORE_H__
#define __SPX_CORE_H__

#include "qsim.h"
#include "kernel/component.h"
#include "kernel/clock.h"
#include "pipeline.h"

//#define SPX_QSIM_DEBUG_ 1

#define IPA_CTRL

namespace manifold {
namespace spx {

#ifdef IPA_CTRL
class ipa_t
{
 public:
  ipa_t(double mips = 4000e6, int m = 5000) { depart_last = 0; update_cycle_last = 0; update_last = 0; target = mips; window_m = m; new_freq = manifold::kernel::Clock::Master().freq; is_transient = false; }

  uint64_t depart_last;
  uint64_t update_cycle_last;
  uint64_t update_last;
  double target;
  double new_freq;
  bool is_transient;
  int window_m;
};
#endif

class spx_core_t : public manifold::kernel::Component
{
public:
  spx_core_t(manifold::kernel::Clock *clk, const int nodeID, Qsim::OSDomain *osd, const char *configFileName, const int coreID);
  ~spx_core_t();

  enum { IN_FROM_CACHE = 0 };
  enum { OUT_TO_CACHE = 0 };

  // manifold component functions
  void tick();
  void handle_cache_response(int temp, cache_request_t *cache_request);

  int node_id; // manifold node ID
  int core_id; // processor ID
  uint64_t clock_cycle;

  pipeline_t *pipeline; // base class of pipeline models

  void print_stats(void);
#ifdef IPA_CTRL
  ipa_t* ipa;
  manifold::kernel::Clock *clock;
#endif
private:
};

} // namespace spx
} // namespace manifold

#endif

