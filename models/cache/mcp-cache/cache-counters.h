#ifndef CACHE_COUNTERS_H
#define CACHE_COUNTERS_H

#include "../../energy_introspector/energy_introspector.h"

using namespace std;

#define MCP_CACHE_COUNTERS
namespace manifold {
namespace mcp_cache_namespace {
class L1_counter_t {
 public:
  L1_counter_t() { reset(); }
  ~L1_counter_t() {}

  class : public EI::counters_t {
   public:
     EI::counters_t missbuf, prefetch, linefill, writeback;
  }IL1, DL1, ITLB, DTLB;

  void reset(void) {
    IL1.reset(); IL1.missbuf.reset(); IL1.prefetch.reset(); IL1.linefill.reset(); IL1.writeback.reset();
    DL1.reset(); DL1.missbuf.reset(); DL1.prefetch.reset(); DL1.linefill.reset(); DL1.writeback.reset();
    ITLB.reset(); ITLB.missbuf.reset(); ITLB.prefetch.reset(); ITLB.linefill.reset(); ITLB.writeback.reset();
    DTLB.reset(); DTLB.missbuf.reset(); DTLB.prefetch.reset(); DTLB.linefill.reset(); DTLB.writeback.reset();
  }
};

class L2_counter_t {
 public:
  L2_counter_t() { reset(); }
  ~L2_counter_t() {}

  class : public EI::counters_t {
   public:
     EI::counters_t missbuf, prefetch, linefill, writeback;
  }DL2, DTLB2;

  void reset(void) {
    DL2.reset(); DL2.missbuf.reset(); DL2.prefetch.reset(); DL2.linefill.reset(); DL2.writeback.reset();
    DTLB2.reset(); DTLB2.missbuf.reset(); DTLB2.prefetch.reset(); DTLB2.linefill.reset(); DTLB2.writeback.reset();
  }
};
}
}
#endif
