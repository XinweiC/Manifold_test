#ifndef __EI_WRAPPER_H__
#define __EI_WRAPPER_H__

#include "kernel/component.h"
#include "kernel/clock.h"

#include "../../models/cache/mcp-cache/hash_table.h"

#include "../../models/energy_introspector/energy_introspector.h"
#include "../../models/cache/mcp-cache/cache-counters.h"
#include "../../models/processor/spx/pipeline.h"
#include "../../models/processor/spx/core.h"

namespace manifold {
namespace ei_wrapper {
typedef unsigned int tick_t;
	
class ei_wrapper_t : public manifold::kernel::Component
{
public:
  ei_wrapper_t(manifold::kernel::Clock* clk, EI::energy_introspector_t *energy_introspector, manifold::spx::pipeline_counter_t* proc_cnt, manifold::spx::ipa_t* proc_ipa, manifold::mcp_cache_namespace::L1_counter_t* c1_cnt, manifold::mcp_cache_namespace::L2_counter_t* c2_cnt, double sampling_period, int num_nodes, int uid);

  ~ei_wrapper_t();

  // manifold component functions
  void tick();
  uint64_t sam_cycle;
  tick_t slack_cycle;
  double last_vdd;
  double next_frq;

private:
  manifold::kernel::Clock *clock;
  EI::energy_introspector_t *ei;

  manifold::spx::ipa_t *p_ipa;
  manifold::spx::pipeline_counter_t *p_cnt;
  manifold::mcp_cache_namespace::L1_counter_t *l1_cnt;
  manifold::mcp_cache_namespace::L2_counter_t *l2_cnt;

//  void TL_feedback(int core_id);

  int id;
};

} // namespace ei_wrapper
} // namespace manifold

#endif

