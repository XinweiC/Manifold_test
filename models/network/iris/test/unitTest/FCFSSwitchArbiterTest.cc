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
//! Class FCFSSwitchArbiterTest is the test class for class FCFSSwitchArbiter. 
//####################################################################
class FCFSSwitchArbiterTest : public CppUnit::TestFixture {
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

	    FCFSSwitchArbiter* swa = new FCFSSwitchArbiter(PORTS, VCS);

	    CPPUNIT_ASSERT_EQUAL(PORTS, swa->ports);
	    CPPUNIT_ASSERT_EQUAL(VCS, swa->vcs);

	    for(unsigned i=0; i<PORTS; i++) {
		CPPUNIT_ASSERT_EQUAL(PORTS, (unsigned)swa->m_requesters.size()); //each port has one queue

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
    //! Create a FCFSSwitchArbiter; call request(); verify the corresponding request
    //! bit is turned on, and the request is put in the right queue.
    void test_request_0()
    {
	const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	const unsigned OUT_VC = random() % VCS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC = random() % VCS;

	FCFSSwitchArbiter* swa = new FCFSSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC, IN_PORT, IN_VC);

        //verify the request bit is turned on
	for(unsigned j=0; j<PORTS*VCS; j++) {
	    bool requested = swa->requested[OUT_PORT][j];
	    if(j == IN_PORT*VCS + IN_VC)
		CPPUNIT_ASSERT_EQUAL(true, requested);
	    else
		CPPUNIT_ASSERT_EQUAL(false, requested);
	}
	//verify the request has been enqueued.
	CPPUNIT_ASSERT_EQUAL(IN_PORT*VCS + IN_VC, swa->m_requesters[OUT_PORT].back());

	delete swa;
    }



    //======================================================================
    //======================================================================
    //! @brief Test request()
    //!
    //! Create a FCFSSwitchArbiter; call request() twice for two different input VCs
    //! with same output port. Verify the two requests are enqueued in the correct order.
    void test_request_1()
    {
	const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	const unsigned OUT_VC = random() % VCS; //OUT_VC is useless
	unsigned IN_PORT1, IN_PORT2;
	while((IN_PORT1 = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	while((IN_PORT2 = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC1 = random() % VCS;
	const unsigned IN_VC2 = random() % VCS;

	FCFSSwitchArbiter* swa = new FCFSSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC, IN_PORT1, IN_VC1);
	swa->request(OUT_PORT, OUT_VC, IN_PORT2, IN_VC2);

	//verify the requests enqueued in correct order.
	CPPUNIT_ASSERT_EQUAL(IN_PORT1*VCS + IN_VC1, swa->m_requesters[OUT_PORT].front());
	CPPUNIT_ASSERT_EQUAL(IN_PORT2*VCS + IN_VC2, swa->m_requesters[OUT_PORT].back());

	delete swa;
    }



    
    
    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a FCFSSwitchArbiter; make a request; call pick_winner(); verify
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

	FCFSSwitchArbiter* swa = new FCFSSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC, IN_PORT, IN_VC);

        //verify the request bit is turned on; the request queue has one item 
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT*VCS + IN_VC]);
	CPPUNIT_ASSERT_EQUAL(1, (int)swa->m_requesters[OUT_PORT].size());


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

        //verify winner has been poped from the queue
	CPPUNIT_ASSERT_EQUAL(0, (int)swa->m_requesters[OUT_PORT].size());


	//verify the request bit stays on; it's only turned off when the VC goes back to
	//VCA_COMPLETE, or when the whole packet has traversed the router.
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT*VCS + IN_VC]);


	delete swa;
    } 

    

    //======================================================================
    //======================================================================
    //! @brief Test pick_winner(): verify FCFS priority
    //!
    //! Create a FCFSSwitchArbiter; make 2 requests from same input port but different
    //! VCs; call pick_winner() 2 times; verify the requestors are picked FCFS.
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

	FCFSSwitchArbiter* swa = new FCFSSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC1, IN_PORT, IN_VC1);
	swa->request(OUT_PORT, OUT_VC2, IN_PORT, IN_VC2);

        //verify the request bit is turned on
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT*VCS + IN_VC1]);
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT*VCS + IN_VC2]);


	//call pick_winner() 2times
	SA_unit winner1 = *(swa->pick_winner(OUT_PORT));
	SA_unit winner2 = *(swa->pick_winner(OUT_PORT));

	//verify winners are picked FCFS
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winner1.port);
	CPPUNIT_ASSERT_EQUAL(IN_VC1, winner1.ch);
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winner2.port);
	CPPUNIT_ASSERT_EQUAL(IN_VC2, winner2.ch);


	delete swa;
    } 

    



    //======================================================================
    //======================================================================
    //! @brief Test pick_winner(): verify FCFS priority.
    //!
    //! Create a FCFSSwitchArbiter; make 2 requests from 2 different input ports;
    //! call pick_winner() 2 times; verify the requestors are picked round-robin.
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

	FCFSSwitchArbiter* swa = new FCFSSwitchArbiter(PORTS, VCS);

	swa->request(OUT_PORT, OUT_VC1, IN_PORT1, IN_VC1);
	swa->request(OUT_PORT, OUT_VC2, IN_PORT2, IN_VC2);

        //verify the request bit is turned on
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT1*VCS + IN_VC1]);
	CPPUNIT_ASSERT_EQUAL(true, (bool)swa->requested[OUT_PORT][IN_PORT2*VCS + IN_VC2]);

	//call pick_winner() 2times
	SA_unit winner1 = *(swa->pick_winner(OUT_PORT));
	SA_unit winner2 = *(swa->pick_winner(OUT_PORT));

	//verify winners are picked FCFS
	CPPUNIT_ASSERT_EQUAL(IN_PORT1, winner1.port);
	CPPUNIT_ASSERT_EQUAL(IN_VC1, winner1.ch);
	CPPUNIT_ASSERT_EQUAL(IN_PORT2, winner2.port);
	CPPUNIT_ASSERT_EQUAL(IN_VC2, winner2.ch);



	delete swa;
    } 





    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("FCFSSwitchArbiterTest");

	mySuite->addTest(new CppUnit::TestCaller<FCFSSwitchArbiterTest>("test_Constructor_0", &FCFSSwitchArbiterTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<FCFSSwitchArbiterTest>("test_request_0", &FCFSSwitchArbiterTest::test_request_0));
	mySuite->addTest(new CppUnit::TestCaller<FCFSSwitchArbiterTest>("test_request_1", &FCFSSwitchArbiterTest::test_request_1));
	mySuite->addTest(new CppUnit::TestCaller<FCFSSwitchArbiterTest>("test_pick_winner_0", &FCFSSwitchArbiterTest::test_pick_winner_0));
	mySuite->addTest(new CppUnit::TestCaller<FCFSSwitchArbiterTest>("test_pick_winner_1", &FCFSSwitchArbiterTest::test_pick_winner_1));
	mySuite->addTest(new CppUnit::TestCaller<FCFSSwitchArbiterTest>("test_pick_winner_2", &FCFSSwitchArbiterTest::test_pick_winner_2));
	return mySuite;
    }
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( FCFSSwitchArbiterTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


