#ifndef __PIPELINE_OUTORDER_H__
#define __PIPELINE_OUTORDER_H__

#include "component.h"
#include "pipeline.h"

namespace manifold {
namespace spx {

class outorder_t : public pipeline_t
{
public:
  outorder_t(spx_core_t *spx_core, libconfig::Config *parser);
  ~outorder_t();
  
  void commit();
  void memory();
  void execute();
  void allocate();
  void frontend();
  void fetch(inst_t *inst);
  void handle_cache_response(int temp, cache_request_t *cache_request);
  void set_config(libconfig::Config *config);  
  
private:
  instQ_t *instQ;
  ROB_t *ROB;
  RS_t *RS;
  STQ_t *STQ;
  LDQ_t *LDQ;
  RF_t *RF;
  std::vector<EX_t*> EX;
};
  
} // namespace spx
} // namespace manifold

#endif

