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
    //! @brief Test handle_link_arrival(): head flit; VC state is FULL.
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Manually set the state of the input VC to a EMPTY or PS_INVALID.
    //! Enter a HEAD flit from both its interface and the West port, destined to
    //! East. Verify both vc states are FULL;
    void test_handle_link_arrival_0()
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
	hflit0->mclass = PROC_REQ;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = PROC_REQ;

	LinkData* lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = hflit1;
	const unsigned VC1 = random() % VCS;
	lkdata1->vc = VC1;

        //manually set the input vcs' state to a random value
	router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage = (random() % 2 == 0) ? PS_INVALID : EMPTY;
	router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage = (random() % 2 == 0) ? PS_INVALID : EMPTY; //currently 10 possible states

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state is FULL
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);


	delete router;
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test handle_link_arrival(): body flit; VC state stays FULL
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a HEAD flit from its interface and the West port, destined to
    //! East. Then enter a BODY flit; verify the BODY flit does not change the
    //! VC state.
    void test_handle_link_arrival_1()
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
	hflit0->mclass = PROC_REQ;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = PROC_REQ;

	LinkData* lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = hflit1;
	const unsigned VC1 = random() % VCS;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state is FULL
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);


	BodyFlit* bflit0 = new BodyFlit();

	lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = bflit0;
	lkdata0->vc = VC0;


	BodyFlit* bflit1 = new BodyFlit();

	lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = bflit1;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state is FULL
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);


	delete router;
    } 

    

    //======================================================================
    //======================================================================
    //! @brief Test handle_link_arrival(): TAIL flit; VC state stays FULL
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a HEAD flit from its interface and the West port, destined to
    //! East. Then enter a TAIL flit; verify the TAIL flit does not change the
    //! VC state.
    void test_handle_link_arrival_2()
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
	hflit0->mclass = PROC_REQ;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = PROC_REQ;

	LinkData* lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = hflit1;
	const unsigned VC1 = random() % VCS;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state is FULL
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);


	TailFlit* tflit0 = new TailFlit();

	lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = tflit0;
	lkdata0->vc = VC0;


	TailFlit* tflit1 = new TailFlit();

	lkdata1 = new LinkData();
	lkdata1->type = FLIT;
	lkdata1->f = tflit1;
	lkdata1->vc = VC1;

	//enter the flits into the router
	router->handle_link_arrival(SimpleRouter::PORT_NI, lkdata0);
	router->handle_link_arrival(SimpleRouter::PORT_WEST, lkdata1);

	//verify virtual channel state is FULL
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(FULL, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);


	delete router;
    } 

    


    

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
    //! since it is allocated to the same output vc.
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
	hflit0->mclass = PROC_REQ;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = PROC_REQ; //same virtual network as the flit above.

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

	//After callind do_vc_allocation(), only 1 advances to SWA_REQUESTED; the FCFS VC allocator allows only 1 input vc to be
	//allocated an output vc for each port each tick.
	CPPUNIT_ASSERT(SimpleRouter::PORT_NI < SimpleRouter::PORT_WEST);
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(2, (int)router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_channel);

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
    //! East. Set all output vc to busy. Verify both vc states are FULL; call do_route_computing, both vc
    //! states change to VCA_REQUESTED; call do_vc_allocation, both stay in
    //! to VCA_REQUESTED since output vcs are busy.
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


	//manually mark all VCs on the output port as taken.
	for(unsigned i=0; i<VCS; i++) {
	    router->vca->ovc_taken[SimpleRouter::PORT_EAST][i] = true;
	}


	HeadFlit* hflit0 = new HeadFlit();
	hflit0->src_id = NODE_ID;  //from interface
	hflit0->dst_id = NODE_ID + 1;  //to East
	hflit0->mclass = PROC_REQ;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;



	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = PROC_REQ;

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

	//both stay in VCA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);
	CPPUNIT_ASSERT_EQUAL(VCA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_WEST*VCS + VC1].pipe_stage);

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
    //! to SWA_REQUESTED; now call do_switch_allocation(), the vc changes
    //! to SW_TRAVERSAL.
    void test_do_switch_allocation_0()
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

	SimpleRouter* router = new SimpleRouter(NODE_ID, &router_params);


	HeadFlit* hflit0 = new HeadFlit();
	hflit0->src_id = NODE_ID;  //from interface
	hflit0->dst_id = NODE_ID + 1;  //to East
	hflit0->mclass = PROC_REQ;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID + 1;  //to East
	hflit1->mclass = PROC_REQ;

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
    //! to SWA_REQUESTED; now call do_switch_allocation(), bot would advance to
    //! SW_TRAVERSAL because they go to different output ports.
    void test_do_switch_allocation_1()
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
	hflit0->mclass = PROC_REQ;

	LinkData* lkdata0 = new LinkData();
	lkdata0->type = FLIT;
	lkdata0->f = hflit0;
	const unsigned VC0 = random() % VCS;
	lkdata0->vc = VC0;


	HeadFlit* hflit1 = new HeadFlit();
	hflit1->src_id = NODE_ID - 1;  //from West
	hflit1->dst_id = NODE_ID;  //to interface
	hflit1->mclass = PROC_REQ;

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

	//both would advance to SWA_REQUESTED because there are 2 VCs.
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

    

    //======================================================================
    //======================================================================
    //! @brief Test do_switch_traversal(): state change; FULL->VCA_REQUESTED->
    //! SWA_REQUESTED->SW_TRAVERSAL->SWA_REQUESTED
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from its interface, destined to East. Verify vc state is FULL;
    //! call do_route_computing, the vc state changes to VCA_REQUESTED; call do_vc_allocation,
    //! state changes to SWA_REQUESTED; call do_switch_allocation(), the sate changes
    //! to SW_TRAVERSAL; now put another flit in its buffer and call do_switch_traversal();
    //! state should change back to SWA_REQUESTED.
    void test_do_switch_traversal_0()
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

        //now put another flit in its input buffer, and call do_switch_traversal(), state should cahnge to SWA_REQUESTED.
	BodyFlit* bflit = new BodyFlit();
	router->in_buffers[SimpleRouter::PORT_NI]->push(VC0, bflit);
	//also ensure credits >0 for this to work
	unsigned oport = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_port;
	unsigned ovc = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_channel;
	CPPUNIT_ASSERT(router->downstream_credits[oport][ovc] > 0);

        //There should be 2 flits in the input buffer.
	CPPUNIT_ASSERT_EQUAL(2, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//############################
        router->do_switch_traversal();
	//############################

        //There should be 1 flit in the input buffer.
	CPPUNIT_ASSERT_EQUAL(1, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//verify state goes to SWA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

	//verify output VC is not released
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->vca->ovc_taken[oport][ovc]);
	//verify SWA request bit is on
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->swa->requested[oport][SimpleRouter::PORT_NI*VCS + VC0]);


	Manifold::unhalt();
	Manifold::StopAt(2); //ensure stop at time is > link delay
	Manifold::Run();

	//verify delivery of flit
	vector<LinkData*> ldata = sink->get_data();
	CPPUNIT_ASSERT_EQUAL(FLIT, ldata[0]->type);
	//and credit
	vector<LinkData*> ldata_ni = iface->get_data();
	CPPUNIT_ASSERT_EQUAL(CREDIT, ldata_ni[0]->type);


	delete router;
	delete iface;
	delete sink;
    } 

    



    //======================================================================
    //======================================================================
    //! @brief Test do_switch_traversal(): state change; FULL->VCA_REQUESTED->
    //! SWA_REQUESTED->SW_TRAVERSAL->VCA_COMPLETE
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from its interface, destined to East. Verify vc state is FULL;
    //! call do_route_computing, the vc state changes to VCA_REQUESTED; call do_vc_allocation,
    //! state changes to SWA_REQUESTED; call do_switch_allocation(), the sate changes
    //! to SW_TRAVERSAL; now put another flit in its buffer and call do_switch_traversal();
    //! manually change the credits to 1; state should change back to VCA_COMPLETE.
    void test_do_switch_traversal_1()
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

        //now put another flit in its input buffer, set credit to 1, and call do_switch_traversal(),
	//state should cahnge to VCA_COMPLETE.
	BodyFlit* bflit = new BodyFlit();
	router->in_buffers[SimpleRouter::PORT_NI]->push(VC0, bflit);
	unsigned oport = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_port;
	unsigned ovc = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_channel;
	router->downstream_credits[oport][ovc] = 1;

        //There should be 2 flits in the input buffer.
	CPPUNIT_ASSERT_EQUAL(2, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//############################
        router->do_switch_traversal();
	//############################

        //There should be 1 flit in the input buffer.
	CPPUNIT_ASSERT_EQUAL(1, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//verify state goes to VCA_COMPLETE
	CPPUNIT_ASSERT_EQUAL(VCA_COMPLETE, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

	//verify output VC is not released
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->vca->ovc_taken[oport][ovc]);
	//verify SWA request bit is off
	CPPUNIT_ASSERT_EQUAL(false, (bool)router->swa->requested[oport][SimpleRouter::PORT_NI*VCS + VC0]);

	Manifold::unhalt();
	Manifold::StopAt(2); //ensure stop at time is > link delay
	Manifold::Run();

	//verify delivery of flit
	vector<LinkData*> ldata = sink->get_data();
	CPPUNIT_ASSERT_EQUAL(FLIT, ldata[0]->type);
	//and credit
	vector<LinkData*> ldata_ni = iface->get_data();
	CPPUNIT_ASSERT_EQUAL(CREDIT, ldata_ni[0]->type);

        delete bflit;

	delete router;
	delete iface;
	delete sink;
    } 

    



    //======================================================================
    //======================================================================
    //! @brief Test do_switch_traversal(): state change; FULL->VCA_REQUESTED->
    //! SWA_REQUESTED->SW_TRAVERSAL->VCA_COMPLETE
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from its interface, destined to East. Verify vc state is FULL;
    //! call do_route_computing, the vc state changes to VCA_REQUESTED; call do_vc_allocation,
    //! state changes to SWA_REQUESTED; call do_switch_allocation(), the sate changes
    //! to SW_TRAVERSAL; now its input buffer is empty; call do_switch_traversal();
    //! state should change back to VCA_COMPLETE.
    void test_do_switch_traversal_2()
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

        //now the input buffer is empty, and call do_switch_traversal(),
	//state should cahnge to VCA_COMPLETE.
	unsigned oport = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_port;
	unsigned ovc = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_channel;
	CPPUNIT_ASSERT(router->downstream_credits[oport][ovc] > 1); //enough credit

        //There should be 1 flit in the input buffer.
	CPPUNIT_ASSERT_EQUAL(1, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//############################
        router->do_switch_traversal();
	//############################

        //There should be no flit in the input buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//verify state goes to VCA_COMPLETE
	CPPUNIT_ASSERT_EQUAL(VCA_COMPLETE, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

	//verify output VC is not released
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->vca->ovc_taken[oport][ovc]);
	//verify SWA request bit is off
	CPPUNIT_ASSERT_EQUAL(false, (bool)router->swa->requested[oport][SimpleRouter::PORT_NI*VCS + VC0]);

	Manifold::unhalt();
	Manifold::StopAt(2); //ensure stop at time is > link delay
	Manifold::Run();

	//verify delivery of flit
	vector<LinkData*> ldata = sink->get_data();
	CPPUNIT_ASSERT_EQUAL(FLIT, ldata[0]->type);
	//and credit
	vector<LinkData*> ldata_ni = iface->get_data();
	CPPUNIT_ASSERT_EQUAL(CREDIT, ldata_ni[0]->type);

	delete router;
	delete iface;
	delete sink;
    } 

    


    


    //======================================================================
    //======================================================================
    //! @brief Test do_switch_traversal(): traversal of BODY flit keeps output VC
    //! reserved.
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from its interface, destined to East. Verify vc state is FULL;
    //! call do_route_computing, the vc state changes to VCA_REQUESTED; call do_vc_allocation,
    //! state changes to SWA_REQUESTED; call do_switch_allocation(), the sate changes
    //! to SW_TRAVERSAL; now put a BODY flit in its buffer and call do_switch_traversal();
    //! the BODY flit traverses; verify the output VC is still taken.
    void test_do_switch_traversal_3()
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

        //now put another flit in its input buffer, set credit to 1, and call do_switch_traversal(),
	//state should cahnge to VCA_COMPLETE.
	BodyFlit* bflit = new BodyFlit();
	router->in_buffers[SimpleRouter::PORT_NI]->push(VC0, bflit);
	unsigned oport = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_port;
	unsigned ovc = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_channel;

        //There should be 2 flits in the input buffer.
	CPPUNIT_ASSERT_EQUAL(2, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//############################
        router->do_switch_traversal();
	//############################

        //There should be 1 flit in the input buffer.
	CPPUNIT_ASSERT_EQUAL(1, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//verify state goes to SWA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

	//verify output VC is not released
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->vca->ovc_taken[oport][ovc]);
	//verify SWA request bit is on
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->swa->requested[oport][SimpleRouter::PORT_NI*VCS + VC0]);

        //call do_switch_allocation() to put it back to SW_TRAVERSAL
        router->do_switch_allocation();
	CPPUNIT_ASSERT_EQUAL(SW_TRAVERSAL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

	//############################
        router->do_switch_traversal();
	//############################

	//verify output VC is not released
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->vca->ovc_taken[oport][ovc]);
	//verify SWA request bit is off
	CPPUNIT_ASSERT_EQUAL(false, (bool)router->swa->requested[oport][SimpleRouter::PORT_NI*VCS + VC0]);


	Manifold::unhalt();
	Manifold::StopAt(2); //ensure stop at time is > link delay
	Manifold::Run();

	//verify delivery of flit
	vector<LinkData*> ldata = sink->get_data();
	CPPUNIT_ASSERT_EQUAL(2, (int)ldata.size());
	CPPUNIT_ASSERT_EQUAL(HEAD, ldata[0]->f->type);
	CPPUNIT_ASSERT_EQUAL(BODY, ldata[1]->f->type);
	//and credit
	vector<LinkData*> ldata_ni = iface->get_data();
	CPPUNIT_ASSERT_EQUAL(2, (int)ldata_ni.size());
	CPPUNIT_ASSERT_EQUAL(CREDIT, ldata_ni[0]->type);
	CPPUNIT_ASSERT_EQUAL(CREDIT, ldata_ni[1]->type);


	delete router;
	delete iface;
	delete sink;
    } 

    



    //======================================================================
    //======================================================================
    //! @brief Test do_switch_traversal(): traversal of TAIL flit releases output VC
    //!
    //! Create a SimpleRouter in a 4-node ring network. The router's id is 1. 
    //! Enter a head flit from its interface, destined to East. Verify vc state is FULL;
    //! call do_route_computing, the vc state changes to VCA_REQUESTED; call do_vc_allocation,
    //! state changes to SWA_REQUESTED; call do_switch_allocation(), the sate changes
    //! to SW_TRAVERSAL; now put a TAIL flit in its buffer and call do_switch_traversal();
    //! the TAIL flit traverses; verify the output VC is released.
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

        //now put another flit in its input buffer, set credit to 1, and call do_switch_traversal(),
	//state should cahnge to VCA_COMPLETE.
	TailFlit* tflit = new TailFlit();
	router->in_buffers[SimpleRouter::PORT_NI]->push(VC0, tflit);
	unsigned oport = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_port;
	unsigned ovc = router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].output_channel;

        //There should be 2 flits in the input buffer.
	CPPUNIT_ASSERT_EQUAL(2, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//############################
        router->do_switch_traversal();
	//############################

        //There should be 1 flit in the input buffer.
	CPPUNIT_ASSERT_EQUAL(1, (int)router->in_buffers[SimpleRouter::PORT_NI]->get_occupancy(VC0));

	//verify state goes to SWA_REQUESTED
	CPPUNIT_ASSERT_EQUAL(SWA_REQUESTED, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

	//verify output VC is not released
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->vca->ovc_taken[oport][ovc]);
	//verify SWA request bit is on
	CPPUNIT_ASSERT_EQUAL(true, (bool)router->swa->requested[oport][SimpleRouter::PORT_NI*VCS + VC0]);

        //call do_switch_allocation() to put it back to SW_TRAVERSAL
        router->do_switch_allocation();
	CPPUNIT_ASSERT_EQUAL(SW_TRAVERSAL, router->input_buffer_state[SimpleRouter::PORT_NI*VCS + VC0].pipe_stage);

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
	CPPUNIT_ASSERT_EQUAL(2, (int)ldata.size());
	CPPUNIT_ASSERT_EQUAL(HEAD, ldata[0]->f->type);
	CPPUNIT_ASSERT_EQUAL(TAIL, ldata[1]->f->type);
	//and credit
	vector<LinkData*> ldata_ni = iface->get_data();
	CPPUNIT_ASSERT_EQUAL(2, (int)ldata_ni.size());
	CPPUNIT_ASSERT_EQUAL(CREDIT, ldata_ni[0]->type);
	CPPUNIT_ASSERT_EQUAL(CREDIT, ldata_ni[1]->type);


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
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_handle_link_arrival_0", &SimpleRouterTest::test_handle_link_arrival_0));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_handle_link_arrival_1", &SimpleRouterTest::test_handle_link_arrival_1));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_handle_link_arrival_2", &SimpleRouterTest::test_handle_link_arrival_2));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_vc_allocation_0", &SimpleRouterTest::test_do_vc_allocation_0));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_vc_allocation_1", &SimpleRouterTest::test_do_vc_allocation_1));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_allocation_0", &SimpleRouterTest::test_do_switch_allocation_0));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_allocation_1", &SimpleRouterTest::test_do_switch_allocation_1));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_traversal_0", &SimpleRouterTest::test_do_switch_traversal_0));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_traversal_1", &SimpleRouterTest::test_do_switch_traversal_1));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_traversal_2", &SimpleRouterTest::test_do_switch_traversal_2));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_traversal_3", &SimpleRouterTest::test_do_switch_traversal_3));
	mySuite->addTest(new CppUnit::TestCaller<SimpleRouterTest>("test_do_switch_traversal_4", &SimpleRouterTest::test_do_switch_traversal_4));
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


