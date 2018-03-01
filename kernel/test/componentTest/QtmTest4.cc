//!
//! @brief This program tests the QTM algorithm.
//!
//! In this program we schedule a few events. The timestamps of the events are
//! a series of random numbers. The processes will send messages to each other.
//! Time base for this program is ticks.
//!
//! This program can be run with N (N>1) LPs. Each creates an arrary of increasing
//! values representing timestamps.
//!
#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>
#include <time.h>

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include "mpi.h"
#include "manifold.h"
#include "component.h"
#include "messenger.h"
#include "quantum_scheduler.h"

using namespace std;
using namespace manifold::kernel;

//####################################################################
// helper classes
//####################################################################
class MyData {
public:
    int x;
};


class MyObj1 : public Component {
private:
    const Clock& m_clock;

public:
    MyObj1(const Clock& clk) : m_clock(clk) {}

    void handler0()
    {
    	Scheduler* sch = Manifold :: get_scheduler();
	    //QuantumSyncAlg* qtmAlg = dynamic_cast<QuantumSyncAlg*>(sch->m_syncAlg);
	    //CPPUNIT_ASSERT(qtmAlg != 0);

      //cout << "get barrier is " << qtmAlg->GetBarrier() << endl;
	    //When the event handler is called, it must be safe to process the timestamp.
	    //CPPUNIT_ASSERT(Manifold::NowTicks() * m_clock.period < qtmAlg->GetBarrier());

	    //randomly pick a successor and send a message.
	    vector<LpId_t>& succs = sch->get_successors();
	    if(succs.size() > 0) {
	      LpId_t succ = succs[random() % succs.size()];
	      MyData* data = new MyData;
	      Send(succ, data);
	    }
    }

    void handle_incoming(int, MyData* d)
    {
        delete d;
    }

};

//####################################################################
//####################################################################
class ManifoldTest : public CppUnit::TestFixture {
private:
    static Clock MasterClock;
    enum { MASTER_CLOCK_HZ = 1 };

    unsigned QUANTUM;

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }

    //! Test the QTM algorithm.
    //!
    //! Each MPI task creates a series of random numbers in increasing
    //! order to represent timestamps of events.
    //! The first number is randomly generated; the rest are generated by
    //! adding a random interval to the previous number.
    //! Then we go through the series and simulating event processing.
    //! When an event is being processed, we verify it is indeed safe to
    //! do so, and randomly pick an output component and send a message.
    //!
    //! For each LP, creat a component; then connect the components in
    //! a random way: for each component C, randomly decide whether D != C
    //! should be a successor of C.
    void test()
    {
        int noOfLps=TheMessenger.get_node_size();
        int Mytid = TheMessenger.get_node_id();

        char buf[10];
        sprintf(buf, "DBG_LOG%d", Mytid);
        ofstream DBG_LOG(buf);
	//redirect cout to file.
	std::streambuf* cout_sbuf = std::cout.rdbuf(); // save original sbuf
	std::cout.rdbuf(DBG_LOG.rdbuf()); // redirect cout


        //all LP must use same seed for random number for interconnection

        //create a component for each LP
        CompId_t comps[noOfLps];

        for(int i=0; i<noOfLps; i++) {
            comps[i] = Component :: Create<MyObj1>(i, MasterClock);
        }

        MyObj1*  MyComp = Component :: GetComponent<MyObj1>(comps[Mytid]);

        vector<LpId_t> expected_preds;
        vector<LpId_t> expected_succs;

        //connect the components
        for(int i=0; i<noOfLps; i++) {
            int count = 0; //successors count

            for(int j=0; j<noOfLps; j++) {
		if(i != j) {
		    if(random() % 2 == 0) {
			count++;
			Manifold :: Connect(comps[i], j, comps[j], i, &MyObj1::handle_incoming, 1);
			//these are used to verify successors and predecessors
			if(i == Mytid)
			    expected_succs.push_back(j);
			if(j == Mytid)
			    expected_preds.push_back(i);
		    }
		}
	    }

	    if(count == 0) { //LP i has no successors
		LpId_t j;
		while((j = random() % noOfLps) == i); //randomly pick one that's different from i
		Manifold :: Connect(comps[i], j, comps[j], i, &MyObj1::handle_incoming, 1);
		if(i == Mytid)
		    expected_succs.push_back(j);
		if(j == Mytid)
		    expected_preds.push_back(i);
	    }
        }

#if 1
DBG_LOG << "preds:  ";
for(unsigned i=0; i<expected_preds.size(); i++) {
DBG_LOG << expected_preds[i] << "  ";
}
DBG_LOG << endl;

DBG_LOG << "succs:  ";
for(unsigned i=0; i<expected_succs.size(); i++) {
DBG_LOG << expected_succs[i] << "  ";
}
DBG_LOG << endl;
#endif


	//verify successors and predecessors
	Scheduler* sch = Manifold :: get_scheduler();

	vector<LpId_t>& preds = sch->get_predecessors();
	vector<LpId_t>& succs = sch->get_successors();

#if 1
DBG_LOG << "actual preds:  ";
for(unsigned i=0; i<preds.size(); i++) {
DBG_LOG << preds[i] << "  ";
}
DBG_LOG << endl;

DBG_LOG << "actual succs:  ";
for(unsigned i=0; i<succs.size(); i++) {
DBG_LOG << succs[i] << "  ";
}
DBG_LOG << endl;
DBG_LOG.flush();
#endif

	CPPUNIT_ASSERT_EQUAL(expected_preds.size(), preds.size());
	CPPUNIT_ASSERT_EQUAL(expected_succs.size(), succs.size());

	for(unsigned i=0; i<expected_preds.size(); i++) {
	    bool found = false;
	    for(unsigned j=0; j<preds.size(); j++) {
	        if(expected_preds[i] == preds[j]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}

	for(unsigned i=0; i<expected_succs.size(); i++) {
	    bool found = false;
	    for(unsigned j=0; j<succs.size(); j++) {
	        if(expected_succs[i] == succs[j]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}


	//each LP gets a different seed
	srandom(Mytid + 1234);

	const int NUM = 10; // NUM numbers are generated.
	Quantum_Scheduler* qsch = dynamic_cast<Quantum_Scheduler*>(Manifold :: get_scheduler());
	assert(qsch);
	assert(qsch->m_quantum_inited);
	unsigned QUANTUM = qsch->m_init_quantum;
cerr << "QUANTUM================== " << QUANTUM << endl;

	//Ticks_t when[NUM+1];
	Ticks_t when[NUM];
	when[0] = random()/(RAND_MAX+1.0) * 10; //a number between 0 and 10
	for(int i=1; i<NUM; i++) {
	    //when[i] = when[i-1] + d;   LOW <= d < HIGH
	    if(QUANTUM >= 5)
		when[i] = when[i-1] + (random()/(RAND_MAX+1.0) * QUANTUM * 0.6 + 1);
	    else
		when[i] = when[i-1] + (random()/(RAND_MAX+1.0) * 6 + 1);
	}

	#if 0
	when[NUM] = when[NUM-1] + 1;

	//create one extra number as stop time. For this last number, all LPs should have the same value.
        Ticks_t last_num[noOfLps];
        //MPI_Allgather(&when[NUM], 1, MPI_LONG, last_num, 1, MPI_LONG, MPI_COMM_WORLD);

	//Allgather for some reason doesn't work as expected; use Gather and Bcast instead.
        MPI_Gather(&when[NUM], 1, MPI_LONG, last_num, 1, MPI_LONG, 0, MPI_COMM_WORLD);
	Ticks_t max=0;

        if(Mytid == 0) {
//for(int i=0; i<noOfLps; i++) {
//DBG_LOG << last_num[i] << "  ";
//}
//DBG_LOG << endl;

	    max = last_num[0];
	    //find the max
	    for(int i=1; i<noOfLps; i++) {
		if(last_num[i] > max)
		    max = last_num[i];
	    }

	    DBG_LOG << "max= " << max << endl;
	}

	MPI_Bcast(&max, 1, MPI_LONG, 0, MPI_COMM_WORLD);

	when[NUM] = max;
	CPPUNIT_ASSERT(when[NUM-1] < when[NUM]);

	DBG_LOG << "last= " << when[NUM] << endl;
	#endif

	for(int i=0; i<NUM; i++) {
	    DBG_LOG << when[i] << " ";
	}
	DBG_LOG << endl;




	//schedule events
	for(int i=0; i<NUM; i++) {
	    Manifold :: Schedule(when[i], &MyObj1::handler0, MyComp);
	}//for

cout << "STOP = " << when[NUM-1] << endl;
	Manifold::StopAt(when[NUM-1]);
	Manifold::Run();

	Manifold :: print_stats(cout);

	std::cout.rdbuf(cout_sbuf);

    }

    /**
     * Build a test suite.
     */
    static CppUnit::Test* suite()
    {
      CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ManifoldTest");

      mySuite->addTest(new CppUnit::TestCaller<ManifoldTest>("test", &ManifoldTest::test));

      return mySuite;
    }
};

Clock ManifoldTest::MasterClock(MASTER_CLOCK_HZ);



int main(int argc, char** argv)
{
    if(argc != 2) {
        cerr << "Usage: mpirun -np <N> " << argv[0] << " <quantum>\n";
	exit(1);
    }

    Manifold :: Init(argc, argv, Manifold::TICKED, SyncAlg::SA_QUANTUM);

    if(TheMessenger.get_node_size() < 2) {
        cerr << "ERROR: Must specify \"-np n (n > 1)\" for mpirun!" << endl;
        return 1;
    }

    unsigned quantum = atoi(argv[1]);
    Quantum_Scheduler* qsch = dynamic_cast<Quantum_Scheduler*>(Manifold :: get_scheduler());
    assert(qsch);

    qsch->init_quantum(quantum);


    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ManifoldTest::suite() );
    runner.run();

    Manifold :: Finalize();

    return 0;
}