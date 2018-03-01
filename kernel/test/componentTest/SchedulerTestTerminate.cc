//! This program tests system termination initiated by a component. Any
//! component can call Manifold :: Terminate() to initiate termination
//! of the simulation program.
//! In this program, we test the case where only one initiator starts
//! the termination.
//!
//!
#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <assert.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "mpi.h"
#include "messenger.h"
#include "component.h"
#include "manifold.h"

using namespace std;
using namespace manifold::kernel;


//####################################################################
// Helper classes
//####################################################################

Ticks_t TermTick; //when to terminate
const int TERM_INITIATOR = 1; //which LP initiates termination

class TestTickComponent : public Component {
public:
    TestTickComponent(int lp) : m_lp(lp)
    {
	m_ticks = 0;
    }

    void tick()
    {
        //cout << "component in LP " << m_lp << " tick " << m_ticks << "\n";
	if(m_ticks == TermTick && m_lp == TERM_INITIATOR) {
	    Manifold :: Terminate();
	}
	m_ticks++;
    }

private:
    int m_lp; //LP of the component.
    int m_ticks;
};






//####################################################################
//####################################################################
//! This is the unit testing class for the class Scheduler.
class SchedulerTest : public CppUnit::TestFixture {
private:

public:
        //==========================================================================
        //==========================================================================
        void test_terminate()
	{
	    int Mytid; //task id
	    MPI_Comm_rank(MPI_COMM_WORLD, &Mytid);

#define DBG
	    #ifdef DBG
	    // create a file into which to write debug info.
	    char buf[10];
	    sprintf(buf, "DBG_LOG%d", Mytid);
	    ofstream DBG_LOG(buf);
	    #endif

	    const int Stop = 1000;

	    assert(Stop > 10);
	    TermTick = random() % (Stop-10) + 1;

            TestTickComponent* comp = new TestTickComponent(Mytid);

	    Clock::Register(comp, &TestTickComponent :: tick, (void(TestTickComponent::*)(void))0);

	    Manifold :: StopAt(Stop);
	    Manifold :: Run();

	    if(Mytid == TERM_INITIATOR) {
	        CPPUNIT_ASSERT_EQUAL(TermTick, Manifold :: NowTicks());
	    }

	    #ifdef DBG
	    Manifold::get_scheduler()->print_stats(DBG_LOG);
	    #endif

	}



        /**
	 * Build a test suite.
	 */
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ScheduelerTest");

	    mySuite->addTest(new CppUnit::TestCaller<SchedulerTest>("test_terminate", &SchedulerTest::test_terminate));

	    return mySuite;
	}
};



int main(int argc, char** argv)
{
    Clock myClock(1000);

    Manifold :: Init(argc, argv, Manifold::TICKED, SyncAlg::SA_CMB);

    if(2 > TheMessenger.get_node_size()) {
        cerr << "ERROR: Must specify \"-np N (N>1)\" for mpirun!" << endl;
	return 1;
    }

    CppUnit::TextUi::TestRunner runner;
    runner.addTest( SchedulerTest::suite() );
    runner.run();

    Manifold :: Finalize();

    return 0;
}

