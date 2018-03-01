#include <iostream>
#include "register.h"
#include "kernel/clock.h"
#include "kernel/component.h"
#include "kernel/manifold.h"

using namespace std;
using namespace manifold::kernel;

Register::Register()
{ 
  latchedInput = 0;
  value = 0;
  knownInputs[InData] = 0;
  knownInputs[InWriteEnable] = 1; // Always enabled
  Clock::Register<Register>(this, &Register::RisingEdge, &Register::FallingEdge); //Registered to the default clock
}


void Register::RisingEdge()
{ 
   if (knownInputs[InWriteEnable] != 0)
    {
      latchedInput = knownInputs[InData];
    }
}

void Register::FallingEdge()
{ // Copy latched input to ouput
  value = latchedInput;
  // Sent output here
  cout << "value of PC is " << value 
       << " NowTicks " << Manifold::NowTicks()
       << endl;
  Send(OutData, value);
}

void Register::LinkArrival(int src, uint64_t data) 
{
  knownInputs[src] = data;
}

