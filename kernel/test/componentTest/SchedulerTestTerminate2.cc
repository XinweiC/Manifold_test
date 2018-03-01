//! This program tests system termination initiated by a component. Any
//! component can call Manifold :: Terminate() to initiate termination
//! of the simulation program.
//! In this program, we test the case where multiple initiators start
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
    TestTickComponent(int lp, bool isTerm) : m_lp(lp), m_isTerminator(isTerm)
    {
	m_ticks = 0;
    }

    void tick()
    {
        //cout << "component in LP " << m_lp << " tick " << m_ticks << "\n";
	if(m_ticks == TermTick && m_isTerminator) {
	    //cout << "calling Terminateeeeeeeeeeeeeeeeeeeeeeeeeeeee\n";
	    Manifold :: Terminate();
	}
	m_ticks++;
    }

private:
    int m_lp; //LP of the component.
    bool m_isTerminator;
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

	    const int Stop = 1000;

	    assert(Stop > 10);
	    TermTick = random() % (Stop-10) + 1;


	    //ensure we have at least 2 terminators.
            int terminator0, terminator1;

	    int n_ranks = TheMessenger.get_node_size();

	    assert(n_ranks > 1);

	    terminator0 = random() % n_ranks;
	    while((terminator1 = random() % n_ranks) == terminator0);

	    bool isTerminator[n_ranks];

	    for(int i=0; i<n_ranks; i++) {
		if(Mytid == terminator0 || Mytid == terminator1)
		    isTerminator[i] = true;
		else
		    isTerminator[i] = ((random() % 2 == 0) ? true : false); //50% chance
	    }

            TestTickComponent* comp = new TestTickComponent(Mytid, isTerminator[Mytid]);

	    Clock::Register(comp, &TestTickComponent :: tick, (void(TestTickComponent::*)(void))0);

	    Manifold :: StopAt(Stop);
	    Manifold :: Run();

	    if(isTerminator[Mytid]) {
	        CPPUNIT_ASSERT_EQUAL(TermTick, Manifold :: NowTicks());
	    }

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

    Manifold :: Init(argc, argv);

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

