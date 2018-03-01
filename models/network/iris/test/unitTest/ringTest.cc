/*
 * =====================================================================================
 *    Description:  unit tests for generic iris interface
 *
 *        Version:  1.0
 *        Created:  09/29/2011 
 *
 *         Author:  Zhenjiang Dong
 *         School:  Georgia Institute of Technology
 *
 * =====================================================================================
 */
 
#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>
#include <iostream>
#include <list>
#include <stdlib.h>
#include "../../interfaces/genericHeader.h"
#include "../../interfaces/genericIrisInterface.h"
#include "../../components/simpleRouter.h"
#include "../../genericTopology/genericTopoCreator.h"
#include "../../genericTopology/ring.h"
#include "../../genericTopology/torus.h"
#include "kernel/manifold.h"
#include "kernel/component.h"
#include "kernel/clock.h"

using namespace manifold::kernel;
using namespace std;

using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################

class TerminalData {
public:
    int type;
    uint src; //must have this 
    uint dest_id;
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    uint get_src() { return src; }
    int get_src_port() { return 0; }
    uint get_dst() { return dest_id; }
    void set_dst_port(int p) {}
    int get_simulated_len() { return sizeof(TerminalData); }
};


//####################################################################
//! Class NetworkInterfaceTest is the test class for class NetworkInterface. 
//####################################################################
class ringTest : public CppUnit::TestFixture {
private:

public:   
    //! Initialization function. Inherited from the CPPUnit framework.
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };
    
    //======================================================================
    //======================================================================
    //! @brief Test constructor function of Ring
    //!
    //! see if paramter has been correctly received and generate corresponding
    //! interfaces and routers
    void test_constructor_0()
    {
        //the paramters for ring network
        ring_init_params rp;
        rp.no_nodes = 4;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.rc_method = RING_ROUTING;
        rp.link_width = 128;
	rp.ni_up_credits = 10;
	rp.ni_upstream_buffer_size = 5;
        
        Clock& clk = MasterClock;
        
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
        Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, (SimulatedLen<TerminalData>*)0, 0, 123, 0, 0);
       
        //the parameters in Ring
        CPPUNIT_ASSERT_EQUAL(rp.no_nodes, topo->no_nodes);
        
        //see if all the parameters have been passed to router and interface correctly
        for (int i = 0; i < rp.no_nodes; i++)
        {
            //the parameters for interface
            CPPUNIT_ASSERT_EQUAL(rp.link_width , topo->interfaces[i]->LINK_WIDTH);
            CPPUNIT_ASSERT_EQUAL(rp.credits , topo->interfaces[i]->credits);
            CPPUNIT_ASSERT_EQUAL(rp.no_vcs , topo->interfaces[i]->no_vcs);
        
            //the parameters for router
            SimpleRouter* rt = static_cast<SimpleRouter*>(topo->routers[i]);
            CPPUNIT_ASSERT_EQUAL(rp.credits , rt->CREDITS);
            CPPUNIT_ASSERT_EQUAL(rp.no_vcs , rt->vcs);
            CPPUNIT_ASSERT_EQUAL(rp.no_nodes , rt->no_nodes);
            CPPUNIT_ASSERT_EQUAL(rp.no_nodes , rt->grid_size);
            CPPUNIT_ASSERT_EQUAL(rp.rc_method , rt->rc_method);
        }
        
        for ( uint i = 0; i < topo->interfaces.size(); i++)
            CPPUNIT_ASSERT_EQUAL(topo->interfaces.at(i)->id, i);
            
        for ( uint i = 0; i < topo->routers.size(); i++)
            CPPUNIT_ASSERT_EQUAL(topo->routers.at(i)->node_id, i);
    } 

           
    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("genericTopoCreatorTest");
	
	mySuite->addTest(new CppUnit::TestCaller<ringTest>("test_constructor_0", 
	                 &ringTest::test_constructor_0));

	return mySuite;
    }
};

Clock ringTest :: MasterClock(ringTest :: MASTER_CLOCK_HZ);

int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ringTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


