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
#include "../../components/genericVcAllocator.h"

#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;
using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################



//####################################################################
//! Class RRVcAllocatorTest is the test class for class RRVcAllocator. 
//####################################################################
class RRVcAllocatorTest : public CppUnit::TestFixture {
private:
    SimpleRouter* create_router(unsigned p, unsigned v)
    {
	router_init_params rp;
	const unsigned x = random() % 20 + 2;
	const unsigned y = random() % 20 + 2;
	rp.no_nodes = x * y;
	rp.grid_size = x;
	rp.no_ports = p;
	rp.no_vcs = v;
	rp.credits = random() % 5 + 1;
	rp.rc_method = TORUS_ROUTING;

	SimpleRouter* router = new SimpleRouter(random() % 1024, &rp);
	return router;
    }

public:
    
    //======================================================================
    //======================================================================
    //! @brief Test constructor.
    //!
    void test_Constructor_0()
    {
        const int N=1;

	for(int i=0; i<N; i++) {
	    //unsigned VCS = random() % 10 + 1; //1 to 10 virtual channels
	    unsigned VCS = 4; //must be 4
	    unsigned PORTS = random() % 9 + 2; //2 to 10 ports

	    SimpleRouter* router = create_router(PORTS, VCS);
	    RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	    //CPPUNIT_ASSERT_EQUAL(PORTS, vca->ports);
	    CPPUNIT_ASSERT_EQUAL(VCS, vca->VCS);
	    for(unsigned p=0; p<PORTS; p++) {
		for(unsigned v=0; v<VCS; v++) {
		    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->ovc_taken[p][v]));
		    CPPUNIT_ASSERT_EQUAL(-1, (int)(vca->last_winner[p][v]));
		}
		for(unsigned i=0; i<PORTS*VCS; i++) {
		    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->requested[p][i].is_valid));
		}
	    }
	    delete vca;
	    delete router;
	}
    }


    //======================================================================
    //======================================================================
    //! @brief Test request()
    //!
    //! Create a RRVcAllocator; call request(); verify the corresponding request
    //! bit is turned on.
    void test_request_0()
    {
	//const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	//const unsigned OUT_VC = random() % VCS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC = random() % VCS;

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT, 0, IN_PORT, IN_VC);

        //verify the request bit is turned on
	for(unsigned j=0; j<PORTS*VCS; j++) {
	    bool requested = vca->requested[OUT_PORT][j].is_valid;
	    if(j == IN_PORT*VCS + IN_VC)
		CPPUNIT_ASSERT_EQUAL(true, requested);
	    else
		CPPUNIT_ASSERT_EQUAL(false, requested);
	}
	delete vca;
	delete router;
    }



    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a RRVcAllocator; make a request; call pick_winner(); verify
    //! the requestor is picked, the request bit is turned off, and a vc is allocated.
    void test_pick_winner_0()
    {
	//const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC = random() % VCS;

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT, 0, IN_PORT, IN_VC);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		bool requested = vca->requested[i][j].is_valid;
		if(i == OUT_PORT && j == IN_PORT*VCS + IN_VC)
		    CPPUNIT_ASSERT_EQUAL(true, requested);
		else
		    CPPUNIT_ASSERT_EQUAL(false, requested);
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i=0; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT][i]);
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();


	//verify only one winner
	CPPUNIT_ASSERT_EQUAL(1, (int)winners.size());

	VCA_unit winner_unit = winners[0];

	//verify winner info
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winner_unit.in_port);
	CPPUNIT_ASSERT_EQUAL(IN_VC, winner_unit.in_vc);
	CPPUNIT_ASSERT_EQUAL(OUT_PORT, winner_unit.out_port);

	//verify the requested bit is turned off
	CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC].is_valid);

	//verify the output vc is marked as taken
	for(unsigned i=0; i<VCS; i++) {
	    bool taken = vca->ovc_taken[OUT_PORT][i];
	    if(i == 0)
		CPPUNIT_ASSERT_EQUAL(true, taken); //first vc is allocated
	    else
		CPPUNIT_ASSERT_EQUAL(false, taken);
	}

	delete vca;
	delete router;
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test pick_winner(): unsuccessful
    //!
    //! Create a RRVcAllocator; make a request; manually set all VCs of the output port
    //! to taken; call pick_winner(); verify the requestor is NOT picked, the request bit
    //! stays on.
    void test_pick_winner_1()
    {
	//const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC = random() % VCS;

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT, 0, IN_PORT, IN_VC);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		bool requested = vca->requested[i][j].is_valid;
		if(i == OUT_PORT && j == IN_PORT*VCS + IN_VC)
		    CPPUNIT_ASSERT_EQUAL(true, requested);
		else
		    CPPUNIT_ASSERT_EQUAL(false, requested);
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i=0; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT][i]);
	}

        //manually set the output VCs' credits to a value less than max
	for(unsigned i=0; i<VCS; i++) {
	    router->downstream_credits[OUT_PORT][i] = random() % (router->CREDITS);
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();

	//verify no winner
	CPPUNIT_ASSERT_EQUAL(0, (int)winners.size());

	//verify the requested bit is on
	CPPUNIT_ASSERT_EQUAL(true, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC].is_valid);


	delete vca;
	delete router;
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test pick_winner(): unsuccessful
    //!
    //! Create a RRVcAllocator; make a request; manually set credits of all VCs of the
    //! output port to a value less than max. call pick_winner(); verify the requestor
    //! is NOT picked, the request bit stays on.
    void test_pick_winner_2()
    {
	//const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC = random() % VCS;

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT, 0, IN_PORT, IN_VC);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		bool requested = vca->requested[i][j].is_valid;
		if(i == OUT_PORT && j == IN_PORT*VCS + IN_VC)
		    CPPUNIT_ASSERT_EQUAL(true, requested);
		else
		    CPPUNIT_ASSERT_EQUAL(false, requested);
	    }
	}

        //manually set all VCs of output port to taken
	for(unsigned i=0; i<VCS; i++) {
	    vca->ovc_taken[OUT_PORT][i] = true;
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();

	//verify no winner
	CPPUNIT_ASSERT_EQUAL(0, (int)winners.size());

	//verify the requested bit is on
	CPPUNIT_ASSERT_EQUAL(true, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC].is_valid);


	delete vca;
	delete router;
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a RRVcAllocator; make 2 requests from the same input port but differnt
    //! input vcs, to the same output port; call pick_winner(); verify
    //! both requestors are picked, the request bit is turned off for both, and 2 vcs allocated.
    void test_pick_winner_3()
    {
	//const unsigned VCS = random() % 5 + 2; //2 to 6 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC1 = random() % VCS;
	unsigned IN_VC2;
	while((IN_VC2 = random() % VCS) == IN_VC1); //in_vc2 must be different from in_vc1

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT, 0, IN_PORT, IN_VC1);
	vca->request(OUT_PORT, 1, IN_PORT, IN_VC2);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		if(i == OUT_PORT && (j == IN_PORT*VCS + IN_VC1 || j == IN_PORT*VCS + IN_VC2))
		    CPPUNIT_ASSERT_EQUAL(true, (bool)(vca->requested[i][j].is_valid));
		else
		    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->requested[i][j].is_valid));
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->ovc_taken[OUT_PORT][i]));
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();


	//verify both are picked
	CPPUNIT_ASSERT_EQUAL(2, (int)winners.size());


	//verify winner info
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winners[0].in_port);
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winners[1].in_port);
	CPPUNIT_ASSERT_EQUAL(OUT_PORT, winners[0].out_port);
	CPPUNIT_ASSERT_EQUAL(OUT_PORT, winners[1].out_port);


	if(winners[0].in_vc == IN_VC1) { //winners[0] is IN_VC1
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[1].in_vc);
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[0].in_vc);
	    CPPUNIT_ASSERT_EQUAL(IN_VC1, winners[1].in_vc);
	}

	//verify the requested bit is turned off
	CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC1].is_valid);
	CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC2].is_valid);

	//verify 2 output vcs marked as taken
	for(unsigned i=0; i<VCS; i++) {
	    bool taken = vca->ovc_taken[OUT_PORT][i];
	    if(i == 0 || i == 1)
		CPPUNIT_ASSERT_EQUAL(true, taken); //first 2 vcs allocated
	    else
		CPPUNIT_ASSERT_EQUAL(false, taken);
	}

	delete vca;
	delete router;
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a RRVcAllocator; make 2 requests from the same input port but differnt
    //! input vcs, to the same output port; call pick_winner(); if both request same output vc,
    //! only one requestor is picked, the request bit is turned off.
    void test_pick_winner_3_1()
    {
	//const unsigned VCS = random() % 5 + 2; //2 to 6 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;
	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT); //in_port must be different from out_port.
	const unsigned IN_VC1 = random() % VCS;
	unsigned IN_VC2;
	while((IN_VC2 = random() % VCS) == IN_VC1); //in_vc2 must be different from in_vc1

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	//both request same output vc
	int OUT_VC = random() % VCS;
	vca->request(OUT_PORT, OUT_VC, IN_PORT, IN_VC1);
	vca->request(OUT_PORT, OUT_VC, IN_PORT, IN_VC2);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		if(i == OUT_PORT && (j == IN_PORT*VCS + IN_VC1 || j == IN_PORT*VCS + IN_VC2))
		    CPPUNIT_ASSERT_EQUAL(true, (bool)(vca->requested[i][j].is_valid));
		else
		    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->requested[i][j].is_valid));
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->ovc_taken[OUT_PORT][i]));
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();


	//verify only one is picked
	CPPUNIT_ASSERT_EQUAL(1, (int)winners.size());


	//verify winner info
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winners[0].in_port);
	CPPUNIT_ASSERT_EQUAL(OUT_PORT, winners[0].out_port);


	//verify the requested bit is turned off
	if(winners[0].in_vc == IN_VC1) { //winners[0] is IN_VC1
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC1].is_valid);
	    CPPUNIT_ASSERT_EQUAL(true, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC2].is_valid);
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(true, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC1].is_valid);
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT][IN_PORT*VCS + IN_VC2].is_valid);
	}

	//verify  output vc marked as taken
	for(unsigned i=0; i<VCS; i++) {
	    bool taken = vca->ovc_taken[OUT_PORT][i];
	    if(i == OUT_VC)
		CPPUNIT_ASSERT_EQUAL(true, taken);
	    else
		CPPUNIT_ASSERT_EQUAL(false, taken);
	}

	delete vca;
	delete router;
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a RRVcAllocator; make 2 requests from the same input port but differnt
    //! input vcs, to the 2 different output ports; call pick_winner(); verify
    //! both requestors are picked, the request bit is turned off for both, and 2 vcs allocated.
    void test_pick_winner_4()
    {
	//const unsigned VCS = random() % 5 + 2; //2 to 6 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT1 = random() % PORTS;
	unsigned OUT_PORT2;
	while((OUT_PORT2 = random() % PORTS) == OUT_PORT1); //out_port2 must be different from out_port1.

	unsigned IN_PORT;
	while((IN_PORT = random() % PORTS) == OUT_PORT1 || IN_PORT == OUT_PORT2); //in_port must be different from both out_port1 and out_port2.
	const unsigned IN_VC1 = random() % VCS;
	unsigned IN_VC2;
	while((IN_VC2 = random() % VCS) == IN_VC1); //in_vc2 must be different from in_vc1

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT1, 0, IN_PORT, IN_VC1);
	vca->request(OUT_PORT2, 0, IN_PORT, IN_VC2);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		bool requested = vca->requested[i][j].is_valid;
		if((i == OUT_PORT1 && (j == IN_PORT*VCS + IN_VC1) ) ||
		   (i == OUT_PORT2 && (j == IN_PORT*VCS + IN_VC2)))
		    CPPUNIT_ASSERT_EQUAL(true, requested);
		else
		    CPPUNIT_ASSERT_EQUAL(false, requested);
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i=0; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT1][i]);
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT2][i]);
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();


	//verify both are picked
	CPPUNIT_ASSERT_EQUAL(2, (int)winners.size());


	//verify winner info
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winners[0].in_port);
	CPPUNIT_ASSERT_EQUAL(IN_PORT, winners[1].in_port);

	if(winners[0].out_port == OUT_PORT1) { //winners[0] is IN_PORT/IN_VC1
	    CPPUNIT_ASSERT_EQUAL(IN_VC1, winners[0].in_vc);
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[1].in_vc);
	    CPPUNIT_ASSERT_EQUAL(OUT_PORT1, winners[0].out_port);
	    CPPUNIT_ASSERT_EQUAL(OUT_PORT2, winners[1].out_port);
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(IN_VC1, winners[1].in_vc);
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[0].in_vc);
	    CPPUNIT_ASSERT_EQUAL(OUT_PORT1, winners[1].out_port);
	    CPPUNIT_ASSERT_EQUAL(OUT_PORT2, winners[0].out_port);
	}


	//verify the requested bit is turned off
	CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT1][IN_PORT*VCS + IN_VC1].is_valid);
	CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT2][IN_PORT*VCS + IN_VC2].is_valid);

	//verify 2 output vcs marked as taken
	for(unsigned i=0; i<VCS; i++) {
	    if(i == 0) {
		CPPUNIT_ASSERT_EQUAL(true, (bool)vca->ovc_taken[OUT_PORT1][i]); //first vc is allocated
		CPPUNIT_ASSERT_EQUAL(true, (bool)vca->ovc_taken[OUT_PORT2][i]); //first vc is allocated
	    }
	    else {
		CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT1][i]);
		CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT2][i]);
	    }
	}

	delete vca;
	delete router;
    } 

    



    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a RRVcAllocator; number of VCs is > 1; make 2 requests from the 2 different
    //! input ports, to the same output port; call pick_winner(); verify
    //! both requestors are picked, the request bit is turned off for both, and 2 vcs allocated.
    void test_pick_winner_5()
    {
	//const unsigned VCS = random() % 5 + 2; //2 to 6 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;

	unsigned IN_PORT1;
	while((IN_PORT1 = random() % PORTS) == OUT_PORT); //in_port1 must be different from out_port.

	unsigned IN_PORT2;
	while((IN_PORT2 = random() % PORTS) == OUT_PORT || IN_PORT2 == IN_PORT1); //in_port2 must be different from both out_port and in_port1.
	const unsigned IN_VC1 = random() % VCS;
	const unsigned IN_VC2 = random() % VCS;

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	const unsigned OUT_VC1 = random() % VCS;
	unsigned OUT_VC2;
	while((OUT_VC2 = random() % VCS) == OUT_VC1);

	vca->request(OUT_PORT, OUT_VC2, IN_PORT1, IN_VC1);
	vca->request(OUT_PORT, OUT_VC1, IN_PORT2, IN_VC2);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		bool requested = vca->requested[i][j].is_valid;
		if((i == OUT_PORT && (j == IN_PORT1*VCS + IN_VC1) ) ||
		   (i == OUT_PORT && (j == IN_PORT2*VCS + IN_VC2)))
		    CPPUNIT_ASSERT_EQUAL(true, requested);
		else
		    CPPUNIT_ASSERT_EQUAL(false, requested);
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i=0; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT][i]);
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();


	//verify both are picked
	CPPUNIT_ASSERT_EQUAL(2, (int)winners.size());


	//verify winner info
	CPPUNIT_ASSERT_EQUAL(OUT_PORT, winners[0].out_port);
	CPPUNIT_ASSERT_EQUAL(OUT_PORT, winners[1].out_port);

	if(winners[0].in_port == IN_PORT1) { //winners[0] is IN_PORT1
	    CPPUNIT_ASSERT_EQUAL(IN_VC1, winners[0].in_vc);
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[1].in_vc);
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(IN_VC1, winners[1].in_vc);
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[0].in_vc);
	}


	//verify the requested bit is turned off
	CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT][IN_PORT1*VCS + IN_VC1].is_valid);
	CPPUNIT_ASSERT_EQUAL(false, (bool)vca->requested[OUT_PORT][IN_PORT2*VCS + IN_VC2].is_valid);

	//verify 2 output vcs marked as taken
	for(unsigned i=0; i<VCS; i++) {
	    bool taken = vca->ovc_taken[OUT_PORT][i];
	    if(i == OUT_VC1 || i == OUT_VC2) {
		CPPUNIT_ASSERT_EQUAL(true, taken); //first 2 vcs allocated
	    }
	    else {
		CPPUNIT_ASSERT_EQUAL(false, taken);
	    }
	}

	delete vca;
	delete router;
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a RRVcAllocator; only 1 output vc available; make 2 requests from the 2 different
    //! input ports, to the same output port; call pick_winner(); verify
    //! only 1 requestor is picked, the request bit is turned off, and 1 vc allocated.
    void test_pick_winner_6()
    {
	//const unsigned VCS = random() % 5 + 2; //2 to 6 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;

	unsigned IN_PORT1;
	while((IN_PORT1 = random() % PORTS) == OUT_PORT); //in_port1 must be different from out_port.

	unsigned IN_PORT2;
	while((IN_PORT2 = random() % PORTS) == OUT_PORT || IN_PORT2 == IN_PORT1); //in_port2 must be different from both out_port and in_port1.
	const unsigned IN_VC1 = random() % VCS;
	const unsigned IN_VC2 = random() % VCS;

	const unsigned OUT_VC1 = random() % VCS;
	unsigned OUT_VC2;
	while((OUT_VC2 = random() % VCS) == OUT_VC1);

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT, OUT_VC1, IN_PORT1, IN_VC1);
	vca->request(OUT_PORT, OUT_VC2, IN_PORT2, IN_VC2);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		bool requested = vca->requested[i][j].is_valid;
		if((i == OUT_PORT && (j == IN_PORT1*VCS + IN_VC1) ) ||
		   (i == OUT_PORT && (j == IN_PORT2*VCS + IN_VC2)))
		    CPPUNIT_ASSERT_EQUAL(true, requested);
		else
		    CPPUNIT_ASSERT_EQUAL(false, requested);
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i=0; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT][i]);
	}

	//randomly pick an output VC from OUT_VC1 and OUT_VC2; mark the rest as taken
	const unsigned AVAILABLE_OVC = ((random() % 2) == 0) ? OUT_VC1 : OUT_VC2;
	for(unsigned i=0; i<VCS; i++) {
	    if(i != AVAILABLE_OVC)
	        vca->ovc_taken[OUT_PORT][i] = true;
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();


	//verify only 1 picked
	CPPUNIT_ASSERT_EQUAL(1, (int)winners.size());


	//verify winner info
	CPPUNIT_ASSERT_EQUAL(OUT_PORT, winners[0].out_port);

	if(winners[0].in_port == IN_PORT1) { //winners[0] is IN_PORT1
	    CPPUNIT_ASSERT_EQUAL(IN_VC1, winners[0].in_vc);
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[0].in_vc);
	}

	//verify the selected output vc
	CPPUNIT_ASSERT_EQUAL(AVAILABLE_OVC, winners[0].out_vc);

	//verify the requested bit is turned off
	if(winners[0].in_port == IN_PORT1) { //winners[0] is IN_PORT1
	    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->requested[OUT_PORT][IN_PORT1*VCS + IN_VC1].is_valid));
	    CPPUNIT_ASSERT_EQUAL(true,  (bool)(vca->requested[OUT_PORT][IN_PORT2*VCS + IN_VC2].is_valid));
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(true, (bool)(vca->requested[OUT_PORT][IN_PORT1*VCS + IN_VC1].is_valid));
	    CPPUNIT_ASSERT_EQUAL(false,  (bool)(vca->requested[OUT_PORT][IN_PORT2*VCS + IN_VC2].is_valid));
	}


	//verify the particular output vc is marked as taken
	bool taken = vca->ovc_taken[OUT_PORT][AVAILABLE_OVC];
	CPPUNIT_ASSERT_EQUAL(true, taken);

	delete vca;
	delete router;
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test pick_winner()
    //!
    //! Create a RRVcAllocator; make 2 requests from the 2 different
    //! input ports, to 2 different output ports; call pick_winner(); verify
    //! both requestor are picked, the request bit is turned off, and 2 vc allocated.
    void test_pick_winner_7()
    {
	//const unsigned VCS = random() % 5 + 2; //2 to 6 virtual channels
	const unsigned VCS = 4;
	const unsigned PORTS = 5;

	const unsigned OUT_PORT1 = 0;
	const unsigned OUT_PORT2 = 1;
	const unsigned IN_PORT1 = 2;
	const unsigned IN_PORT2 = 3;

	const unsigned IN_VC1 = random() % VCS;
	const unsigned IN_VC2 = random() % VCS;

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT1, 0, IN_PORT1, IN_VC1);
	vca->request(OUT_PORT2, 0, IN_PORT2, IN_VC2);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		bool requested = vca->requested[i][j].is_valid;
		if((i == OUT_PORT1 && (j == IN_PORT1*VCS + IN_VC1) ) ||
		   (i == OUT_PORT2 && (j == IN_PORT2*VCS + IN_VC2)))
		    CPPUNIT_ASSERT_EQUAL(true, requested);
		else
		    CPPUNIT_ASSERT_EQUAL(false, requested);
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i=0; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT1][i]);
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT2][i]);
	}


	//call pick_winner()
	vector<VCA_unit>& winners = vca->pick_winner();


	//verify 2 picked
	CPPUNIT_ASSERT_EQUAL(2, (int)winners.size());


	//verify winner info

	if(winners[0].in_port == IN_PORT1) { //winners[0] is IN_PORT1
	    CPPUNIT_ASSERT_EQUAL(IN_PORT2, winners[1].in_port);
	    CPPUNIT_ASSERT_EQUAL(IN_VC1, winners[0].in_vc);
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[1].in_vc);
	    CPPUNIT_ASSERT_EQUAL(OUT_PORT1, winners[0].out_port);
	    CPPUNIT_ASSERT_EQUAL(OUT_PORT2, winners[1].out_port);
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(IN_PORT2, winners[0].in_port);
	    CPPUNIT_ASSERT_EQUAL(IN_PORT1, winners[1].in_port);
	    CPPUNIT_ASSERT_EQUAL(IN_VC2, winners[0].in_vc);
	    CPPUNIT_ASSERT_EQUAL(IN_VC1, winners[1].in_vc);
	    CPPUNIT_ASSERT_EQUAL(OUT_PORT2, winners[0].out_port);
	    CPPUNIT_ASSERT_EQUAL(OUT_PORT1, winners[1].out_port);
	}


	//verify the requested bit is turned off
	if(winners[0].in_port == IN_PORT1) { //winners[0] is IN_PORT1
	    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->requested[OUT_PORT1][IN_PORT1 + PORTS*IN_VC1].is_valid));
	    CPPUNIT_ASSERT_EQUAL(false,  (bool)(vca->requested[OUT_PORT2][IN_PORT2 + PORTS*IN_VC2].is_valid));
	}
	else {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)(vca->requested[OUT_PORT2][IN_PORT1 + PORTS*IN_VC1].is_valid));
	    CPPUNIT_ASSERT_EQUAL(false,  (bool)(vca->requested[OUT_PORT1][IN_PORT2 + PORTS*IN_VC2].is_valid));
	}



	delete vca;
	delete router;
    } 

    



    //======================================================================
    //======================================================================
    //! @brief Test pick_winner(); verify the round-robin priority.
    //!
    //! Create a RRVcAllocator; only 1 output vc available; make 2 requests from the 2 different
    //! input ports, to the same output port; call pick_winner() N times; after each call
    //! the winner makes another request; verify the 2 input ports are picked as winner in
    //! turn.
    void test_pick_winner_8()
    {
	//const unsigned VCS = random() % 5 + 2; //2 to 6 virtual channels
	const unsigned VCS = 4; //must be 4
	const unsigned PORTS = 5;

	const unsigned OUT_PORT = random() % PORTS;

	unsigned IN_PORT1;
	while((IN_PORT1 = random() % PORTS) == OUT_PORT); //in_port1 must be different from out_port.

	unsigned IN_PORT2;
	while((IN_PORT2 = random() % PORTS) == OUT_PORT || IN_PORT2 == IN_PORT1); //in_port2 must be different from both out_port and in_port1.
	const unsigned IN_VC1 = random() % VCS;
	const unsigned IN_VC2 = random() % VCS;

	SimpleRouter* router = create_router(PORTS, VCS);
	RRVcAllocator* vca = new RRVcAllocator(router, PORTS, VCS);

	vca->request(OUT_PORT, 0, IN_PORT1, IN_VC1);
	vca->request(OUT_PORT, 0, IN_PORT2, IN_VC2);

        //verify the request bit is turned on
	for(unsigned i=0; i<PORTS; i++) {
	    for(unsigned j=0; j<PORTS*VCS; j++) {
		bool requested = vca->requested[i][j].is_valid;
		if((i == OUT_PORT && (j == IN_PORT1*VCS + IN_VC1) ) ||
		   (i == OUT_PORT && (j == IN_PORT2*VCS + IN_VC2)))
		    CPPUNIT_ASSERT_EQUAL(true, requested);
		else
		    CPPUNIT_ASSERT_EQUAL(false, requested);
	    }
	}

        //verify no output vc is marked taken
	for(unsigned i=0; i<VCS; i++) {
	    CPPUNIT_ASSERT_EQUAL(false, (bool)vca->ovc_taken[OUT_PORT][i]);
	}

	//randomly pick an output VC; mark the rest as taken
	const unsigned AVAILABLE_OVC = random() % VCS;
	for(unsigned i=0; i<VCS; i++) {
	    if(i != AVAILABLE_OVC)
	        vca->ovc_taken[OUT_PORT][i] = true;
	}


	const int N = 100;

	VCA_unit WINNER[N];

	for(int i=0; i<N; i++) {
	    //call pick_winner()
	    vector<VCA_unit>& winners = vca->pick_winner();

	    WINNER[i] = winners[0];

	    //let the winner make request again
	    if(winners[0].in_port == IN_PORT1) { //winners[0] is IN_PORT1
		vca->request(OUT_PORT, 0, IN_PORT1, IN_VC1);
	    }
	    else {
		vca->request(OUT_PORT, 0, IN_PORT2, IN_VC2);
	    }

	    vca->ovc_taken[OUT_PORT][AVAILABLE_OVC] = false;
	}


	//verify winners were picked round-robin
	for(int i=2; i<N; i++) {
	    if(i % 2 == 0)
		CPPUNIT_ASSERT_EQUAL(WINNER[0].in_port, WINNER[i].in_port);
	    else
		CPPUNIT_ASSERT_EQUAL(WINNER[1].in_port, WINNER[i].in_port);
	}
	CPPUNIT_ASSERT(WINNER[0].in_port != WINNER[1].in_port);



	delete vca;
	delete router;
    } 

    

    

    

    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("RRVcAllocatorTest");

	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_Constructor_0", &RRVcAllocatorTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_request_0", &RRVcAllocatorTest::test_request_0));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_0", &RRVcAllocatorTest::test_pick_winner_0));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_1", &RRVcAllocatorTest::test_pick_winner_1));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_2", &RRVcAllocatorTest::test_pick_winner_2));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_3", &RRVcAllocatorTest::test_pick_winner_3));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_3_1", &RRVcAllocatorTest::test_pick_winner_3_1));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_4", &RRVcAllocatorTest::test_pick_winner_4));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_5", &RRVcAllocatorTest::test_pick_winner_5));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_6", &RRVcAllocatorTest::test_pick_winner_6));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_7", &RRVcAllocatorTest::test_pick_winner_7));
	mySuite->addTest(new CppUnit::TestCaller<RRVcAllocatorTest>("test_pick_winner_8", &RRVcAllocatorTest::test_pick_winner_8));
	/*
	*/
	return mySuite;
    }
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( RRVcAllocatorTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


