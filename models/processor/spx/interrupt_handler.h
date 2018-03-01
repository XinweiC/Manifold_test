#ifndef __SPX_INTERRUPT_HANDLER_H__
#define __SPX_INTERRUPT_HANDLER_H__

#include "qsim.h"
#include "kernel/component.h"
#include "kernel/clock.h"

namespace manifold {
namespace spx {

class interrupt_handler_t : public manifold::kernel::Component
{
public:
    interrupt_handler_t(manifold::kernel::Clock *clk, Qsim::OSDomain *osd);
    ~interrupt_handler_t();

    void tick();
    void set_interval(uint64_t Interval);
    
private:
    Qsim::OSDomain *Qsim_osd;
    uint64_t clock_cycle;
    uint64_t interval;
    manifold::kernel::Clock *clock;
};

} // namespace spx
} // namespace manifold

#endif

