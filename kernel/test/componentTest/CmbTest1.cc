//!
//! @brief This program tests the CMB Null-message algorithm. Specifically, there are
//! 2 functions: isSafeToProcess() and send_null_msgs().
//! In this program we don't actually schedule any events. Instead, we create
//! a series of random numbers to simulate event timestamps. The processes
//! don't send messages to each other either.
//!
//! This program can be run with N (N>1) LPs. Each creates an arrary of increasing
//! values representing timestamps. Then it goes through the array and checks
//! if the timestamps are safe to process.
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
#include "scheduler.h"

using namespace std;
using namespace manifold::kernel;

//####################################################################
//####################################################################
class ManifoldTest : public CppUnit::TestFixture {

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }

    //! Test the isSafeToProcess() and send_null_msgs() of CmbSyncAlg.
    //! Each MPI task creates a series of random numbers in increasing 
    //! order to represent timestamps of events.
    //! The first number is randomly generated; the rest are generated by
    //! adding a random interval to the previous number.
    //! Then we go through the series and simulating event processing.
    void test()
    {
        int noOfLps=TheMessenger.get_node_size();
	int Mytid = TheMessenger.get_node_id();

	const double LOOK_AHEAD = random()/(RAND_MAX+1.0) + 1.0; //look-ahead between 1.0 and 2.0

	//each LP gets a different seed
	srandom(Mytid + 1234);

	char buf[10];
	sprintf(buf, "DBG_LOG%d", Mytid);
	ofstream DBG_LOG(buf);


	const int NUM = 10; // NUM numbers are generated.
	double when[NUM+1];
	when[0] = random()/(RAND_MAX+1.0) * 10; //a number between 0 and 10
	for(int i=1; i<NUM; i++) {
	    //when[i] = when[i-1] + d;   LOW <= d < HIGH
	    const int LOW = 1;
	    const int HIGH = 6;
	    when[i] = when[i-1] + (random()/(RAND_MAX+1.0) * (HIGH-LOW) + LOW);
	}
	when[NUM] = when[NUM-1] + 1;


	//create one extra number. For this last number, all LPs should have the same value.
        double last_num[noOfLps];
        //int rc = MPI_Allgather(&when[NUM], 1, MPI_DOUBLE, last_num, 1, MPI_DOUBLE, MPI_COMM_WORLD);

	//Allgather for some reason doesn't work as expected; use Gather and Bcast instead.
        int rc = MPI_Gather(&when[NUM], 1, MPI_DOUBLE, last_num, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	double max=0;

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

	MPI_Bcast(&max, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	when[NUM] = max;
	CPPUNIT_ASSERT(when[NUM-1] < when[NUM]);

	DBG_LOG << "last= " << when[NUM] << endl;

	for(int i=0; i<NUM+1; i++) {
	    DBG_LOG << when[i] << " ";
	}
	DBG_LOG << endl;


	Scheduler* sch = Manifold :: get_scheduler();
	CmbSyncAlg* cmbAlg = dynamic_cast<CmbSyncAlg*>(sch->m_syncAlg);
	CPPUNIT_ASSERT(cmbAlg != 0);
	//cmbAlg->m_neighborVersion = sch->get_neighborVersion() + 1;
	cmbAlg->UpdateLookahead(LOOK_AHEAD);

	for(int i=0; i<noOfLps; i++) {
	    if(i != Mytid) {
	        sch->AddPredecessorLp(i);
	        sch->AddSuccessorLp(i);
	    }
	}

	int tries[NUM+1];
	double normal_tries[NUM+1];

	//go through the numbers.
	for(int i=0; i<NUM+1; i++) {
	    DBG_LOG << "### processing event " << i+1 << " @" << when[i] << endl;
	    int count = 0;
	    while(true) {
	        DBG_LOG << "try " << ++count << ":\n";
		DBG_LOG << "    channel clocks:  ";
		for(CmbSyncAlg::ts_t::iterator it=cmbAlg->m_eits.begin(); it != cmbAlg->m_eits.end(); ++it) {
		    DBG_LOG << it->first << ": " << it->second << "  ";
		}
		DBG_LOG << endl;
		if(cmbAlg->isSafeToProcess(when[i])) {
		    DBG_LOG << "    safe to process: ts= " << when[i] << endl;
		    for(CmbSyncAlg::ts_t::iterator it=cmbAlg->m_eits.begin(); it != cmbAlg->m_eits.end(); ++it) {
			CPPUNIT_ASSERT(when[i] <= it->second);
		    }
		    cmbAlg->send_null_msgs(); //don't forget to send null messages before break
		    break;
		}
		else {
		        DBG_LOG << "    not safe." << endl;
		}
		cmbAlg->send_null_msgs();
	    }//while
	    tries[i] = count;
	    normal_tries[i] = (i==0) ? (count / when[0]) : (count / (when[i]-when[i-1]));
	}//for

	DBG_LOG << " tries:  ";
	for(int i=0; i<NUM+1; i++)
	    DBG_LOG << tries[i] << "  ";
	DBG_LOG << endl;
	DBG_LOG << " normalized tries:  ";
	for(int i=0; i<NUM+1; i++)
	    DBG_LOG << normal_tries[i] << "  ";
	DBG_LOG << endl;

	//cmbAlg->PrintStats(DBG_LOG);

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



int main(int argc, char** argv)
{
    Manifold :: Init(argc, argv, Manifold::TIMED, SyncAlg::SA_CMB, Lookahead::LA_GLOBAL);

    if(TheMessenger.get_node_size() < 2) {
        cerr << "ERROR: Must specify \"-np n (n > 1)\" for mpirun!" << endl;
	return 1;
    }

    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ManifoldTest::suite() );
    runner.run();

    Manifold :: Finalize();

    return 0;
}
