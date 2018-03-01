#include <iostream>
#include "register.h"
#include "adder.h"
#include "kernel/manifold.h"
#include "kernel/component.h"
#include "mpi.h"

using namespace std;
using namespace manifold::kernel;

#define LC3FREQ 1000.0
Clock globalClock(LC3FREQ); //Set the default clock frequency to 1000 ticks/sec

int main(int argc, char** argv)
{ 
  Manifold::Init(argc, argv);
  
/*Create PC and Adder, both on LP #0 */
  CompId_t PCReg = Component::Create<Register>(0);  // Create PCReg on LP#0
  CompId_t PCAdder = Component::Create<Adder, uint64_t, uint64_t>(1, 1, 0); // Create PCAdder on LP#1

/*Make the Connections as in Figure 2*/  
  Manifold::ConnectTime(PCReg, Register::OutData, PCAdder, Adder::Input0, &Adder::LinkArrival, 0.00001); 
  Manifold::ConnectTime(PCAdder, Adder::OutData, PCReg,  Register::InData, &Register::LinkArrival, 0.00001); 

  Manifold::StopAtTime(0.2); // stop at 0.2s, i.e. 200 ticks
  Manifold::Run(); // start the simulation
  cout << "Simulation completed" << endl;
  Manifold :: Finalize();
}

