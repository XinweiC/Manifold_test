#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include <sys/time.h>
//#include "interfaceCreator.h"
//#include "network.h"
#include "router.h"

#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;

using namespace manifold::simple_net;

//####################################################################
// helper classes
//####################################################################

/*
class TerminalData {
public:
    int src;
    int dst;
    int get_src() { return src; }
    int get_dst() { return dst; }
};

//! This class emulates a terminal
class MockTerminal : public manifold::kernel::Component {
public:
    enum {IN=0};

    void handle_incoming(int, TerminalData* pkt)
    {
        m_pkts.push_back(*pkt);
        m_ticks.push_back(Manifold :: NowTicks());
	delete pkt;
    }

    list<TerminalData>& get_pkts() { return m_pkts; }
    list<Ticks_t>& get_ticks() { return m_ticks; }
private:
    list<TerminalData> m_pkts;
    list<Ticks_t> m_ticks;
};


// The handle_terminal() function requires a network object, but I don't
// want to create network objects such as MeshNetwork which creates routers
// and register with clock, etc. So this dummy network class is created.
class DummyNetwork : public SimpleNetwork {
public:
    DummyNetwork(int n, manifold::kernel::LpId_t lp, InterfaceCreator* ic, Terminal_to_net_mapping* m) :
	SimpleNetwork(n, lp, ic, m) {}
};


*/



//! This class emulates a router
class MockRouter : public manifold::kernel::Component {
public:
    enum {IN=0};
    void handle_incoming(int, NetworkPkt* pkt)
    {
        if(pkt->type == NetworkPkt::DATA) {
	    m_pkts.push_back(*pkt);
	    m_ticks.push_back(Manifold :: NowTicks());
	}
	else
	    m_credits.push_back(*pkt);

	delete pkt;
    }

    list<NetworkPkt>& get_pkts() { return m_pkts; }
    list<NetworkPkt>& get_credits() { return m_credits; }
    list<Ticks_t>& get_ticks() { return m_ticks; }
private:
    list<NetworkPkt> m_pkts;
    list<Ticks_t> m_ticks;
    list<NetworkPkt> m_credits;
};


void test_rising_edge_helper(RingRouter* router)
{
    router->rising_edge();
}


void test_rising_edge_helper_schedule_ni(RingRouter* router, NetworkPkt* pkt)
{
    router->rising_edge();
    router->handle_interface(RingRouter::IN_NI, pkt);
}


//####################################################################
//####################################################################
class RingRouterTest : public CppUnit::TestFixture {
private:

public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }



    //======================================================================
    //======================================================================
    //! @brief Verify ID is correctly initialized.
    //!
    //! Create N RingRouter objects with random IDs; verify the ID
    //! is what's in the constructor.
    void testConstructor_0()
    {
        for(int i=0; i<1000; i++) {
	    int id = random() % 1000;
	    RingRouter* r = new RingRouter(id, 1);

	    CPPUNIT_ASSERT_EQUAL(id, r->get_id());
	    delete r;
	}

    }




    //======================================================================
    //======================================================================
    //! @brief Verify x, y, credits are correctly initialized.
    void testConstructor_1()
    {
        for(int i=0; i<1000; i++) {
	    int credits = random() % 1000;
	    
	    RingRouter* r = new RingRouter(0, credits);

	    CPPUNIT_ASSERT_EQUAL(credits, r->m_CREDITS);
	    for(int j=0; j<2; j++)
		CPPUNIT_ASSERT_EQUAL(credits, r->m_credits[j]);
	    delete r;
	}

    }


    //======================================================================
    //======================================================================
    //! @brief Verify handle_interface() puts packets in input buffer and in correct order.
    //!
    //! Create a RingRouter; call handle_interface() twice; verify the
    //! packets are put in the right input buffer in the right order. Repeat N times.
    void testHandle_interface_0()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100 + 2; //at least 2
	    
	    const int ID = 1;

	    //create a RingRouter
	    RingRouter* router = new RingRouter(ID, bufsize);

	    NetworkPkt* pkts[2];

	    for(int p=0; p<2; p++) {
	        pkts[p] = new NetworkPkt;
		pkts[p]->type = NetworkPkt :: DATA;
		pkts[p]->src = random() % 1000;
		while((pkts[p]->dst = random() % 1000) == ID); //dst cannot be same as ID

	        router->handle_interface(RingRouter::IN_NI, pkts[p]);
	    }

	    list<NetworkPkt*>& buf = router->m_in_buffers[RingRouter::IN_NI];
	    CPPUNIT_ASSERT_EQUAL(2, (int)buf.size());
	    CPPUNIT_ASSERT_EQUAL(pkts[0]->src, buf.front()->src);
	    CPPUNIT_ASSERT_EQUAL(pkts[0]->dst, buf.front()->dst);
	    CPPUNIT_ASSERT_EQUAL(pkts[1]->src, buf.back()->src);
	    CPPUNIT_ASSERT_EQUAL(pkts[1]->dst, buf.back()->dst);

	    delete router;
	}//for
    }




    //======================================================================
    //======================================================================
    //! @brief Verify for buffer size B, handle_interface() can be called B times.
    //!
    //! Create a RingRouter with buffer size B; verify handle_interface()
    //! can be called B times. Repeat N times.
    void testHandle_interface_1()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100;
	    
	    const int ID = 1;

	    //create a RingRouter
	    RingRouter* router = new RingRouter(ID, bufsize);

	    for(int p=0; p<bufsize; p++) {
	        NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: DATA;
		pkt->dst = 2; //dst cannot be same as ID.

	        router->handle_interface(RingRouter::IN_NI, pkt);
	    }

	    list<NetworkPkt*>& buf = router->m_in_buffers[RingRouter::IN_NI];
	    CPPUNIT_ASSERT_EQUAL(bufsize, (int)buf.size());

	    delete router;
	}//for
    }





    //======================================================================
    //======================================================================
    //! @brief Verify handle_interface() correctly updates credit.
    //!
    //! Create a RingRouter with buffer size B; verify credit for the network
    //! interface is set to B (implying the NI's input buffer size is also B).
    //! Manually set the cretdit to 0. Then send B credits by calling handle_interface().
    //! Verify credit is correctly updated.
    void testHandle_interface_2()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100;
	    
	    const int ID = 1;

	    //create a RingRouter
	    RingRouter* router = new RingRouter(ID, bufsize);

	    //verify the credit for the interface is bufsize
	    CPPUNIT_ASSERT_EQUAL(bufsize, router->m_credits[RingRouter::OUT_NI]);

            //manually set the credit to 0
            router->m_credits[RingRouter::OUT_NI] = 0;

	    for(int p=0; p<bufsize; p++) {
	        NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: CREDIT;

                int old_credit = router->m_credits[RingRouter::OUT_NI];
		//call handle_interface
	        router->handle_interface(RingRouter::IN_NI, pkt);
		//verify credit is incremented by 1.
		CPPUNIT_ASSERT_EQUAL(old_credit+1, router->m_credits[RingRouter::OUT_NI]);
	    }

	    delete router;
	}//for
    }



    //======================================================================
    //======================================================================
    //! @brief Verify handle_pred() puts packets in right input buffer and in right order.
    //!
    //! Create a RingRouter; call handle_pred() twice; verify the
    //! packets are put in the right queue in the right order. Repeat N times.
    void testHandle_pred_0()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100 + 2; //at least 2
	    
	    const int ID = random() % 1000;

	    //create a RingRouter
	    RingRouter* router = new RingRouter(ID, bufsize);

	    NetworkPkt* pkts[2];

	    for(int p=0; p<2; p++) {
	        pkts[p] = new NetworkPkt;
		pkts[p]->type = NetworkPkt :: DATA;
		pkts[p]->src = random() % 1000;

	        router->handle_pred(RingRouter::IN_PRED, pkts[p]);
	    }

	    list<NetworkPkt*>& buf = router->m_in_buffers[RingRouter::IN_BUF_PRED];
	    CPPUNIT_ASSERT_EQUAL(2, (int)buf.size());
	    CPPUNIT_ASSERT_EQUAL(pkts[0]->src, buf.front()->src);
	    CPPUNIT_ASSERT_EQUAL(pkts[0]->dst, buf.front()->dst);
	    CPPUNIT_ASSERT_EQUAL(pkts[1]->src, buf.back()->src);
	    CPPUNIT_ASSERT_EQUAL(pkts[1]->dst, buf.back()->dst);

	    delete router;
	}//for
    }




    //======================================================================
    //======================================================================
    //! @brief Verify for buffer size B, handle_pred() can be called B times.
    //!
    //! Create a RingRouter with buffer size B; verify handle_pred()
    //! can be called B times. Repeat N times.
    void testHandle_pred_1()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100;
	    
	    const int ID = random() % 1000;

	    //create a RingRouter
	    RingRouter* router = new RingRouter(ID, bufsize);

	    for(int p=0; p<bufsize; p++) {
	        NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: DATA;

	        router->handle_pred(RingRouter::IN_PRED, pkt);
	    }

	    list<NetworkPkt*>& buf = router->m_in_buffers[RingRouter::IN_BUF_PRED];
	    CPPUNIT_ASSERT_EQUAL(bufsize, (int)buf.size());

	    delete router;
	}//for
    }



    //======================================================================
    //======================================================================
    //! @brief Verify handle_pred() correctly updates credit.
    //!
    //! @brief Create a RingRouter with buffer size B; verify credit for output ports
    //! is set to B.  Manually set the cretdit to 0. Then send B credits by calling handle_succ().
    //! Verify credit is correctly updated.
    void testHandle_succ_0()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100;
	    
	    const int ID = random() % 1000;

	    //create a RingRouter
	    RingRouter* router = new RingRouter(ID, bufsize);

	    //verify the credit for all output ports is bufsize
	    for(int i=1; i<2; i++)
		CPPUNIT_ASSERT_EQUAL(bufsize, router->m_credits[i]);

            router->m_credits[RingRouter::OUT_BUF_SUCC] = 0;

	    for(int p=0; p<bufsize; p++) {
	        NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: CREDIT;

                int old_credit = router->m_credits[RingRouter::OUT_BUF_SUCC];
		//call handle_pred
	        router->handle_succ(RingRouter::IN_SUCC, pkt);
		CPPUNIT_ASSERT_EQUAL(old_credit+1, router->m_credits[RingRouter::OUT_BUF_SUCC]);
	    }

	    delete router;
	}//for
    }






    //======================================================================
    //======================================================================
    //! @brief Test credit is used to control number of outstanding packets.
    //!
    //! Create a router and a 2 sinks in a 3-node network with input buffer size B.
    //! Call handle_pred(), followed by rising_edge() B times.
    //! All packets are destined to the east sink. Since the sink doesn't send back credits.
    //! The credits for the output channel are exhausted. Then send B more packets.
    //! Since there's no credit, no packets should leave the router.
    //! Also verify the behavior of the round-robin arbitration.
    //!
    //!  ID:      0                  1               2
    //!        | sink |<--credit---| R |--data--->| sink |
    //!
    void testRising_edge_0()
    {
        //create a router
	const int ID = 1;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<RingRouter>(0, ID, BUFSIZE);
	RingRouter* router = Component::GetComponent<RingRouter>(router_cid);

        //create 2 sink routers
	CompId_t sink_router0_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink_router0 = Component::GetComponent<MockRouter>(sink_router0_cid);
	CompId_t sink_router2_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink_router2 = Component::GetComponent<MockRouter>(sink_router2_cid);


        //connect the components
	//router to sink0
	Manifold::Connect(router_cid, RingRouter::OUT_PRED, sink_router0_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);

	//router to sink2
	Manifold::Connect(router_cid, RingRouter::OUT_SUCC, sink_router2_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);


        //Call handle_pred() and rising_edge() BUFSIZE times; this would exhaust the
	//credit for the output channel.
	for(int i=0; i<BUFSIZE; i++) {
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = 2;

            //pkt comes in from predecessor
	    router->handle_pred(RingRouter::IN_PRED, pkt);

	    int credit = router->m_credits[RingRouter::OUT_BUF_SUCC];

	    //call rising_edge()
	    router->rising_edge();

	    //Since only IN_PRED enters packets, every time it is set to the lowest priority
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_BUF_PRED, router->m_priority[1]);

	    //verify credit is decremented.
	    CPPUNIT_ASSERT_EQUAL(credit-1, router->m_credits[RingRouter::OUT_BUF_SUCC]);
	}

        //verify credit for the output channel is 0
	CPPUNIT_ASSERT_EQUAL(0, router->m_credits[RingRouter::OUT_BUF_SUCC]);
	CPPUNIT_ASSERT_EQUAL(0, (int)router->m_in_buffers[RingRouter::IN_BUF_PRED].size());


        //again send BUFSIZE packets
	for(int i=0; i<BUFSIZE; i++) {
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = 2;

            //pkt comes in from west
	    router->handle_pred(RingRouter::IN_PRED, pkt);

	    //verify credit is 0.
	    CPPUNIT_ASSERT_EQUAL(0, router->m_credits[RingRouter::OUT_BUF_SUCC]);

	    //call rising_edge()
	    router->rising_edge();

	    //verify credit is 0.
	    CPPUNIT_ASSERT_EQUAL(0, router->m_credits[RingRouter::OUT_BUF_SUCC]);
	}

	Manifold::unhalt();

	Manifold::StopAt(1);
	Manifold::Run(); //get the events processed

        //verify although rising_edge() were called 2*BUFSIZE times, only BUFSIZE packets were delivered.
	CPPUNIT_ASSERT_EQUAL(BUFSIZE, (int)sink_router2->get_pkts().size());

        //verify sink 0 only received credit packets.
	CPPUNIT_ASSERT_EQUAL(0, (int)sink_router0->get_pkts().size());

	//how many credits did sink 0 receive? The first BUFSIZE messages caused BUFSIZE credits.
	//For the 2nd batch of messages, min(BUFSIZE, m_OUTPUT_BUFSIZE) credits were sent.
	CPPUNIT_ASSERT_EQUAL(BUFSIZE + (router->m_OUTPUT_BUFSIZE < BUFSIZE ? router->m_OUTPUT_BUFSIZE : BUFSIZE),
	                     (int)sink_router0->get_credits().size());

	delete router;
	delete sink_router0;
	delete sink_router2;

    }




    //======================================================================
    //======================================================================
    //! @brief Test credit is used to control number of outstanding packets.
    //!
    //! Same as above, except here packets are injected from the network interface.
    //! Create a router and a 2 sinks in a 3-node network with input buffer size B.
    //! Sink 0 plays the network interface.
    //! Call handle_interface(), followed by rising_edge() B times.
    //! All packets are destined to the east sink. Since the sink doesn't send back credits.
    //! The credits for the output channel are exhausted. Then send B more packets.
    //! Since there's no credit, no packets should leave the router.
    //! Also verify the behavior of the round-robin arbitration.
    //!
    //!  ID:      0                  1               2
    //!        | sink |<--credit---| R |--data--->| sink |
    //!
    void testRising_edge_1()
    {
        //create a router
	const int ID = 1;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<RingRouter>(0, ID, BUFSIZE);
	RingRouter* router = Component::GetComponent<RingRouter>(router_cid);

        //create 2 sink routers
	CompId_t sink_router0_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink_router0 = Component::GetComponent<MockRouter>(sink_router0_cid);
	CompId_t sink_router2_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink_router2 = Component::GetComponent<MockRouter>(sink_router2_cid);


        //connect the components
	//router to sink0: sink 0 is the interface
	Manifold::Connect(router_cid, RingRouter::OUT_NI, sink_router0_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);

	//router to sink2
	Manifold::Connect(router_cid, RingRouter::OUT_SUCC, sink_router2_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);


        //Call handle_interface() and rising_edge() BUFSIZE times; this would exhaust the
	//credit for the output channel.
	for(int i=0; i<BUFSIZE; i++) {
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = 2;

            //pkt comes in from predecessor
	    router->handle_interface(RingRouter::IN_NI, pkt);

	    int credit = router->m_credits[RingRouter::OUT_BUF_SUCC];

	    //call rising_edge()
	    router->rising_edge();

	    //verify credit is decremented.
	    CPPUNIT_ASSERT_EQUAL(credit-1, router->m_credits[RingRouter::OUT_BUF_SUCC]);
	}

        //verify credit for the output channel is 0
	CPPUNIT_ASSERT_EQUAL(0, router->m_credits[RingRouter::OUT_BUF_SUCC]);
	CPPUNIT_ASSERT_EQUAL(0, (int)router->m_in_buffers[RingRouter::IN_BUF_NI].size());


        //again send BUFSIZE packets
	for(int i=0; i<BUFSIZE; i++) {
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = 2;

            //pkt comes in from west
	    router->handle_interface(RingRouter::IN_NI, pkt);

	    //verify credit is 0.
	    CPPUNIT_ASSERT_EQUAL(0, router->m_credits[RingRouter::OUT_BUF_SUCC]);

	    //call rising_edge()
	    router->rising_edge();

	    //Since only IN_NI enters packets, every time it is set to the lowest priority
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_BUF_NI, router->m_priority[1]);

	    //verify credit is 0.
	    CPPUNIT_ASSERT_EQUAL(0, router->m_credits[RingRouter::OUT_BUF_SUCC]);
	}

	Manifold::unhalt();

	Manifold::StopAt(1);
	Manifold::Run(); //get the events processed

        //verify although rising_edge() were called 2*BUFSIZE times, only BUFSIZE packets were delivered.
	CPPUNIT_ASSERT_EQUAL(BUFSIZE, (int)sink_router2->get_pkts().size());

        //verify sink 0 only received credit packets.
	CPPUNIT_ASSERT_EQUAL(0, (int)sink_router0->get_pkts().size());

	//how many credits did sink 0 receive? The first BUFSIZE messages caused BUFSIZE credits.
	//For the 2nd batch of messages, min(BUFSIZE, m_OUTPUT_BUFSIZE) credits were sent.
	CPPUNIT_ASSERT_EQUAL(BUFSIZE + (router->m_OUTPUT_BUFSIZE < BUFSIZE ? router->m_OUTPUT_BUFSIZE : BUFSIZE),
	                     (int)sink_router0->get_credits().size());

	delete router;
	delete sink_router0;
	delete sink_router2;

    }




    //======================================================================
    //======================================================================
    //! @brief Test priority of input buffers.
    //!
    //! Create a RingRouter. Manually put the priority in a random state.
    //! Enter a packet to each of the 2 inputs. Call rising_edge() in 2 consecutive cycles.
    //! Verify packets are delivered in expected order.
    //!
    //! The router is connected to 3 sink objects.
    //!
    //!                 | sinkni (as NI) |
    //!                         |
    //!  ID:       0          1 |             2
    //!        | sink |-------| R |------->| sink |
    //!
    void testRising_edge_2()
    {
        //create a router
	const int ID = 1;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<RingRouter>(0, ID, BUFSIZE);
	RingRouter* router = Component::GetComponent<RingRouter>(router_cid);

        //create 3 sinks
	CompId_t sinkni_cid = Component :: Create<MockRouter>(0);
	MockRouter* sinkni = Component::GetComponent<MockRouter>(sinkni_cid);
	CompId_t sink0_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink0 = Component::GetComponent<MockRouter>(sink0_cid);
	CompId_t sink2_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink2 = Component::GetComponent<MockRouter>(sink2_cid);

        //connect the components
	//router to sinkni (sinkni acts as NI)
	Manifold::Connect(router_cid, RingRouter::OUT_NI, sinkni_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink0
	Manifold::Connect(router_cid, RingRouter::OUT_PRED, sink0_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink2
	Manifold::Connect(router_cid, RingRouter::OUT_SUCC, sink2_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);


        //put the input priority in a random state
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

	if(random()/(RAND_MAX+1.0) <= 0.5) {
	    router->m_priority[0] = RingRouter::IN_BUF_NI;
	    router->m_priority[1] = RingRouter::IN_BUF_PRED;
	}
	else {
	    router->m_priority[0] = RingRouter::IN_BUF_PRED;
	    router->m_priority[1] = RingRouter::IN_BUF_NI;
	}

	int old_priority[2];
	old_priority[0] = router->m_priority[0];
	old_priority[1] = router->m_priority[1];




	//enter a packet to each input buffer, all destined to sink 2.
	NetworkPkt* pkt = new NetworkPkt;
	pkt->type = NetworkPkt::DATA;
	pkt->dst = 2;
	pkt->data[0] = RingRouter::IN_NI; //use pkt->data[0] as a signature, which is the port ID.
			                  //so we know from which input the pkt is from.
	router->handle_interface(RingRouter::IN_NI, pkt);

	pkt = new NetworkPkt;
	pkt->type = NetworkPkt::DATA;
	pkt->dst = 2;
	pkt->data[0] = RingRouter::IN_PRED; //use pkt->data[0] as a signature, which is the port ID.
			                    //so we know from which input the pkt is from.
	router->handle_pred(RingRouter::IN_PRED, pkt);


	Manifold::unhalt();
	Ticks_t When = 1;

	//now call rising_edge() 2 times at different time
	for(int i=0; i<2; i++) {
	    Manifold::Schedule(When, test_rising_edge_helper, router);
	    When += 1;
	}

	Manifold::StopAt(When);
	Manifold::Run(); //get the events processed

        //verify sink 2 receives 2 packets
	CPPUNIT_ASSERT_EQUAL(2, (int)sink2->get_pkts().size());

        //verify the packets are delivered in expected order
	list<NetworkPkt>& sink2_pkts = sink2->get_pkts();

        if(old_priority[0] == RingRouter::IN_BUF_NI) { //NI has higher priority
	    list<NetworkPkt>::iterator it = sink2_pkts.begin();
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_NI, (int)(*it).data[0]);
	    ++it;
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_PRED, (int)(*it).data[0]);
	}
	else {
	    list<NetworkPkt>::iterator it = sink2_pkts.begin();
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_PRED, (int)(*it).data[0]);
	    ++it;
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_NI, (int)(*it).data[0]);
	}

	delete router;
	delete sinkni;
	delete sink0;
	delete sink2;

    }





    //======================================================================
    //======================================================================
    //! @brief Test priority of input buffers.
    //!
    //! Create a RingRouter.
    //! Enter a packet to each of the 2 inputs. Call rising_edge(). The packet
    //! from the one input (call it A) should be delivered. Enter another packet to input A.
    //! Call rising_edge() again. The packet from the other input B is delivered.
    //! Call rising_edge() a third time. This time the packet from the A is delivered.
    //! Verify packets are delivered in expected order.
    //!
    //! The router is connected to 3 sink objects.
    //!
    //!                 | sinkni (as NI) |
    //!                         |
    //!  ID:       0          1 |             2
    //!        | sink |-------| R |------->| sink |
    //!
    void testRising_edge_3()
    {
        //create a router
	const int ID = 1;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<RingRouter>(0, ID, BUFSIZE);
	RingRouter* router = Component::GetComponent<RingRouter>(router_cid);

        //create 3 sinks
	CompId_t sinkni_cid = Component :: Create<MockRouter>(0);
	MockRouter* sinkni = Component::GetComponent<MockRouter>(sinkni_cid);
	CompId_t sink0_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink0 = Component::GetComponent<MockRouter>(sink0_cid);
	CompId_t sink2_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink2 = Component::GetComponent<MockRouter>(sink2_cid);

        //connect the components
	//router to sinkni (sinkni acts as NI)
	Manifold::Connect(router_cid, RingRouter::OUT_NI, sinkni_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink0
	Manifold::Connect(router_cid, RingRouter::OUT_PRED, sink0_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink2
	Manifold::Connect(router_cid, RingRouter::OUT_SUCC, sink2_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);

        //put the input priority in a random state
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

	if(random()/(RAND_MAX+1.0) <= 0.5) {
	    router->m_priority[0] = RingRouter::IN_BUF_NI;
	    router->m_priority[1] = RingRouter::IN_BUF_PRED;
	}
	else {
	    router->m_priority[0] = RingRouter::IN_BUF_PRED;
	    router->m_priority[1] = RingRouter::IN_BUF_NI;
	}

	int old_priority[2];
	old_priority[0] = router->m_priority[0];
	old_priority[1] = router->m_priority[1];


	//enter a packet to each input buffer, all destined to sink 2.
	NetworkPkt* pkt = new NetworkPkt;
	pkt->type = NetworkPkt::DATA;
	pkt->dst = 2;
	pkt->data[0] = RingRouter::IN_NI; //use pkt->data[0] as a signature, which is the port ID.
			                  //so we know from which input the pkt is from.
	router->handle_interface(RingRouter::IN_NI, pkt);

	pkt = new NetworkPkt;
	pkt->type = NetworkPkt::DATA;
	pkt->dst = 2;
	pkt->data[0] = RingRouter::IN_PRED; //use pkt->data[0] as a signature, which is the port ID.
			                    //so we know from which input the pkt is from.
	router->handle_pred(RingRouter::IN_PRED, pkt);


        //This pkt is entered to the NI right after the first call of rising_edge().
	pkt = new NetworkPkt;
	pkt->type = NetworkPkt::DATA;
	pkt->dst = 2;
	pkt->data[0] = RingRouter::IN_NI;



	Manifold::unhalt();
	Ticks_t When = 1;

	//now call rising_edge() 3 times at different time
	Manifold::Schedule(When, test_rising_edge_helper_schedule_ni, router, pkt);

	for(int i=0; i<2; i++) {
	    Manifold::Schedule(When, test_rising_edge_helper, router);
	    When += 1;
	}

	Manifold::StopAt(When);
	Manifold::Run(); //get the events processed

        //verify sink 2 receives 3 packets
	CPPUNIT_ASSERT_EQUAL(3, (int)sink2->get_pkts().size());

        //verify the packets are delivered in expected order
	if(old_priority[0] == RingRouter::IN_BUF_NI) { //packets would be NI, PRED, NI
	    list<NetworkPkt>& sink2_pkts = sink2->get_pkts();

	    list<NetworkPkt>::iterator it = sink2_pkts.begin();
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_NI, (int)(*it).data[0]);
	    ++it;
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_PRED, (int)(*it).data[0]);
	    ++it;
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_NI, (int)(*it).data[0]);
	}
	else { //packets would be PRED, NI, NI
	    list<NetworkPkt>& sink2_pkts = sink2->get_pkts();

	    list<NetworkPkt>::iterator it = sink2_pkts.begin();
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_PRED, (int)(*it).data[0]);
	    ++it;
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_NI, (int)(*it).data[0]);
	    ++it;
	    CPPUNIT_ASSERT_EQUAL((int)RingRouter::IN_NI, (int)(*it).data[0]);
	}

	delete router;
	delete sinkni;
	delete sink0;
	delete sink2;

    }





    //======================================================================
    //======================================================================
    //! @brief Test packet forwarding: packets for different outputs are forwared in the same cycle.
    //!
    //! Create a RingRouter. Enter a packet to each of the 2 inputs: packet from NI goes to
    //! successor, and packet from predecessor goes to NI.
    //! Call rising_edge() once. Since the 2 packets at the head of the 2 input buffers
    //! go to different outputs, both 2 are forwarded in the same cycle.
    //!
    //! The router is connected to 3 sink objects.
    //!
    //!                | sinkni (as NI) |
    //!                         |
    //!  ID:       0          1 |             2
    //!        | sink |<------| R |------->| sink |
    //!
    void testRising_edge_4()
    {
        //create a router
	const int ID = 1;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<RingRouter>(0, ID, BUFSIZE);
	RingRouter* router = Component::GetComponent<RingRouter>(router_cid);

        //create 3 sinks
	CompId_t sinkni_cid = Component :: Create<MockRouter>(0);
	MockRouter* sinkni = Component::GetComponent<MockRouter>(sinkni_cid);
	CompId_t sink0_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink0 = Component::GetComponent<MockRouter>(sink0_cid);
	CompId_t sink2_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink2 = Component::GetComponent<MockRouter>(sink2_cid);

        //connect the components
	//router to sinkni (sinkni acts as NI)
	Manifold::Connect(router_cid, RingRouter::OUT_NI, sinkni_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink0
	Manifold::Connect(router_cid, RingRouter::OUT_PRED, sink0_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink2
	Manifold::Connect(router_cid, RingRouter::OUT_SUCC, sink2_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);



	//enter a packet to each input
	NetworkPkt* pkt = new NetworkPkt;
	pkt->type = NetworkPkt::DATA;
	pkt->dst = 2; //packet from NI goes to successor

	router->handle_interface(RingRouter::IN_NI, pkt);

	pkt = new NetworkPkt;
	pkt->type = NetworkPkt::DATA;
	pkt->dst = ID; //packet from predecessor goes to NI

	router->handle_pred(RingRouter::IN_PRED, pkt);

	Manifold::unhalt();

	Ticks_t When = 1;

	//call rising_edge()
	Manifold::Schedule(When, test_rising_edge_helper, router);
	When += 1;

	Manifold::StopAt(When);
	Manifold::Run(); //get the events processed

        //verify both NI and successor receive a packet
	CPPUNIT_ASSERT_EQUAL(1, (int)sinkni->get_pkts().size());
	CPPUNIT_ASSERT_EQUAL(1, (int)sink2->get_pkts().size());

        //verify the packets are delivered at the same cycle
	CPPUNIT_ASSERT_EQUAL(sinkni->get_ticks().front(), sink2->get_ticks().front());

	delete router;
	delete sinkni;
	delete sink0;
	delete sink2;
    }





    //======================================================================
    //======================================================================
    //! @brief Test routing.
    //!
    //! Create a RingRouter (ID 1) in a 16-node network. Randomly send packets from
    //! NI and predecessor with random destinations. Verify the packets are forwarded
    //! as expected.
    //!
    //! The router is connected to 3 sink objects.
    //!
    //!                | sinkni (as NI) |
    //!                         |
    //!  ID:       0          1 |             2
    //!        | sink |<------| R |------->| sink |
    //!
    void testRising_edge_5()
    {
        //create a router
	const int N = 16; 
	const int ID = 1;
	const int BUFSIZE = random() % 100 + 100;
	CompId_t router_cid = Component :: Create<RingRouter>(0, ID, BUFSIZE);
	RingRouter* router = Component::GetComponent<RingRouter>(router_cid);

        //create 3 sinks
	CompId_t sinkni_cid = Component :: Create<MockRouter>(0);
	MockRouter* sinkni = Component::GetComponent<MockRouter>(sinkni_cid);
	CompId_t sink0_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink0 = Component::GetComponent<MockRouter>(sink0_cid);
	CompId_t sink2_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink2 = Component::GetComponent<MockRouter>(sink2_cid);

        //connect the components
	//router to sinkni (sinkni acts as NI)
	Manifold::Connect(router_cid, RingRouter::OUT_NI, sinkni_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink0
	Manifold::Connect(router_cid, RingRouter::OUT_PRED, sink0_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink2
	Manifold::Connect(router_cid, RingRouter::OUT_SUCC, sink2_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);


	int ni_pkts = 0;
	int pred_pkts = 0;

	while(ni_pkts < BUFSIZE || pred_pkts < BUFSIZE) {
	    if(ni_pkts < BUFSIZE) {
		if(random() / (RAND_MAX+1.0) < 0.5) { //50% chance, enter a packet
		    NetworkPkt* pkt = new NetworkPkt;
		    pkt->type = NetworkPkt::DATA;
		    //packet from ni cannot be destined to self
		    while((pkt->dst = random() % N) == ID);

		    router->handle_interface(RingRouter::IN_NI, pkt);
		    ni_pkts++;
		}
	    }
	    if(pred_pkts < BUFSIZE) {
		if(random() / (RAND_MAX+1.0) < 0.5) { //50% chance, enter a packet
		    NetworkPkt* pkt = new NetworkPkt;
		    pkt->type = NetworkPkt::DATA;
		    pkt->dst = random() % N;

		    router->handle_pred(RingRouter::IN_PRED, pkt);
		    pred_pkts++;
		}
	    }

	    router->rising_edge();
	}

	Manifold::unhalt();

	Manifold::StopAt(1);
	Manifold::Run(); //get the events processed

        //verify received packets: NI
	for(list<NetworkPkt>::iterator it=sinkni->get_pkts().begin();
	      it != sinkni->get_pkts().end(); ++it) {
	    CPPUNIT_ASSERT_EQUAL(ID, (*it).dst);
	}

        //verify received packets: node 2
	for(list<NetworkPkt>::iterator it=sink2->get_pkts().begin();
	      it != sink2->get_pkts().end(); ++it) {
	    CPPUNIT_ASSERT((*it).dst != ID);
	}


	delete router;
	delete sinkni;
	delete sink0;
	delete sink2;

    }













    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("RingRouterTest");

	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testConstructor_0", &RingRouterTest::testConstructor_0));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testConstructor_1", &RingRouterTest::testConstructor_1));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testHandle_interface_0", &RingRouterTest::testHandle_interface_0));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testHandle_interface_1", &RingRouterTest::testHandle_interface_1));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testHandle_interface_2", &RingRouterTest::testHandle_interface_2));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testHandle_pred_0", &RingRouterTest::testHandle_pred_0));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testHandle_pred_1", &RingRouterTest::testHandle_pred_1));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testHandle_succ_0", &RingRouterTest::testHandle_succ_0));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testRising_edge_0", &RingRouterTest::testRising_edge_0));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testRising_edge_1", &RingRouterTest::testRising_edge_1));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testRising_edge_2", &RingRouterTest::testRising_edge_2));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testRising_edge_3", &RingRouterTest::testRising_edge_3));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testRising_edge_4", &RingRouterTest::testRising_edge_4));
	mySuite->addTest(new CppUnit::TestCaller<RingRouterTest>("testRising_edge_5", &RingRouterTest::testRising_edge_5));
	/*
	*/
	return mySuite;
    }
};


Clock RingRouterTest :: MasterClock(RingRouterTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold::Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( RingRouterTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


