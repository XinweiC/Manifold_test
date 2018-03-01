#include "kernel/component-decl.h"

class Register : public manifold::kernel::Component
{
public:
  // Registers have two inputs (data/write-enable) and one output
  enum { InData, InWriteEnable };
  enum { OutData };
  Register(); 
  void RisingEdge(); // UpTick handler
  void FallingEdge(); // DownTick handler
  void LinkArrival(int src, uint64_t data); //link arrival handler

private:
  uint64_t knownInputs[2];
  uint64_t latchedInput; // Current latched input
  uint64_t value;        // Current output value
};

