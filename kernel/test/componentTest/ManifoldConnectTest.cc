//!
//! @brief This program tests Manifold :: Connect() to verify the predecessor LPs and
//! successor LPs are correctly set.
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
#include "manifold.h"
#include "component.h"
#include "messenger.h"

using namespace std;
using namespace manifold::kernel;


//####################################################################
// Helper classes
//####################################################################
class A
{};

class Comp : public Component {
public:
    void handler(int, A*)
    {}
};


//####################################################################
//####################################################################
class ConnectTest : public CppUnit::TestFixture {
private:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

public:
    //==========================================================================
    //==========================================================================
    //! @brief Test Manifold :: Connect()
    //!
    //! Randomly pick 2 LPs and create one component in each LP. Verify the predecessors
    //! and successors.
    void test_connect_0()
    {
	int Mytid; //task id
	MPI_Comm_rank(MPI_COMM_WORLD, &Mytid);

	const int N_nodes = TheMessenger.get_node_size();

	int lp0 = random() % N_nodes;
	int lp1;
	while((lp1 = random() % N_nodes) == lp0); //make sure lp1 != lp0

	//cout << "lp0 = " << lp0 << "  lp1= " << lp1 << endl;

	CompId_t cid0 = Component :: Create<Comp>(lp0);
	CompId_t cid1 = Component :: Create<Comp>(lp1);

	Manifold :: Connect(cid0, 0, cid1, 0, &Comp :: handler, 1);


	Scheduler* theSch = Manifold :: get_scheduler();
	std::vector<LpId_t> preds = theSch->get_predecessors();
	std::vector<LpId_t> succs = theSch->get_successors();

	bool lp0_is_pred = false;
	for(unsigned i=0; i<preds.size(); i++) {
	    if(preds[i] == lp0) {
		lp0_is_pred = true;
		break;
	    }
	}
	bool lp1_is_succ = false;
	for(unsigned i=0; i<succs.size(); i++) {
	    if(succs[i] == lp1) {
		lp1_is_succ = true;
		break;
	    }
	}

	if(Mytid == lp0) {
	    CPPUNIT_ASSERT_EQUAL(false, lp0_is_pred);
	    CPPUNIT_ASSERT_EQUAL(true, lp1_is_succ);
	}
	else if(Mytid == lp1) {
	    CPPUNIT_ASSERT_EQUAL(true, lp0_is_pred);
	    CPPUNIT_ASSERT_EQUAL(false, lp1_is_succ);
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(false, lp0_is_pred);
	    CPPUNIT_ASSERT_EQUAL(false, lp1_is_succ);
	}

    }



    //==========================================================================
    //==========================================================================
    //! @brief Test Manifold :: Connect(): successors.
    //!
    //! Create a component C in a random LP. Then create N components on random LPs.
    //! Connect C to each of the N components. Verify C's successors set is correct.
    void test_connect_1()
    {
	int Mytid; //task id
	MPI_Comm_rank(MPI_COMM_WORLD, &Mytid);

	const int N_nodes = TheMessenger.get_node_size();

	LpId_t lp0 = random() % N_nodes;

	//cout << "lp0 = " << lp0 << endl;

	std::set<LpId_t> lp_set; //use a set so same LPs only appear once
	lp_set.insert(lp0);

	CompId_t cid0 = Component :: Create<Comp>(lp0);

	const int N = 20; //create N more components
	for(int i=0; i<N; i++) {
	    LpId_t lp = random() % N_nodes;
	    lp_set.insert(lp);

	    CompId_t cid = Component :: Create<Comp>(lp);
	    //connect
	    Manifold :: Connect(cid0, 0, cid, 0, &Comp :: handler, 1);
	}




	Scheduler* theSch = Manifold :: get_scheduler();
	std::vector<LpId_t> preds = theSch->get_predecessors();
	std::vector<LpId_t> succs = theSch->get_successors();

	if(Mytid == lp0) {
	    CPPUNIT_ASSERT_EQUAL(lp_set.size()-1, succs.size());
	    for(std::set<LpId_t>::iterator it = lp_set.begin(); it != lp_set.end(); ++it) {
	        if(lp0 != *it) {
		    // *it must be one of the successors.
		    bool found = false;
		    for(unsigned i=0; i<succs.size(); i++) {
			if(succs[i] == *it) {
			    found = true;
			    break;
			}
		    }
		    CPPUNIT_ASSERT_EQUAL(true, found);
		}
	    }
	}
	else {
	}

    }



    //==========================================================================
    //==========================================================================
    //! @brief Test Manifold :: Connect(): predecessors.
    //!
    //! Create a component C in a random LP. Then create N components on random LPs.
    //! Connect each of the N components to C. Verify C's predecessors set is correct.
    void test_connect_2()
    {
	int Mytid; //task id
	MPI_Comm_rank(MPI_COMM_WORLD, &Mytid);

	const int N_nodes = TheMessenger.get_node_size();

	LpId_t lp0 = random() % N_nodes;

	//cout << "lp0 = " << lp0 << endl;

	std::set<LpId_t> lp_set; //use a set so same LPs only appear once
	lp_set.insert(lp0);

	CompId_t cid0 = Component :: Create<Comp>(lp0);

	const int N = 20; //create N more components
	for(int i=0; i<N; i++) {
	    LpId_t lp = random() % N_nodes;
	    lp_set.insert(lp);

	    CompId_t cid = Component :: Create<Comp>(lp);
	    //connect
	    Manifold :: Connect(cid, 0, cid0, i, &Comp :: handler, 1);
	}




	Scheduler* theSch = Manifold :: get_scheduler();
	std::vector<LpId_t> preds = theSch->get_predecessors();
	std::vector<LpId_t> succs = theSch->get_successors();

	if(Mytid == lp0) {
	    CPPUNIT_ASSERT_EQUAL(lp_set.size()-1, preds.size());
	    for(std::set<LpId_t>::iterator it = lp_set.begin(); it != lp_set.end(); ++it) {
	        if(lp0 != *it) {
		    // *it must be one of the predecessors.
		    bool found = false;
		    for(unsigned i=0; i<preds.size(); i++) {
			if(preds[i] == *it) {
			    found = true;
			    break;
			}
		    }
		    CPPUNIT_ASSERT_EQUAL(true, found);
		}
	    }
	}
	else {
	}

    }



    /**
     * Build a test suite.
     */
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ConnectTest");

	mySuite->addTest(new CppUnit::TestCaller<ConnectTest>("test_connect_0", &ConnectTest::test_connect_0));
	mySuite->addTest(new CppUnit::TestCaller<ConnectTest>("test_connect_1", &ConnectTest::test_connect_1));
	mySuite->addTest(new CppUnit::TestCaller<ConnectTest>("test_connect_2", &ConnectTest::test_connect_2));

	return mySuite;
    }
};


Clock ConnectTest::MasterClock(MASTER_CLOCK_HZ);


int main(int argc, char** argv)
{
    Manifold :: Init(argc, argv, Manifold::TICKED, SyncAlg::SA_CMB);
    if(2 > TheMessenger.get_node_size()) {
        cerr << "ERROR: Must specify \"-np N (N>1)\" for mpirun!" << endl;
	return 1;
    }

    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ConnectTest::suite() );
    runner.run();

    Manifold :: Finalize();

    return 0;
}

