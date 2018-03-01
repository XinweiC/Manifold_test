//! This program tests simple_mc as a component. We use a Mock
//! component and connect the simple_mc to the component.
//!
//!                   ------------
//!                  | MockSender |
//!                   ------------
//!                         |
//!                  --------------
//!                 | simple_cache |
//!                  --------------

#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <vector>
#include <stdlib.h>
#include "simple_mc.h"

#include "kernel/manifold-decl.h"
#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;
using namespace manifold::simple_mc;
using namespace manifold::simple_cache;

//####################################################################
// helper classes
//####################################################################

//! This class emulates a component that sends requests to the MC.
class MockSender : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};

    void send_mreq(mem_req* mreq)
    {
	Send(OUT, mreq);
    }

    void handle_incoming(int, mem_req* mresp)
    {
        m_mc_resps.push_back(*mresp);
        m_ticks.push_back(Manifold :: NowTicks());
	delete mresp;
    }
    vector<mem_req>& get_mc_resps() { return m_mc_resps; }
    vector<Ticks_t>& get_ticks() { return m_ticks; }
private:
    vector<mem_req> m_mc_resps;
    vector<Ticks_t> m_ticks;
};



//####################################################################
//! Class simple_mcTest is the test class for class simple_mc. 
//####################################################################
class simple_mcTest : public CppUnit::TestFixture {
private:

    //latencies
    static const Ticks_t SENDER_TO_MC = 1;

    MockSender* m_senderp;
    SimpleMC* m_mcp;

    void mySetup(int nid, Ticks_t lat, bool st_resp)
    {
        //create a MockSender, and a SimpleMC
	CompId_t senderId = Component :: Create<MockSender>(0);
	m_senderp = Component::GetComponent<MockSender>(senderId);

	CompId_t mcId = Component :: Create<SimpleMC>(0, nid, lat, st_resp);
	m_mcp = Component::GetComponent<SimpleMC>(mcId);

        //connect the components
	//sender to mc
	Manifold::Connect(senderId, MockSender::OUT, mcId, SimpleMC::IN,
			  &SimpleMC::handle_incoming, SENDER_TO_MC);
	//mc to sender
	Manifold::Connect(mcId, SimpleMC::OUT, senderId, MockSender::IN,
			  &MockSender::handle_incoming, SENDER_TO_MC);
    }


public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }



    //======================================================================
    //======================================================================
    //! @brief Test handle_incoming with a single load.
    //!
    //! Create a SimpleMC that sends NO response for stores. Send a single LOAD.
    //! Verify the response and its timing.
    void test_handle_incoming_0()
    {
        int nid = random() % 1024;  //node id for the mc
	Ticks_t mc_latency = random() % 200 + 50;
        mySetup(nid, mc_latency, false);

	//create a LOAD request
	paddr_t addr = random();
	int req_id = random();
	mem_req* mreq = new mem_req(req_id, 123, addr, OpMemLd, INITIAL);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockSender to send the mem_req
	Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, mreq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Ticks_t rtt = SENDER_TO_MC +  mc_latency; //round trip time

	Manifold::StopAt(scheduledAt + rtt + 10);
	Manifold::Run();

        //verify sender got a response
	vector<mem_req>& mresps = m_senderp->get_mc_resps();
        vector<Ticks_t>& ticks = m_senderp->get_ticks();
	mem_req mresp = mresps[0];

	CPPUNIT_ASSERT_EQUAL(1, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(req_id, mresp.req_id);
	CPPUNIT_ASSERT_EQUAL(addr, mresp.addr);
	CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, mresp.msg);
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SENDER_TO_MC+mc_latency, ticks[0]);
    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_incoming with a single store.
    //!
    //! Create a SimpleMC that sends NO response for stores. Send a single STORE.
    //! Verify no response is received.
    void test_handle_incoming_1()
    {
        int nid = random() % 1024;  //node id for the mc
	Ticks_t mc_latency = random() % 200 + 50;
        mySetup(nid, mc_latency, false);

	//create a STORE request
	paddr_t addr = random();
	int req_id = random();
	mem_req* mreq = new mem_req(req_id, 123, addr, OpMemSt, INITIAL);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockSender to send the mem_req
	Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, mreq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Ticks_t rtt = SENDER_TO_MC +  mc_latency; //round trip time

	Manifold::StopAt(scheduledAt + rtt + 10);
	Manifold::Run();

        //verify sender got no response
	vector<mem_req>& mresps = m_senderp->get_mc_resps();
        vector<Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(0, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(0, (int)ticks.size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_incoming with N random LOADs and STOREs.
    //!
    //! Create a SimpleMC that sends NO response for stores. Send N random LOADs
    //! and STOREs. Verify the responses and their timing.
    void test_handle_incoming_2()
    {
        int nid = random() % 1024;  //node id for the mc
	Ticks_t mc_latency = random() % 200 + 50;
        mySetup(nid, mc_latency, false);

	//create Nreqs LOAD request
	const int Nreqs = 1000;

	mem_req* reqs[Nreqs];
	mem_req reqs_copy[Nreqs]; //make a copy for verification; reqs will be deleted
	                          //by sender.
        int num_loads = 0;
        for(int i=0; i<Nreqs; i++) {
	    paddr_t addr = random();
	    int req_id = random();
	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(req_id, 123, addr, OpMemLd, INITIAL);
		num_loads++;
	    }
	    else {
		reqs[i] = new mem_req(req_id, 123, addr, OpMemSt, INITIAL);
	    }
	    reqs_copy[i] = *reqs[i];
	}

	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs];

	Ticks_t rtt = SENDER_TO_MC +  mc_latency; //round trip time
	for(int i=0; i<Nreqs; i++) {
	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, reqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	    When += rtt + 10;
	}


	Manifold::StopAt(When);
	Manifold::Run();

        //verify sender got a response
	vector<mem_req>& mresps = m_senderp->get_mc_resps();
        vector<Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_loads, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(num_loads, (int)ticks.size());

        int idx=0;
        for(int i=0; i<Nreqs; i++) {
	    if(reqs_copy[i].op_type == OpMemLd) {
		CPPUNIT_ASSERT_EQUAL(reqs_copy[i].req_id, mresps[idx].req_id);
		CPPUNIT_ASSERT_EQUAL(reqs_copy[i].addr, mresps[idx].addr);
		CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SENDER_TO_MC+mc_latency, ticks[idx]);
		idx++;
	    }
	}
    }






    //======================================================================
    //======================================================================
    //! @brief Test handle_incoming with a single load.
    //!
    //! Create a SimpleMC that sends response for stores. Send a single LOAD.
    //! Verify the response and its timing.
    void test_handle_incoming_r0()
    {
        int nid = random() % 1024;  //node id for the mc
	Ticks_t mc_latency = random() % 200 + 50;
        mySetup(nid, mc_latency, true); // send_st_response set to true

	//create a LOAD request
	paddr_t addr = random();
	int req_id = random();
	mem_req* mreq = new mem_req(req_id, 123, addr, OpMemLd, INITIAL);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockSender to send the mem_req
	Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, mreq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Ticks_t rtt = SENDER_TO_MC +  mc_latency; //round trip time

	Manifold::StopAt(scheduledAt + rtt + 10);
	Manifold::Run();

        //verify sender got a response
	vector<mem_req>& mresps = m_senderp->get_mc_resps();
        vector<Ticks_t>& ticks = m_senderp->get_ticks();
	mem_req mresp = mresps[0];

	CPPUNIT_ASSERT_EQUAL(1, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(req_id, mresp.req_id);
	CPPUNIT_ASSERT_EQUAL(addr, mresp.addr);
	CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, mresp.msg);
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SENDER_TO_MC+mc_latency, ticks[0]);
    }



    //======================================================================
    //======================================================================
    //! @brief Test handle_incoming with a single store.
    //!
    //! Create a SimpleMC that sends response for stores. Send a single STORE.
    //! Verify a response is received; also verify timing.
    void test_handle_incoming_r1()
    {
        int nid = random() % 1024;  //node id for the mc
	Ticks_t mc_latency = random() % 200 + 50;
        mySetup(nid, mc_latency, true); // send_st_response set to true

	//create a STORE request
	paddr_t addr = random();
	int req_id = random();
	mem_req* mreq = new mem_req(req_id, 123, addr, OpMemSt, INITIAL);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockSender to send the mem_req
	Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, mreq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Ticks_t rtt = SENDER_TO_MC +  mc_latency; //round trip time

	Manifold::StopAt(scheduledAt + rtt + 10);
	Manifold::Run();

        //verify sender got no response
	vector<mem_req>& mresps = m_senderp->get_mc_resps();
        vector<Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(1, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(1, (int)ticks.size());
	mem_req mresp = mresps[0];

	CPPUNIT_ASSERT_EQUAL(req_id, mresp.req_id);
	CPPUNIT_ASSERT_EQUAL(addr, mresp.addr);
	CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, mresp.msg);
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SENDER_TO_MC+mc_latency, ticks[0]);
    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_incoming with N random LOADs and STOREs.
    //!
    //! Create a SimpleMC that sends response for stores. Send N random LOADs
    //! and STOREs. Verify the responses and their timing.
    void test_handle_incoming_r2()
    {
        int nid = random() % 1024;  //node id for the mc
	Ticks_t mc_latency = random() % 200 + 50;
        mySetup(nid, mc_latency, true);

	//create Nreqs LOAD request
	const int Nreqs = 1000;

	mem_req* reqs[Nreqs];
	mem_req reqs_copy[Nreqs]; //make a copy for verification; reqs will be deleted
	                          //by sender.
        int num_loads = 0;
        for(int i=0; i<Nreqs; i++) {
	    paddr_t addr = random();
	    int req_id = random();
	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(req_id, 123, addr, OpMemLd, INITIAL);
		num_loads++;
	    }
	    else {
		reqs[i] = new mem_req(req_id, 123, addr, OpMemSt, INITIAL);
	    }
	    reqs_copy[i] = *reqs[i];
	}

	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs];

	Ticks_t rtt = SENDER_TO_MC +  mc_latency; //round trip time
	for(int i=0; i<Nreqs; i++) {
	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, reqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	    When += rtt + 10;
	}


	Manifold::StopAt(When);
	Manifold::Run();

        //verify sender got a response
	vector<mem_req>& mresps = m_senderp->get_mc_resps();
        vector<Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(Nreqs, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(Nreqs, (int)ticks.size());

        for(int i=0; i<Nreqs; i++) {
	    CPPUNIT_ASSERT_EQUAL(reqs_copy[i].req_id, mresps[i].req_id);
	    CPPUNIT_ASSERT_EQUAL(reqs_copy[i].addr, mresps[i].addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SENDER_TO_MC+mc_latency, ticks[i]);
	    if(reqs_copy[i].op_type == OpMemLd)
		CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, mresps[i].msg);
	    else
		CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, mresps[i].msg);
	}
    }







    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("simple_mcTest");

	mySuite->addTest(new CppUnit::TestCaller<simple_mcTest>("test_handle_incoming_0", &simple_mcTest::test_handle_incoming_0));
	mySuite->addTest(new CppUnit::TestCaller<simple_mcTest>("test_handle_incoming_1", &simple_mcTest::test_handle_incoming_1));
	mySuite->addTest(new CppUnit::TestCaller<simple_mcTest>("test_handle_incoming_2", &simple_mcTest::test_handle_incoming_2));
	mySuite->addTest(new CppUnit::TestCaller<simple_mcTest>("test_handle_incoming_r0", &simple_mcTest::test_handle_incoming_r0));
	mySuite->addTest(new CppUnit::TestCaller<simple_mcTest>("test_handle_incoming_r1", &simple_mcTest::test_handle_incoming_r1));
	mySuite->addTest(new CppUnit::TestCaller<simple_mcTest>("test_handle_incoming_r2", &simple_mcTest::test_handle_incoming_r2));

	return mySuite;
    }
};


Clock simple_mcTest :: MasterClock(simple_mcTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( simple_mcTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


