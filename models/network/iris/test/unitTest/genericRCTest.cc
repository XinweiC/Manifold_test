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
#include "../../components/genericRC.h"

#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;
using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################



//####################################################################
//! Class GenericRCTest is the test class for class GenericRC. 
//####################################################################
class GenericRCTest : public CppUnit::TestFixture {
private:

public:
    
    //======================================================================
    //======================================================================
    //! @brief Test route_ring()
    //!
    //! Create a GenericRC for RING_ROUTING; generate N HeadFlits with random destionations;
    //! call route_ring; verify the selected output port is correct.
    void test_route_ring_0()
    {
	const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned GRID_SIZE = random() % 90 + 10; //number of routers is 10 to 100

	GenericRCSettings setting;
	setting.grid_size = GRID_SIZE;
	setting.node_id = random() % GRID_SIZE;
	setting.rc_method = RING_ROUTING;
	setting.no_nodes = GRID_SIZE; //for a ring, number of nodes equals to grid size

	GenericRC* rc = new GenericRC(VCS, setting);

	const int N = 1000;

	for(int i=0; i<N; i++) {
	    HeadFlit* hflit = new HeadFlit;
	    double rnd = random() / (RAND_MAX+1.0);
	    if(rnd < 0.3)
		hflit->dst_id = setting.node_id; //to self
	    else
	        while((hflit->dst_id = random() % setting.no_nodes) == setting.node_id); //not self

            rc->route_ring(hflit);

	    if(hflit->dst_id == setting.node_id) { //to self
	        CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_NI, (int)rc->possible_out_ports.back());
	    }
	    else {
	        if(hflit->dst_id > setting.node_id) { //destination is to the east
		    if((hflit->dst_id - setting.node_id) > GRID_SIZE/2) //disance > half; go west
			CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_WEST, (int)rc->possible_out_ports.back());
		    else
			CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_EAST, (int)rc->possible_out_ports.back());
		}
		else { //destination is to the west
		    if((setting.node_id - hflit->dst_id) > GRID_SIZE/2) //disance > half; go east
			CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_EAST, (int)rc->possible_out_ports.back());
		    else
			CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_WEST, (int)rc->possible_out_ports.back());
		}
	    }

	    delete hflit;
	}

	delete rc;
        
    } 

    



    //======================================================================
    //======================================================================
    //! @brief Test route_torus()
    //!
    //! Create a GenericRC for TORUS_ROUTING; generate N HeadFlits with random destionations;
    //! call route_torus; verify the selected output port is correct.
    void test_route_torus_0()
    {
	const unsigned VCS = random() % 5 + 1; //1 to 5 virtual channels
	const unsigned GRID_SIZE = random() % 13 + 4; //X dimension: 4 to 16
	const unsigned Y_DIM = random() % 13 + 4; //X dimension: 4 to 16

	GenericRCSettings setting;
	setting.grid_size = GRID_SIZE;
	setting.no_nodes = GRID_SIZE * Y_DIM;
	setting.node_id = random() % (setting.no_nodes);
	setting.rc_method = TORUS_ROUTING;

	GenericRC* rc = new GenericRC(VCS, setting);

	const unsigned NODE_X = setting.node_id % GRID_SIZE; // the router's X-Y coordinates
	const unsigned NODE_Y = setting.node_id / GRID_SIZE;

	const int N = 1000;

	for(int i=0; i<N; i++) {
	    HeadFlit* hflit = new HeadFlit;
	    double rnd = random() / (RAND_MAX+1.0);
	    if(rnd < 0.3)
		hflit->dst_id = setting.node_id; //to self
	    else
	        while((hflit->dst_id = random() % setting.no_nodes) == setting.node_id); //not self

            rc->route_torus(hflit);

	    if(hflit->dst_id == setting.node_id) { //to self
	        CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_NI, (int)rc->possible_out_ports.back());
	    }
	    else { //not to self, then route X first
	        unsigned destx = hflit->dst_id % GRID_SIZE; //destination's x-y coordinates
	        unsigned desty = hflit->dst_id / GRID_SIZE;

	        if(NODE_X == destx) { //x coordinates are the same; route y
		    if(desty > NODE_Y) {
			if((desty - NODE_Y) > setting.no_nodes/GRID_SIZE/2) //disance > half; go north
			    CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_NORTH, (int)rc->possible_out_ports.back());
			else
			    CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_SOUTH, (int)rc->possible_out_ports.back());
		    }
		    else {
			if((NODE_Y- desty) > setting.no_nodes/GRID_SIZE/2) //disance > half; go south
			    CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_SOUTH, (int)rc->possible_out_ports.back());
			else
			    CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_NORTH, (int)rc->possible_out_ports.back());
		    }
		}
		else { // not same X; route X
		    if(destx > NODE_X) {
			if((destx - NODE_X) > GRID_SIZE/2) //disance > half; go west
			    CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_WEST, (int)rc->possible_out_ports.back());
			else
			    CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_EAST, (int)rc->possible_out_ports.back());
		    }
		    else {
			if((NODE_X- destx) > GRID_SIZE/2) //disance > half; go east
			    CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_EAST, (int)rc->possible_out_ports.back());
			else
			    CPPUNIT_ASSERT_EQUAL((int)SimpleRouter::PORT_WEST, (int)rc->possible_out_ports.back());
		    }
		}
	    }

	    delete hflit;
	}

	delete rc;
        
    } 

    



    

    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("GenericRCTest");

	mySuite->addTest(new CppUnit::TestCaller<GenericRCTest>("test_route_ring_0", &GenericRCTest::test_route_ring_0));
	mySuite->addTest(new CppUnit::TestCaller<GenericRCTest>("test_route_torus_0", &GenericRCTest::test_route_torus_0));
	return mySuite;
    }
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( GenericRCTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


