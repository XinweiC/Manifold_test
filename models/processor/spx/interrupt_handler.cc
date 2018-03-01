#include "interrupt_handler.h"

using namespace std;
using namespace manifold;
using namespace manifold::kernel;
using namespace manifold::spx;

interrupt_handler_t::interrupt_handler_t(Clock *clk, Qsim::OSDomain *osd) :
    Qsim_osd(osd),
    clock_cycle(0),
    clock(clk),
    interval(1)
{
  Clock::Register<interrupt_handler_t>(*clock,this,&interrupt_handler_t::tick, (void(interrupt_handler_t::*)(void))0);
}

interrupt_handler_t::~interrupt_handler_t()
{
}

void interrupt_handler_t::set_interval(uint64_t Interval)
{
    interval = Interval;   
}

void interrupt_handler_t::tick()
{
    clock_cycle = clock->NowTicks();
    if((clock_cycle % interval == 0) && (clock_cycle > 0)) { 
      cerr << "@" << manifold::kernel::Manifold::NowTicks() << "***Entering timer interrupt!" << endl;
      Qsim_osd->timer_interrupt(); 
    }
}

