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


void test_rising_edge_helper(MeshRouter* router)
{
    router->rising_edge();
}



//####################################################################
//####################################################################
class MeshRouterTest : public CppUnit::TestFixture {
private:

    //This function is used to test the round-robin arbitration of MeshRouter.
    //Based on current input and output buffer status, it predict the new
    //priority array. We then compare the prediction with the actual values.
    void predict_priority(MeshRouter* router, int priority[])
    {
        //The following is copied from MeshRouter :: rising_edge(). In that function
	//the input packets are moved to output buffers. Obviously this should not
	//be done here. For output buffers, we only need to know their size.
	int output_buffer_size[5];
	for(int i=0; i<5; i++)
	    output_buffer_size[i] = router->m_outputs[i].size();


	vector<int> not_served_inputs;
	vector<int> served_inputs;

	//move (at most) 1 packet from each input buffer to the output buffer.
	for(int i=0; i<5; i++) { //check all input buffers
	    int input = router->m_priority[i];

	    if(router->m_in_buffers[input].size() > 0) {
		NetworkPkt* pkt = router->m_in_buffers[input].front();

		//Find out the output channel for the packet.
		int output = MeshRouter::OUT_NI; //If I'm the destination, go to NI.

		if(router->m_id != pkt->dst) { //I'm not the destination
		    //packet is forwarded based on destination; x first
		    int dest_x = pkt->dst % router->m_x_dimension;
		    int dest_y = pkt->dst / router->m_x_dimension;
		    if(dest_x < router->m_id % router->m_x_dimension) {
			output = MeshRouter::OUT_WEST;
		    }
		    else if(dest_x > router->m_id % router->m_x_dimension) {
			output = MeshRouter::OUT_EAST;
		    }
		    else { //same column
			if(dest_y < router->m_id / router->m_x_dimension)
			    output = MeshRouter::OUT_NORTH;
			else
			    output = MeshRouter::OUT_SOUTH;
		    }
		}

		if(output_buffer_size[output] < router->m_OUTPUT_BUFSIZE) {
		    output_buffer_size[output]++; //simulate output buffer size change.
		    //put in served_inputs so we can update input priority
		    served_inputs.push_back(input);
		}
		else {
		    not_served_inputs.push_back(input);
		}
	    }
	    else { //input buffer is empty
		not_served_inputs.push_back(input);
	    }
	}//for

	assert(served_inputs.size() + not_served_inputs.size() == 5);

	//update priority
	int idx=0;
	for(unsigned i=0; i<not_served_inputs.size(); i++)
	    priority[idx++] = not_served_inputs[i];
	for(unsigned i=0; i<served_inputs.size(); i++)
	    priority[idx++] = served_inputs[i];
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
    //! @brief Verify ID is correctly initialized.
    //!
    //! Create N MeshRouter objects with random IDs; verify the ID
    //! is what's in the constructor.
    void testConstructor_0()
    {
        for(int i=0; i<1000; i++) {
	    int id = random() % 1000;
	    MeshRouter* r = new MeshRouter(id, 2,2,10);

	    CPPUNIT_ASSERT_EQUAL(id, r->get_id());
	    delete r;
	}

    }


    //======================================================================
    //======================================================================
    //! @brief Verify x, y, bufsize are correctly initialized.
    void testConstructor_1()
    {
        for(int i=0; i<1000; i++) {
	    int x = random() % 1000;
	    int y = random() % 1000;
	    int credits = random() % 1000;
	    
	    MeshRouter* r = new MeshRouter(0, x, y, credits);

	    CPPUNIT_ASSERT_EQUAL(x, r->m_x_dimension);
	    CPPUNIT_ASSERT_EQUAL(y, r->m_y_dimension);
	    CPPUNIT_ASSERT_EQUAL(credits, r->m_CREDITS);
	    for(int j=0; j<5; j++)
		CPPUNIT_ASSERT_EQUAL(credits, r->m_credits[j]);
	    delete r;
	}

    }



    //======================================================================
    //======================================================================
    //! @brief Verify handle_interface() puts packets in input buffer and in correct order.
    //!
    //! Create a MeshRouter; call handle_interface() twice; verify the
    //! packets are put in the right input buffer in the right order. Repeat N times.
    void testHandle_interface_0()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100 + 2; //at least 2
	    
	    const int ID = 1;

	    //create a MeshRouter
	    MeshRouter* router = new MeshRouter(ID, 2, 2, bufsize);

	    NetworkPkt* pkts[2];

	    for(int p=0; p<2; p++) {
	        pkts[p] = new NetworkPkt;
		pkts[p]->type = NetworkPkt :: DATA;
		pkts[p]->src = random() % 1000;
		while((pkts[p]->dst = random() % 1000) == ID); //dst cannot be same as ID

	        router->handle_interface(MeshRouter::IN_NI, pkts[p]);
	    }

	    list<NetworkPkt*>& buf = router->m_in_buffers[MeshRouter::IN_NI];
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
    //! Create a MeshRouter with buffer size B; verify handle_interface()
    //! can be called B times. Repeat N times.
    void testHandle_interface_1()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100;
	    
	    const int ID = 1;

	    //create a MeshRouter
	    MeshRouter* router = new MeshRouter(ID, 2, 2, bufsize);

	    for(int p=0; p<bufsize; p++) {
	        NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: DATA;
		pkt->dst = 2; //dst cannot be same as ID.

	        router->handle_interface(MeshRouter::IN_NI, pkt);
	    }

	    list<NetworkPkt*>& buf = router->m_in_buffers[MeshRouter::IN_NI];
	    CPPUNIT_ASSERT_EQUAL(bufsize, (int)buf.size());

	    delete router;
	}//for
    }



    //======================================================================
    //======================================================================
    //! @brief Verify handle_interface() correctly updates credit.
    //!
    //! Create a MeshRouter with buffer size B; verify credit for the network
    //! interface is set to B (implying the NI's input buffer size is also B).
    //! Manually set the cretdit to 0. Then send B credits by calling handle_interface().
    //! Verify credit is correctly updated.
    void testHandle_interface_2()
    {
        for(int i=0; i<1000; i++) {
	    int credits = random() % 100;
	    
	    const int ID = 1;

	    //create a MeshRouter
	    MeshRouter* router = new MeshRouter(ID, 2, 2, credits);

            //manually set the credit to 0
            router->m_credits[MeshRouter::OUT_NI] = 0;

	    for(int p=0; p<credits; p++) {
	        NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: CREDIT;

                int old_credit = router->m_credits[MeshRouter::OUT_NI];
		//call handle_interface
	        router->handle_interface(MeshRouter::IN_NI, pkt);
		//verify credit is incremented by 1.
		CPPUNIT_ASSERT_EQUAL(old_credit+1, router->m_credits[MeshRouter::OUT_NI]);
	    }

	    delete router;
	}//for
    }



    //======================================================================
    //======================================================================
    //! @brief Verify handle_router() puts packets in right input buffer and in right order.
    //!
    //! Create a MeshRouter; call handle_router() twice; verify the
    //! packets are put in the right queue in the right order. Repeat N times.
    void testHandle_router_0()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100 + 2; //at least 2
	    
	    const int ID = random() % 1000;
	    const int PORT = random() % 4 + 1; // 1 to 4; randomly pick a port

	    //create a MeshRouter
	    MeshRouter* router = new MeshRouter(ID, 2, 2, bufsize);

	    NetworkPkt* pkts[2];

	    for(int p=0; p<2; p++) {
	        pkts[p] = new NetworkPkt;
		pkts[p]->type = NetworkPkt :: DATA;
		pkts[p]->src = random() % 1000;

	        router->handle_router(PORT, pkts[p]);
	    }

	    list<NetworkPkt*>& buf = router->m_in_buffers[PORT];
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
    //! @brief Verify for buffer size B, handle_router() can be called B times.
    //!
    //! Create a MeshRouter with buffer size B; verify handle_router()
    //! can be called B times. Repeat N times.
    void testHandle_router_1()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100;
	    
	    const int ID = random() % 1000;
	    const int PORT = random() % 4 + 1; // 1 to 4

	    //create a MeshRouter
	    MeshRouter* router = new MeshRouter(ID, 2, 2, bufsize);

	    for(int p=0; p<bufsize; p++) {
	        NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: DATA;

	        router->handle_router(PORT, pkt);
	    }

	    list<NetworkPkt*>& buf = router->m_in_buffers[PORT];
	    CPPUNIT_ASSERT_EQUAL(bufsize, (int)buf.size());

	    delete router;
	}//for
    }



    //======================================================================
    //======================================================================
    //! @brief Verify handle_router() correctly updates credit.
    //!
    //! @brief Create a MeshRouter with buffer size B; verify credit for all router ports
    //! is set to B.  Manually set the cretdit to 0. Then send B credits by calling handle_router().
    //! Verify credit is correctly updated.
    void testHandle_router_2()
    {
        for(int i=0; i<1000; i++) {
	    int bufsize = random() % 100;
	    
	    const int ID = random() % 1000;
	    const int PORT = random() % 4 + 1; // 1 to 4

	    //create a MeshRouter
	    MeshRouter* router = new MeshRouter(ID, 2, 2, bufsize);

	    //verify the credit for all router ports is bufsize
	    for(int i=1; i<5; i++)
		CPPUNIT_ASSERT_EQUAL(bufsize, router->m_credits[i]);

            router->m_credits[PORT] = 0;

	    for(int p=0; p<bufsize; p++) {
	        NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: CREDIT;

                int old_credit = router->m_credits[PORT];
		//call handle_router
	        router->handle_router(PORT, pkt);
		CPPUNIT_ASSERT_EQUAL(old_credit+1, router->m_credits[PORT]);
	    }

	    delete router;
	}//for
    }






    //======================================================================
    //======================================================================
    //! @brief Test credit is used to control number of outstanding packets.
    //!
    //! Create a router and a 2 sinks in a 1x3 network with input buffer size B.
    //! Call handle_router(), followed by rising_edge() B times.
    //! All packets are destined to the east sink. Since the sink doesn't send back credits.
    //! The credits for the output channel are exhausted. Then send B more packets.
    //! Since there's no credit, no packets should leave the router.
    //! Also verify the behavior of the round-robin arbitration.
    //!
    //!  ID:      0                   1                2
    //!        | sink |<--credit----| R |---data--->| sink |
    //!
    void testRising_edge_0()
    {
        //create a router
	const int X = 3;
	const int Y = 1;
	const int ID = 1;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<MeshRouter>(0, ID, X, Y, BUFSIZE);
	MeshRouter* router = Component::GetComponent<MeshRouter>(router_cid);

        //create 2 sink routers
	CompId_t sink_router0_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink_router0 = Component::GetComponent<MockRouter>(sink_router0_cid);
	CompId_t sink_router2_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink_router2 = Component::GetComponent<MockRouter>(sink_router2_cid);


        //connect the components
	//router to sink0
	Manifold::Connect(router_cid, MeshRouter::OUT_WEST, sink_router0_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);

	//router to sink2
	Manifold::Connect(router_cid, MeshRouter::OUT_EAST, sink_router2_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);

        //Before any packets is processed, the default lowest priority is input 5 (south).
	CPPUNIT_ASSERT_EQUAL((int)MeshRouter::IN_SOUTH, router->m_priority[4]);

        //Call handle_router() and rising_edge() BUFSIZE times; this would exhaust the
	//credit for the output channel.
	for(int i=0; i<BUFSIZE; i++) {
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = 2;

            //pkt comes in from west
	    router->handle_router(MeshRouter::IN_WEST, pkt);

	    int credit = router->m_credits[MeshRouter::OUT_EAST];

	    //call rising_edge()
	    router->rising_edge();

            //Since only IN_WEST enters packets, every time it is set to the lowest priority
	    CPPUNIT_ASSERT_EQUAL((int)MeshRouter::IN_WEST, router->m_priority[4]);


	    //verify credit is decremented.
	    CPPUNIT_ASSERT_EQUAL(credit-1, router->m_credits[MeshRouter::OUT_EAST]);
	}

        //verify credit for the output channel is 0
	CPPUNIT_ASSERT_EQUAL(0, router->m_credits[MeshRouter::OUT_EAST]);
	CPPUNIT_ASSERT_EQUAL(0, (int)router->m_in_buffers[MeshRouter::IN_WEST].size());


        //again send BUFSIZE packets
	for(int i=0; i<BUFSIZE; i++) {
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = 2;

            //pkt comes in from west
	    router->handle_router(MeshRouter::IN_WEST, pkt);

	    //verify credit is 0.
	    CPPUNIT_ASSERT_EQUAL(0, router->m_credits[MeshRouter::OUT_EAST]);

	    //call rising_edge()
	    router->rising_edge();

	    //verify credit is 0.
	    CPPUNIT_ASSERT_EQUAL(0, router->m_credits[MeshRouter::OUT_EAST]);
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
    //! Same as above, except packets are injected from the network interface.
    //! Create a router and a 2 sinks in a 1x3 network with input buffer size B.
    //! Call handle_router(), followed by rising_edge() B times.
    //! All packets are destined to the east sink. Since the sink doesn't send back credits.
    //! The credits for the output channel are exhausted. Then send B more packets.
    //! Since there's no credit, no packets should leave the router.
    //! Also verify the behavior of the round-robin arbitration.
    //!
    //!  ID:      0                   1                2
    //!        | sink |<--credit----| R |---data--->| sink |
    //!
    void testRising_edge_1()
    {
        //create a router
	const int X = 3;
	const int Y = 1;
	const int ID = 1;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<MeshRouter>(0, ID, X, Y, BUFSIZE);
	MeshRouter* router = Component::GetComponent<MeshRouter>(router_cid);

        //create 2 sink routers
	CompId_t sink_router0_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink_router0 = Component::GetComponent<MockRouter>(sink_router0_cid);
	CompId_t sink_router2_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink_router2 = Component::GetComponent<MockRouter>(sink_router2_cid);


        //connect the components
	//router to sink0: sink0 represents the network interface
	Manifold::Connect(router_cid, MeshRouter::OUT_NI, sink_router0_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);

	//router to sink2
	Manifold::Connect(router_cid, MeshRouter::OUT_EAST, sink_router2_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);

        //Before any packets is processed, the default lowest priority is input 5 (south).
	CPPUNIT_ASSERT_EQUAL((int)MeshRouter::IN_SOUTH, router->m_priority[4]);

        //Call handle_router() and rising_edge() BUFSIZE times; this would exhaust the
	//credit for the output channel.
	for(int i=0; i<BUFSIZE; i++) {
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = 2;

            //pkt comes in from ni
	    router->handle_interface(MeshRouter::IN_NI, pkt);

	    int credit = router->m_credits[MeshRouter::OUT_EAST];

	    //call rising_edge()
	    router->rising_edge();

            //Since only IN_NI enters packets, every time it is set to the lowest priority
	    CPPUNIT_ASSERT_EQUAL((int)MeshRouter::IN_NI, router->m_priority[4]);

	    //verify credit is decremented.
	    CPPUNIT_ASSERT_EQUAL(credit-1, router->m_credits[MeshRouter::OUT_EAST]);
	}

        //verify credit for the output channel is 0
	CPPUNIT_ASSERT_EQUAL(0, router->m_credits[MeshRouter::OUT_EAST]);
	CPPUNIT_ASSERT_EQUAL(0, (int)router->m_in_buffers[MeshRouter::IN_NI].size());


        //again send BUFSIZE packets
	for(int i=0; i<BUFSIZE; i++) {
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = 2;

            //pkt comes in from west
	    router->handle_interface(MeshRouter::IN_NI, pkt);

	    //verify credit is 0.
	    CPPUNIT_ASSERT_EQUAL(0, router->m_credits[MeshRouter::OUT_EAST]);

	    //call rising_edge()
	    router->rising_edge();

	    //verify credit is 0.
	    CPPUNIT_ASSERT_EQUAL(0, router->m_credits[MeshRouter::OUT_EAST]);
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
    //! @brief Test input priority is updated round-robin after each call of rising_edge.
    //!
    //! Create a MeshRouter; randomly enter packets by calling handle_interface()
    //! and handle_router(); at the same time call rising_edge() to process the inputs.
    //! Call rising_edge() a random number of times. After each call verify the input priority is
    //! correctly updated.
    //!
    //! The router is connected to 5 sink objects.
    //!
    //!                 | sink 1 |  | sinkni (as NI) |
    //!                         |  /
    //!  ID:       3          4 | /           5
    //!        | sink |<------| R |------->| sink |
    //!                         |
    //!                      | sink 7 |
    void testRising_edge_2()
    {

        //create a router
	const int X = 3; 
	const int Y = 3;
	const int ID = 4;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<MeshRouter>(0, ID, X, Y, BUFSIZE);
	MeshRouter* router = Component::GetComponent<MeshRouter>(router_cid);

        //create 5 sinks
	CompId_t sink1_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink1 = Component::GetComponent<MockRouter>(sink1_cid);
	CompId_t sinkni_cid = Component :: Create<MockRouter>(0);
	MockRouter* sinkni = Component::GetComponent<MockRouter>(sinkni_cid);
	CompId_t sink3_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink3 = Component::GetComponent<MockRouter>(sink3_cid);
	CompId_t sink5_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink5 = Component::GetComponent<MockRouter>(sink5_cid);
	CompId_t sink7_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink7 = Component::GetComponent<MockRouter>(sink7_cid);

        //connect the components
	//router to sinkni (sinkni acts as NI)
	Manifold::Connect(router_cid, MeshRouter::OUT_NI, sinkni_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink1
	Manifold::Connect(router_cid, MeshRouter::OUT_NORTH, sink1_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink3
	Manifold::Connect(router_cid, MeshRouter::OUT_WEST, sink3_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink5
	Manifold::Connect(router_cid, MeshRouter::OUT_EAST, sink5_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink7
	Manifold::Connect(router_cid, MeshRouter::OUT_SOUTH, sink7_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);


        //Call rising_edge() random number of times.
        int Ncalls = random() % (4*BUFSIZE) + BUFSIZE;

        int npkts[5];
	for(int i=0; i<5; i++)
	    npkts[i] = 0;

        //each time before calling rising_edge(), packets arrive in a random way.
	for(int i=0; i<Ncalls; i++) {
	    for(int j=0; j<5; j++) {//for all 5 input ports
		if(npkts[j] < BUFSIZE) { //no one port should send more than BUFSIZE as we don't get credit back.
		    if(random() / (RAND_MAX+1.0) < 0.5) { //50% chance, enter a packet
			NetworkPkt* pkt = new NetworkPkt;
			pkt->type = NetworkPkt::DATA;
			if(j == MeshRouter::IN_NI) {//for interface, destination can't be self
			    while((pkt->dst = random()%(X*Y)) == 4);
			    router->handle_interface(j, pkt);
			}
			else {
			    pkt->dst = random() % (X*Y);
			    router->handle_router(j, pkt);
			}
			npkts[j]++;
		    }
		}
	    }//for

	    //predict the priority array
	    int priority[5];

            //This function is essentially a copy of rising_edge(). So it's not surprising
	    //the predicted values would be the same as the actual values. Is this verification
	    //useful? Anyway, this is the best that can be done for now.
	    predict_priority(router, priority);

	    //call rising_edge()
	    router->rising_edge();

	    //verify
	    for(int i=0; i<5; i++)
		CPPUNIT_ASSERT_EQUAL(priority[i], router->m_priority[i]);

	}

	Manifold::unhalt();

	Manifold::StopAt(1);
	Manifold::Run(); //get the events processed


	delete router;
	delete sinkni;
	delete sink1;
	delete sink3;
	delete sink5;
	delete sink7;
    }




    //======================================================================
    //======================================================================
    //! @brief Test priority of input buffers.
    //!
    //! Create a MeshRouter. Manually put the input priority to a random state.
    //! Enter a packet to each of the 5 inputs except east, all destined to east.
    //! Call rising_edge() in 4 consecutive cycles. Verify packets are delivered
    //! in expected order.
    //!
    //! The router is connected to 5 sink objects.
    //!
    //!                 | sink 1 |  | sinkni (as NI) |
    //!                         |  /
    //!  ID:       3          4 | /           5
    //!        | sink |<------| R |------->| sink |
    //!                         |
    //!                      | sink 7 |
    void testRising_edge_3()
    {
        //create a router
	const int X = 3; 
	const int Y = 3;
	const int ID = 4;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<MeshRouter>(0, ID, X, Y, BUFSIZE);
	MeshRouter* router = Component::GetComponent<MeshRouter>(router_cid);

        //create 5 sinks
	CompId_t sink1_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink1 = Component::GetComponent<MockRouter>(sink1_cid);
	CompId_t sinkni_cid = Component :: Create<MockRouter>(0);
	MockRouter* sinkni = Component::GetComponent<MockRouter>(sinkni_cid);
	CompId_t sink3_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink3 = Component::GetComponent<MockRouter>(sink3_cid);
	CompId_t sink5_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink5 = Component::GetComponent<MockRouter>(sink5_cid);
	CompId_t sink7_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink7 = Component::GetComponent<MockRouter>(sink7_cid);

        //connect the components
	//router to sinkni (sinkni acts as NI)
	Manifold::Connect(router_cid, MeshRouter::OUT_NI, sinkni_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink1
	Manifold::Connect(router_cid, MeshRouter::OUT_NORTH, sink1_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink3
	Manifold::Connect(router_cid, MeshRouter::OUT_WEST, sink3_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink5
	Manifold::Connect(router_cid, MeshRouter::OUT_EAST, sink5_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink7
	Manifold::Connect(router_cid, MeshRouter::OUT_SOUTH, sink7_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);



        //put m_priority in a random state
	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

	int draws[5]; //holds all input buffer IDs
	for(int i=0; i<5; i++)
	    draws[i] = i;

        for(int i=0; i<5; i++) {
	    int idx = random() % (5-i);
	    router->m_priority[i] = draws[idx];
	    draws[idx] = draws[5-i-1];
	}

        for(int i=0; i<5; i++) { //ensure m_priority is a permutation of all input buffer IDs.
	    bool found = false;
	    for(int j=0; j<5; j++) {
	        if(router->m_priority[j] == i) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}

        int old_priority[5];
        for(int i=0; i<5; i++)
	    old_priority[i] = router->m_priority[i];


	//enter a packet to each input buffer except east, all destined to sink 5
	for(int i=0; i<5; i++) {
	    if(i != MeshRouter::IN_EAST) {
		NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt::DATA;
		pkt->dst = 5;
		pkt->data[0] = i; //use pkt->data[0] as a signature, which is the port ID.
		                  //so we know from which input the pkt is from.

		if(i != MeshRouter::IN_NI)
		    router->handle_router(i, pkt);
		else
		    router->handle_interface(i, pkt);
	    }
	}


	Manifold::unhalt();
	Ticks_t When = 1;

	//now call rising_edge() 4 times at different time
	for(int i=0; i<4; i++) {
	    Manifold::Schedule(When, test_rising_edge_helper, router);
	    When += 1;
	}

	Manifold::StopAt(When);
	Manifold::Run(); //get the events processed

        //verify sink 5 receives 4 packets
	CPPUNIT_ASSERT_EQUAL(4, (int)sink5->get_pkts().size());

        //verify the packets are delivered in expected order
	//since EAST is 1, if m_priority at the beginning is  A 1 B C D
	//Then packets would be in the order A B C D
	list<NetworkPkt>& sink5_pkts = sink5->get_pkts();

	list<NetworkPkt>::iterator it = sink5_pkts.begin();
	for(int i=0; i<5; i++) {
	    int input = old_priority[i];

	    if(input != MeshRouter::IN_EAST) {
		CPPUNIT_ASSERT_EQUAL(input, (int)(*it).data[0]);
		++it;
	    }
	}

	//The priority would become 1 A B C D
	
	//use something similar to bubble sort to move 1 to index 0
	for(int i=4; i>0; i--)
	    if(old_priority[i] == MeshRouter::IN_EAST) { //exchange index i and i-1.
	        old_priority[i] = old_priority[i-1];
		old_priority[i-1] = MeshRouter::IN_EAST;
	    }

	for(int i=0; i<5; i++) {
	    CPPUNIT_ASSERT_EQUAL(old_priority[i], router->m_priority[i]);
	}

	delete router;
	delete sinkni;
	delete sink1;
	delete sink3;
	delete sink5;
	delete sink7;
    }





    //======================================================================
    //======================================================================
    //! @brief Test packet forwarding: packets for different outputs are forwared in the same cycle.
    //!
    //! Create a MeshRouter. Manually put the input priority to a random state.
    //! Enter a packet to each of the 5 inputs: packet from NI goes north, packet from
    //! north goes west, packet from west goes south, packet from south goes east,
    //! and packet from east goes to ni.
    //! Call rising_edge() once. Since all 5 packets at the head of the 5 input buffers
    //! go to different outputs, all 5 are forwarded in the same cycle.
    //!
    //! The router is connected to 5 sink objects.
    //!
    //!                 | sink 1 |  | sinkni (as NI) |
    //!                         |  /
    //!  ID:       3          4 | /           5
    //!        | sink |<------| R |------->| sink |
    //!                         |
    //!                      | sink 7 |
    void testRising_edge_4()
    {
        //create a router
	const int X = 3; 
	const int Y = 3;
	const int ID = 4;
	const int BUFSIZE = random() % 100 + 10;
	CompId_t router_cid = Component :: Create<MeshRouter>(0, ID, X, Y, BUFSIZE);
	MeshRouter* router = Component::GetComponent<MeshRouter>(router_cid);

        //create 5 sinks
	CompId_t sink1_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink1 = Component::GetComponent<MockRouter>(sink1_cid);
	CompId_t sinkni_cid = Component :: Create<MockRouter>(0);
	MockRouter* sinkni = Component::GetComponent<MockRouter>(sinkni_cid);
	CompId_t sink3_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink3 = Component::GetComponent<MockRouter>(sink3_cid);
	CompId_t sink5_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink5 = Component::GetComponent<MockRouter>(sink5_cid);
	CompId_t sink7_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink7 = Component::GetComponent<MockRouter>(sink7_cid);

        //connect the components
	//router to sinkni (sinkni acts as NI)
	Manifold::Connect(router_cid, MeshRouter::OUT_NI, sinkni_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink1
	Manifold::Connect(router_cid, MeshRouter::OUT_NORTH, sink1_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink3
	Manifold::Connect(router_cid, MeshRouter::OUT_WEST, sink3_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink5
	Manifold::Connect(router_cid, MeshRouter::OUT_EAST, sink5_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink7
	Manifold::Connect(router_cid, MeshRouter::OUT_SOUTH, sink7_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);


	//enter a packet to each input
	for(int i=0; i<5; i++) {
	    int dst;
	    switch(i) {
	        case MeshRouter::IN_NI: //packet from NI goes north
		    dst = 1; //north
		    break;
	        case MeshRouter::IN_NORTH: //packet from north goes west
		    dst = 3; //west
		    break;
	        case MeshRouter::IN_WEST: //packet from west goes south
		    dst = 7; //south
		    break;
	        case MeshRouter::IN_SOUTH: //packet from south goes east
		    dst = 5; //east
		    break;
	        case MeshRouter::IN_EAST: //packet from east goes ni
		    dst = 4; //self
		    break;
	    }
	    NetworkPkt* pkt = new NetworkPkt;
	    pkt->type = NetworkPkt::DATA;
	    pkt->dst = dst;

	    if(i != MeshRouter::IN_NI)
		router->handle_router(i, pkt);
	    else
		router->handle_interface(i, pkt);
	}

	Manifold::unhalt();

	Ticks_t When = 1;

	//call rising_edge()
	Manifold::Schedule(When, test_rising_edge_helper, router);
	When += 1;

	Manifold::StopAt(When);
	Manifold::Run(); //get the events processed

        //verify all sinks receive a data pkt.
	CPPUNIT_ASSERT_EQUAL(1, (int)sinkni->get_pkts().size());
	CPPUNIT_ASSERT_EQUAL(1, (int)sink1->get_pkts().size());
	CPPUNIT_ASSERT_EQUAL(1, (int)sink3->get_pkts().size());
	CPPUNIT_ASSERT_EQUAL(1, (int)sink5->get_pkts().size());
	CPPUNIT_ASSERT_EQUAL(1, (int)sink7->get_pkts().size());

        //verify the packets are delivered at the same cycle
	CPPUNIT_ASSERT_EQUAL(sinkni->get_ticks().front(), sink1->get_ticks().front());
	CPPUNIT_ASSERT_EQUAL(sinkni->get_ticks().front(), sink3->get_ticks().front());
	CPPUNIT_ASSERT_EQUAL(sinkni->get_ticks().front(), sink5->get_ticks().front());
	CPPUNIT_ASSERT_EQUAL(sinkni->get_ticks().front(), sink7->get_ticks().front());

	delete router;
	delete sinkni;
	delete sink1;
	delete sink3;
	delete sink5;
	delete sink7;
    }





    //======================================================================
    //======================================================================
    //! @brief Test routing.
    //!
    //! Create a MeshRouter (ID 5) in a 4x4 network. Randomly send packets from
    //! west and north with random destinations. Verify the packets are forwarded
    //! as expected.
    //!
    //! The router is connected to 5 sink objects.
    //!
    //!    0   1   2  3
    //!    4  (5)  6  7
    //!    8   9  10  11
    //!   12  13  14  15
    //!
    //!                 | sink 1 |  | sinkni (as NI) |
    //!                         |  /
    //!  ID:       4          5 | /           6
    //!        | sink |<------| R |------->| sink |
    //!                         |
    //!                      | sink 9 |
    void testRising_edge_5()
    {
        //create a router
	const int X = 4; 
	const int Y = 4;
	const int ID = 5;
	const int BUFSIZE = random() % 100 + 100;
	CompId_t router_cid = Component :: Create<MeshRouter>(0, ID, X, Y, BUFSIZE);
	MeshRouter* router = Component::GetComponent<MeshRouter>(router_cid);

        //create 5 sinks
	CompId_t sink1_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink1 = Component::GetComponent<MockRouter>(sink1_cid);
	CompId_t sinkni_cid = Component :: Create<MockRouter>(0);
	MockRouter* sinkni = Component::GetComponent<MockRouter>(sinkni_cid);
	CompId_t sink4_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink4 = Component::GetComponent<MockRouter>(sink4_cid);
	CompId_t sink6_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink6 = Component::GetComponent<MockRouter>(sink6_cid);
	CompId_t sink9_cid = Component :: Create<MockRouter>(0);
	MockRouter* sink9 = Component::GetComponent<MockRouter>(sink9_cid);

        //connect the components
	//router to sinkni (sinkni acts as NI)
	Manifold::Connect(router_cid, MeshRouter::OUT_NI, sinkni_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink1
	Manifold::Connect(router_cid, MeshRouter::OUT_NORTH, sink1_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink4
	Manifold::Connect(router_cid, MeshRouter::OUT_WEST, sink4_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink6
	Manifold::Connect(router_cid, MeshRouter::OUT_EAST, sink6_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);
	//router to sink9
	Manifold::Connect(router_cid, MeshRouter::OUT_SOUTH, sink9_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, 1);


	int west_pkts = 0;
	int north_pkts = 0;

	while(west_pkts < BUFSIZE || north_pkts < BUFSIZE) {
	    if(west_pkts < BUFSIZE) {
		if(random() / (RAND_MAX+1.0) < 0.5) { //50% chance, enter a packet
		    NetworkPkt* pkt = new NetworkPkt;
		    pkt->type = NetworkPkt::DATA;
		    //packet from node 4 to node 5 cannot have 0, 4, 8, 12 as destination
		    while((pkt->dst = random() %16) % 4 ==0);

		    router->handle_router(MeshRouter::IN_WEST, pkt);
		    west_pkts++;
		}
	    }
	    if(north_pkts < BUFSIZE) {
		if(random() / (RAND_MAX+1.0) < 0.5) { //50% chance, enter a packet
		    NetworkPkt* pkt = new NetworkPkt;
		    pkt->type = NetworkPkt::DATA;
		    //packet from node 1 to node 5 cannot have 0, 1, 2, 3 as destination
		    while((pkt->dst = random() %16) < 4);

		    router->handle_router(MeshRouter::IN_NORTH, pkt);
		    north_pkts++;
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

        //verify received packets: node 1
	//node 1 receives packets destined to 1st rown: 0, 1, 2, 3
	for(list<NetworkPkt>::iterator it=sink1->get_pkts().begin();
	      it != sink1->get_pkts().end(); ++it) {
	    CPPUNIT_ASSERT((*it).dst < 4);
	}

        //verify received packets: node 4
	//node 1 receives packets destined to 1st column: 0, 4, 8, 12
	for(list<NetworkPkt>::iterator it=sink4->get_pkts().begin();
	      it != sink4->get_pkts().end(); ++it) {
	    CPPUNIT_ASSERT((*it).dst % 4 == 0);
	}

        //verify received packets: node 6
	//node 1 receives packets destined to the right-most 2 columns
	for(list<NetworkPkt>::iterator it=sink6->get_pkts().begin();
	      it != sink6->get_pkts().end(); ++it) {
	    CPPUNIT_ASSERT((*it).dst % 4 > 1);
	}

        //verify received packets: node 9
	//node 1 receives packets destined to the bottom 2 rows
	for(list<NetworkPkt>::iterator it=sink9->get_pkts().begin();
	      it != sink9->get_pkts().end(); ++it) {
	    CPPUNIT_ASSERT((*it).dst > 7);
	}

	delete router;
	delete sinkni;
	delete sink1;
	delete sink4;
	delete sink6;
	delete sink9;

    }














    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MeshRouterTest");

	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testConstructor_0", &MeshRouterTest::testConstructor_0));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testConstructor_1", &MeshRouterTest::testConstructor_1));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testHandle_interface_0", &MeshRouterTest::testHandle_interface_0));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testHandle_interface_1", &MeshRouterTest::testHandle_interface_1));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testHandle_interface_2", &MeshRouterTest::testHandle_interface_2));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testHandle_router_0", &MeshRouterTest::testHandle_router_0));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testHandle_router_1", &MeshRouterTest::testHandle_router_1));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testHandle_router_2", &MeshRouterTest::testHandle_router_2));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testRising_edge_0", &MeshRouterTest::testRising_edge_0));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testRising_edge_1", &MeshRouterTest::testRising_edge_1));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testRising_edge_2", &MeshRouterTest::testRising_edge_2));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testRising_edge_3", &MeshRouterTest::testRising_edge_3));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testRising_edge_4", &MeshRouterTest::testRising_edge_4));
	mySuite->addTest(new CppUnit::TestCaller<MeshRouterTest>("testRising_edge_5", &MeshRouterTest::testRising_edge_4));

	return mySuite;
    }
};


Clock MeshRouterTest :: MasterClock(MeshRouterTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold::Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MeshRouterTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


