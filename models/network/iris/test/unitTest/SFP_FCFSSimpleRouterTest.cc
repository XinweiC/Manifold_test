//Test single-flit packet (SFP) with FCFS SimpleRouter.
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
#include "../../components/simpleRouter.h"

#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;
using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################

class SinkRouter : public Component {
public:
    enum {IN=0};

    void handle_input(int, LinkData* d)
    {
        m_data.push_back(d);    
/*
cout << this << " received ";
if(d->type == FLIT) {
    if(d->f->type == HEAD)
        cout << "HEAD\n";
    else if(d->f->type == BODY)
        cout << "BODY\n";
    else if(d->f->type == TAIL)
        cout << "TAIL\n";
}
else {
cout << "CREDIT\n";
}
*/
    }

    vector<LinkData*>& get_data() { return m_data; }
private:
    vector<LinkData*> m_data;
};



//####################################################################
//! Class SimpleRouterTest is the test class for class SimpleRouter. 
//####################################################################
class SimpleRouterTest : public CppUnit::TestFixture {
private:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };


public:
    

    //======================================================================
    //======================================================================
    //! @brief Test do_switch_traversal(): traversal of single-flit releases output VC
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from its interface, destined to East. Verify vc state is FULL;
    //! call do_vc_allocation, the vc state changes to VCA_REQUESTED; call do_vc_allocation
    //! again, state changes to SWA_REQUESTED; call do_switch_allocation(), the sate changes
    //! to SW_TRAVERSAL; call do_switch_traversal(), the single flit traverses.
    //! verify the output VC is released.
    void test_do_switch_traversal_4()
    {
	const unsigned VCS = 4; //must be 4

	router_init_params router_params;
	router_params.no_nodes = 4;
	router_params.grid_size = 2; 
	router_params.no_ports = 5;
	router_params.no_vcs = VCS;
	router_params.credits = 4;
	router_params.rc_method= RING_ROUTING;

	unsigned NODE_ID = 1;

	//do_switch_traversal() calls Send(), so must connect components.

	//create a SimpleRouter, a Sink as interface, and a Sink as another router.
	CompId_t if_id = Component :: Create<SinkRouter> (0);
	SinkRouter* iface = Component :: GetComponent<SinkRouter>(if_id);

	CompId_t router_id = Component :: Create<SimpleRouter> (0, NODE_ID, &router_params);
	SimpleRouter* router = Component :: GetComponent<SimpleRouter>(router_id);

	CompId_t sink_id = Component :: Create<SinkRouter> (0);
	SinkRouter* sink = Component :: GetComponent<SinkRouter>(sink_id);

	//connect the router to the other components
	Manifold::Connect(router_id, SimpleRouter::PORT_NI, if_id, SinkRouter::IN,
                          &SinkRouter::handle_input,1);

	Manifold::Connect(router_id, SimpleRouter::PORT_EAST, sink_id, SinkRouter::IN,
                          &SinkRouter::handle_input,1);


	HeadFlit* hflit0 = new HeadFlit();
	hflit0->src_id = NODE_ID;  //from interface
	hflit0->dst_id = NODE_ID + 1;  //to East
	hflit0->mclass = PROC_REQ;
	hflit0->pkt_length = 1; //####### set packet length to 1

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	//enter the flit into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);

	//verify virtual channel state before calling do_route_computing()
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	router->do_route_computing();

	//state changes to VCA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

	router->do_vc_allocation();

	//state changes to SWA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

        //call do_switch_allocation(); state changes to SW_TRAVERSAL
        router->do_switch_allocation();
	CPPUNIT_ASSERT_EQUAL(SW_TRAVERSAL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

	unsigned oport = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_port;
	unsigned ovc = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_channel;

	//############################
        router->do_switch_traversal();
	//############################


	//verify output VC is released
	CPPUNIT_ASSERT_EQUAL(false, (bool)router->vca->ovc_taken[oport][ovc]);
	//verify SWA request bit is off
	CPPUNIT_ASSERT_EQUAL(false, (bool)router->swa->requested[oport][SimpleRouter::PORT_NI*VCS + VC0]);


	Manifold::unhalt();
	Manifold::StopAt(2); //ensure stop at time is > link delay
	Manifold::Run();

	//verify delivery of flit
	vector<LinkData*> ldata = sink->get_data();
	CPPUNIT_ASSERT_EQUAL(1, (int)ldata.size());
	CPPUNIT_ASSERT_EQUAL(HEAD, ldata[0]->f->type);
	//and credit
	vector<LinkData*> ldata_ni = iface->get_data();
	CPPUNIT_ASSERT_EQUAL(1, (int)ldata_ni.size());
	CPPUNIT_ASSERT_EQUAL(CREDIT, ldata_ni[0]->type);


	delete router;
	delete iface;
	delete sink;
    } 

    



    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("SimpleRouterTest");

	/*
	*/
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_traversal_0", &SimpleRouterTest::test_do_switch_traversal_4));
	/*
	*/
	return mySuite;
    }
};

Clock SimpleRouterTest :: MasterClock(SimpleRouterTest :: MASTER_CLOCK_HZ);


int main()
{
    Manifold::Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( SimpleRouterTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


