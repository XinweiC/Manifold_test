//Test virtual network with FCFS router.
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
    //! @brief Test do_vc_allocation(): head flit
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from both its interface and the West port, destined to
    //! East. Verify both vc states are FULL; call do_route_computing, both vc
    //! states change to VCA_REQUESTED; call do_vc_allocation, one input gets
    //! an output VC and its state changes to SWA_REQUESTED; the other stays in
    //! VCA_REQUESTED. Call do_vc_allocation() again; the 2nd vc stays in VCA_REQUESTED
    //! since it is allocated to the same output vc, because the 2 flits are for
    //! the same virtual network.
    void test_do_vc_allocation_0()
    {
	const unsigned VCS = 4;

	router_init_params router_params;
	router_params.no_nodes = 4;
	router_params.grid_size = 2; 
	router_params.no_ports = 5;
	router_params.no_vcs = VCS;
	router_params.credits = 4;
	router_params.rc_method= RING_ROUTING;

	unsigned NODE_ID = 1;

	SimpleRouter* router = new SimpleRouter(NODE_ID, &router_params);


	HeadFlit* hflit0 = new HeadFlit();
	hflit0->src_id = NODE_ID;  //from interface
	hflit0->dst_id = NODE_ID + 1;  //to East
	hflit0->mclass = (random() % 2 == 0) ? PROC_REQ : MC_RESP;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = hflit0->mclass; //same virtual network as hflit0

	LinkData* lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = hflit1;
	const unsigned VC1 = random() % VCS;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state before calling do_route_computing()
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);
	router->do_route_computing();

	//both advance to VCA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	router->do_vc_allocation();

	//only 1 advances to SWA_REQUESTED; the FCFS VC allocator allows only 1 input vc to be
	//allocated an output vc for each port each tick.
	CPPUNIT_ASSERT(SimpleRouter::PORT_NI < SimpleRouter::PORT_WEST);
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	//call again
	router->do_vc_allocation();

        //verify the 2nd vc stays in VCA_REQUESTED.
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	delete router;

    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test do_vc_allocation(): head flit
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from both its interface and the West port, destined to
    //! East. Verify both vc states are FULL; call do_route_computing, both vc
    //! states change to VCA_REQUESTED; call do_vc_allocation, both inputs get
    //! an output VC and state changes to SWA_REQUESTED;
    //! since they request different output vc, because the 2 flits are for
    //! different virtual network.
    //!
    //! This test case is same as above, except the 2 flits use different virtual networks.
    void test_do_vc_allocation_1()
    {
	const unsigned VCS = 4;

	router_init_params router_params;
	router_params.no_nodes = 4;
	router_params.grid_size = 2; 
	router_params.no_ports = 5;
	router_params.no_vcs = VCS;
	router_params.credits = 4;
	router_params.rc_method= RING_ROUTING;

	unsigned NODE_ID = 1;

	SimpleRouter* router = new SimpleRouter(NODE_ID, &router_params);


	HeadFlit* hflit0 = new HeadFlit();
	hflit0->src_id = NODE_ID;  //from interface
	hflit0->dst_id = NODE_ID + 1;  //to East
	hflit0->mclass = (random() % 2 == 0) ? PROC_REQ : MC_RESP;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = (hflit0->mclass == PROC_REQ) ? MC_RESP : PROC_REQ; //different from hflit0->mclass

	LinkData* lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = hflit1;
	const unsigned VC1 = random() % VCS;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state before calling do_route_computing()
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);
	router->do_route_computing();

	//both advance to VCA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	router->do_vc_allocation();

	//only 1 advances to SWA_REQUESTED; the FCFS VC allocator allows only 1 input vc to be
	//allocated an output vc for each port each tick.
	CPPUNIT_ASSERT(SimpleRouter::PORT_NI < SimpleRouter::PORT_WEST);
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	delete router;

    } 

    


    
    //======================================================================
    //======================================================================
    //! @brief Test do_switch_allocation(): head flit; FULL->VCA_REQUESTED->
    //! SWA_REQUESTED->SW_TRAVERSAL
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from both its interface and the West port, destined to
    //! East. Verify both vc states are FULL; call do_route_computing, both vc
    //! states change to VCA_REQUESTED; call do_vc_allocation, one vc changes
    //! to SWA_REQUESTED; now call do_switch_allocation(), the one in SWA_REQUESTED
    //! advances to SW_TRAVERSAL.
    void test_do_switch_allocation_0()
    {
        //both requests use same vnet, after 2nd call of do_vc_alloc(), one stays
	//in VCA_REQUESTED; call do_switch_allo(), the one in SWA_REQU moves to
	//SW_TRAVERSAL with no competetion.
	const unsigned VCS = 4; //must be 4

	router_init_params router_params;
	router_params.no_nodes = 4;
	router_params.grid_size = 2; 
	router_params.no_ports = 5;
	router_params.no_vcs = VCS;
	router_params.credits = 4;
	router_params.rc_method= RING_ROUTING;

	unsigned NODE_ID = 1;

	SimpleRouter* router = new SimpleRouter(NODE_ID, &router_params);


	HeadFlit* hflit0 = new HeadFlit();
	hflit0->src_id = NODE_ID;  //from interface
	hflit0->dst_id = NODE_ID + 1;  //to East
	hflit0->mclass = (random() % 2 == 0) ? PROC_REQ : MC_RESP;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = hflit0->mclass; //same virtual network as hflit0

	LinkData* lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = hflit1;
	const unsigned VC1 = random() % VCS;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state before calling do_route_computing()
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);
	router->do_route_computing();

	//both advance to VCA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	router->do_vc_allocation();

	//since both use same virtual network, only one changes to SWA_REQUESTED.
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

        //now call do_switch_allocation(); only one moves to SW_TRAVERSAL
        router->do_switch_allocation();
	CPPUNIT_ASSERT_EQUAL(SW_TRAVERSAL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	delete router;

    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test do_switch_allocation(): head flit; FULL->VCA_REQUESTED->
    //! SWA_REQUESTED->SW_TRAVERSAL
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from both its interface and the West port, with different
    //! destinations. Verify both vc states are FULL; call do_route_computing, both vc
    //! states change to VCA_REQUESTED; call do_vc_allocation, both states change
    //! to SWA_REQUESTED; now call do_switch_allocation(), one would advance to
    //! SW_TRAVERSAL.
    void test_do_switch_allocation_1()
    {
        //requests use different vnet, after 2nd call of do_vc_alloc(), both
	//in SWA_REQUESTED; call do_switch_allo(), only one moves to
	//SW_TRAVERSAL.
	const unsigned VCS = 4; //must be 4

	router_init_params router_params;
	router_params.no_nodes = 4;
	router_params.grid_size = 2; 
	router_params.no_ports = 5;
	router_params.no_vcs = VCS;
	router_params.credits = 4;
	router_params.rc_method= RING_ROUTING;

	unsigned NODE_ID = 1;

	SimpleRouter* router = new SimpleRouter(NODE_ID, &router_params);


	HeadFlit* hflit0 = new HeadFlit();
	hflit0->src_id = NODE_ID;  //from interface
	hflit0->dst_id = NODE_ID + 1;  //to East
	hflit0->mclass = (random() % 2 == 0) ? PROC_REQ : MC_RESP;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = (hflit0->mclass == PROC_REQ) ? MC_RESP : PROC_REQ; //different from hflit0->mclass

	LinkData* lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = hflit1;
	const unsigned VC1 = random() % VCS;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state before calling do_route_computing()
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);
	router->do_route_computing();

	//both advance to VCA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	router->do_vc_allocation();

	//since both use different virtual network, both change to SWA_REQUESTED.
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	//call do_vc_allocation(), both should be in SWA_REQUESTED.
	router->do_vc_allocation();
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

        //now call do_switch_allocation(); only one moves to SW_TRAVERSAL
        router->do_switch_allocation();
	CPPUNIT_ASSERT_EQUAL(SW_TRAVERSAL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	delete router;

    } 

    
    void test_do_switch_allocation_2()
    {
        //both requests use same vnet but go to different ports,
	const unsigned VCS = 4;

	router_init_params router_params;
	router_params.no_nodes = 4;
	router_params.grid_size = 2; 
	router_params.no_ports = 5;
	router_params.no_vcs = VCS;
	router_params.credits = 4;
	router_params.rc_method= RING_ROUTING;

	unsigned NODE_ID = 1;

	SimpleRouter* router = new SimpleRouter(NODE_ID, &router_params);


	HeadFlit* hflit0 = new HeadFlit();
	hflit0->src_id = NODE_ID;  //from interface
	hflit0->dst_id = NODE_ID + 1;  //to East
	hflit0->mclass = (random() % 2 == 0) ? PROC_REQ : MC_RESP;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID;  //to interface
	hflit1->mclass = hflit0->mclass; //same as hflit0

	LinkData* lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = hflit1;
	const unsigned VC1 = random() % VCS;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state before calling do_route_computing()
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);
	router->do_route_computing();

	//both advance to VCA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

	router->do_vc_allocation();

	//both would advance to SWA_REQUESTED because they use different output ports
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

        //now call do_switch_allocation(); both move to SW_TRAVERSAL
	//#############################
        router->do_switch_allocation();
	//#############################
	CPPUNIT_ASSERT_EQUAL(SW_TRAVERSAL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(SW_TRAVERSAL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);


	delete router;

    }



    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("SimpleRouterTest");

	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_vc_allocation_0", &SimpleRouterTest::test_do_vc_allocation_0));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_vc_allocation_1", &SimpleRouterTest::test_do_vc_allocation_1));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_allocation_0", &SimpleRouterTest::test_do_switch_allocation_0));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_allocation_1", &SimpleRouterTest::test_do_switch_allocation_1));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_allocation_2", &SimpleRouterTest::test_do_switch_allocation_2));
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


