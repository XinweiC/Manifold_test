#include "kernel/component-decl.h"

class Adder : public manifold::kernel::Component //Adder inherits from Component
{
  public:
  enum {Input0, Input1};
  enum {OutData};  
  Adder(uint64_t in1, uint64_t s);
  virtual void LinkArrival(int src, uint64_t data); // link arrival handler
  
  private:
  uint64_t sum;
  uint64_t knownInputs[2];
};
