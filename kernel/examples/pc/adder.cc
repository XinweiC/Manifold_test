#include <iostream>
#include <iomanip>
#include "adder.h"
#include "kernel/manifold.h"

Adder::Adder(uint64_t in1, uint64_t s)
  : sum(s)
{
  knownInputs[1] = in1;
}

void Adder::LinkArrival(int src, uint64_t data) 
{
  knownInputs[src] = data;  
  uint64_t sum = 0;
  for (int i = 0; i < 2; ++i)
  {
    sum += knownInputs[i];
  }

  Send(OutData, sum);
}
