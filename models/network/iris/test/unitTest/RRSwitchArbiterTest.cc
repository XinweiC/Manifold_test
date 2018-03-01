#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <vector>
#include <stdlib.h>
#include "../../data_types/flit.h"
#include "../../components/genericSwitchArbiter.h"

#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;
using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################



//####################################################################
//! Class RRSwitchArbiterTest is the test class for class RRSwitchArbiter. 
//####################################################################
class RRSwitchArbiterTest : public CppUnit::TestFixture {
private:

public:
    
    //======================================================================
    //======================================================================
    //! @brief Test constructor.
    //!
    void test_Constructor_0()
    {
        const int N=100;

	for(int i=0; i<N; i++) {
	    unsigned VCS = random() % 10 + 1; //1 to 10 virtual channels
	    unsigned PORTS = random() % 9 + 2; //2 to 10 ports

	    RRSwitchArbiter* swa = new RRSwitchArbiter(PORTS, VCS);

	    CPPUNIT_ASSERT_EQUAL(PORTS, swa->ports);
	    CPPUNIT_ASSERT_EQUAL(VCS, swa->vcs);

	    for(unsigned i=0; i<PORTS; i++) {
		CPPUNIT_ASSERT_EQUAL(-1, (int)swa->last_port_winner[i]);

		for(unsigned j=0; j<PORTS*VCS; j++) {
		    CPPUNIT_ASSERT_EQUAL(false, (bool)swa->requested[i][j]);
		}
	    }
	    delete swa;
	}
    }



    //======================================================================
    //======================================================================
    //! @brief Test request()
    //!
    //! Create a RRSwitchArbiter; call request(); verify the corresponding request
    //! bit is turned on.
    void test_request_0()
    {
	const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	const unsigned OUT_VC = random() % VCS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC = random() % VCS;

	RRSwitchArbiter* swa = new RRSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC, IN_PORT, IN_VC);

        //verify the request bit is turned on
	for(unsigned j=0; j<PORTS*VCS; j++) {
	    bool requested = swa->requested[OUT_PORT][j];
	    if(j == IN_PORT*VCS + IN_VC)
		CPPUNIT_ASSERT_EQUAL(true, requested);
	    else
		CPPUNIT_ASSERT_EQUAL(false, requested);
	}
	delete swa;
    }



    
    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a RRSwitchArbiter; make a request; call pick_winner(); verify
    //! the requestor is picked.
    void test_pick_winner_0()
    {
	const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	const unsigned OUT_VC = random() % VCS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC = random() % VCS;

	RRSwitchArbiter* swa = new RRSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC, IN_PORT, IN_VC);

        //verify the request bit is turned on
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT*VCS + IN_VC]);


	//call pick_winner()
	//If the port is not OUT_PORT, there won't be a winner.
	for(unsigned i=0; i<PORTS; i++) {
	    if(i != OUT_PORT)
	    CPPUNIT_ASSERT(0 == swa->pick_winner(i));
	}

	const SA_unit* winner = swa->pick_winner(OUT_PORT);

	//verify winner info
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winner->port);
	CPPUNIT_ASSERT_EQUAL(IN_VC, winner->ch);

        //verify last_port_winner[OUT_PORT] is set to the requesting VC
	CPPUNIT_ASSERT_EQUAL(IN_PORT*VCS + IN_VC, swa->last_port_winner[OUT_PORT]);


	//verify the request bit stays on; it's only turned off when the VC goes back to
	//VCA_COMPLETE, or when the whole packet has traversed the router.
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT*VCS + IN_VC]);


	delete swa;
    } 

    

    //======================================================================
    //======================================================================
    //! @brief Test pick_winner(): verify round-robin priority.
    //!
    //! Create a RRSwitchArbiter; make 2 requests from same input port but different
    //! VCs; call pick_winner() N times; verify the requestors are picked round-robin.
    void test_pick_winner_1()
    {
	const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	const unsigned OUT_VC1 = random() % VCS;
	const unsigned OUT_VC2 = random() % VCS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC1 = random() % VCS;
	unsigned IN_VC2;
	while((IN_VC2 = random() % VCS) == IN_VC1); //in_vc2 must be different from in_vc1.

	RRSwitchArbiter* swa = new RRSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC1, IN_PORT, IN_VC1);
	swa->request(OUT_PORT, OUT_VC2, IN_PORT, IN_VC2);

        //verify the request bit is turned on
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT*VCS + IN_VC1]);
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT*VCS + IN_VC2]);


	//call pick_winner() N times
	const int N = 100;

	SA_unit winners[N];

	for(int i=0; i<N; i++) {
	    winners[i] = *(swa->pick_winner(OUT_PORT));

	    //verify last_port_winner[OUT_PORT]
	    if(IN_VC1 < IN_VC2) {
		if(i % 2 == 0)
		    CPPUNIT_ASSERT_EQUAL(IN_PORT*VCS + IN_VC1, swa->last_port_winner[OUT_PORT]);
		else
		    CPPUNIT_ASSERT_EQUAL(IN_PORT*VCS + IN_VC2, swa->last_port_winner[OUT_PORT]);
	    }
	    else {
		if(i % 2 == 0)
		    CPPUNIT_ASSERT_EQUAL(IN_PORT*VCS + IN_VC2, swa->last_port_winner[OUT_PORT]);
		else
		    CPPUNIT_ASSERT_EQUAL(IN_PORT*VCS + IN_VC1, swa->last_port_winner[OUT_PORT]);
	    }
	}


	//verify winners are picked round-robin
	for(int i=2; i<N; i++) {
	    if(i % 2 == 0) {
		CPPUNIT_ASSERT_EQUAL(winners[0].port, winners[i].port);
		CPPUNIT_ASSERT_EQUAL(winners[0].ch, winners[i].ch);
	    }
	    else {
		CPPUNIT_ASSERT_EQUAL(winners[1].port, winners[i].port);
		CPPUNIT_ASSERT_EQUAL(winners[1].ch, winners[i].ch);
	    }
	}


	delete swa;
    } 

    



    //======================================================================
    //======================================================================
    //! @brief Test pick_winner(): verify round-robin priority.
    //!
    //! Create a RRSwitchArbiter; make 2 requests from 2 different input ports;
    //! call pick_winner() N times; verify the requestors are picked round-robin.
    void test_pick_winner_2()
    {
	const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	const unsigned OUT_VC1 = random() % VCS;
	const unsigned OUT_VC2 = random() % VCS;
	unsigned IN_PORT1;
	while((IN_PORT1 = random() % PORTS) == OUT_PORT); //in_port1 must be different from out_port.
	unsigned IN_PORT2;
	while((IN_PORT2 = random() % PORTS) == OUT_PORT || IN_PORT2 == IN_PORT1); //in_port2 must be different from out_port and in_port1.
	const unsigned IN_VC1 = random() % VCS;
	const unsigned IN_VC2 = random() % VCS;

	RRSwitchArbiter* swa = new RRSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC1, IN_PORT1, IN_VC1);
	swa->request(OUT_PORT, OUT_VC2, IN_PORT2, IN_VC2);

        //verify the request bit is turned on
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT1*VCS + IN_VC1]);
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT2*VCS + IN_VC2]);


	//call pick_winner() N times
	const int N = 100;

	SA_unit winners[N];

	for(int i=0; i<N; i++) {
	    winners[i] = *(swa->pick_winner(OUT_PORT));

	    //verify last_port_winner[OUT_PORT]
	    if(IN_PORT1*VCS + IN_VC1 < IN_PORT2*VCS + IN_VC2) {
		if(i % 2 == 0)
		    CPPUNIT_ASSERT_EQUAL(IN_PORT1*VCS + IN_VC1, swa->last_port_winner[OUT_PORT]);
		else
		    CPPUNIT_ASSERT_EQUAL(IN_PORT2*VCS + IN_VC2, swa->last_port_winner[OUT_PORT]);
	    }
	    else {
		if(i % 2 == 0)
		    CPPUNIT_ASSERT_EQUAL(IN_PORT2*VCS + IN_VC2, swa->last_port_winner[OUT_PORT]);
		else
		    CPPUNIT_ASSERT_EQUAL(IN_PORT1*VCS + IN_VC1, swa->last_port_winner[OUT_PORT]);
	    }
	}


	//verify winners are picked round-robin
	for(int i=2; i<N; i++) {
	    if(i % 2 == 0) {
		CPPUNIT_ASSERT_EQUAL(winners[0].port, winners[i].port);
		CPPUNIT_ASSERT_EQUAL(winners[0].ch, winners[i].ch);
	    }
	    else {
		CPPUNIT_ASSERT_EQUAL(winners[1].port, winners[i].port);
		CPPUNIT_ASSERT_EQUAL(winners[1].ch, winners[i].ch);
	    }
	}


	delete swa;
    } 

    




    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("RRSwitchArbiterTest");

	mySuite->addTest(new CppUnit::TestCaller<RRSwitchArbiterTest>("test_Constructor_0", &RRSwitchArbiterTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<RRSwitchArbiterTest>("test_request_0", &RRSwitchArbiterTest::test_request_0));
	mySuite->addTest(new CppUnit::TestCaller<RRSwitchArbiterTest>("test_pick_winner_0", &RRSwitchArbiterTest::test_pick_winner_0));
	mySuite->addTest(new CppUnit::TestCaller<RRSwitchArbiterTest>("test_pick_winner_1", &RRSwitchArbiterTest::test_pick_winner_1));
	mySuite->addTest(new CppUnit::TestCaller<RRSwitchArbiterTest>("test_pick_winner_2", &RRSwitchArbiterTest::test_pick_winner_2));
	return mySuite;
    }
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( RRSwitchArbiterTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


