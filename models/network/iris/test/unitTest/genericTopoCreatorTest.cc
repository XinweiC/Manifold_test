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


#define CREDIT_TYPE 123

//####################################################################
//! Class NetworkInterfaceTest is the test class for class NetworkInterface. 
//####################################################################
class genericTopoCreatorTest : public CppUnit::TestFixture {
private:

public:   
    //! Initialization function. Inherited from the CPPUnit framework.
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };
    
    //======================================================================
    //======================================================================
    //! @brief Test create_ring of topoCreator
    //!
    //! see if the ring network have been created successfully  with correct 
    //! # of interfaces and routers  
    void test_create_ring_0()
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
        Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, (SimulatedLen<TerminalData>*)0, (VnetAssign<TerminalData>*)0, CREDIT_TYPE, 0, 0);
        CPPUNIT_ASSERT_EQUAL(rp.no_nodes, uint(topo->get_interface_id().size()) );
        CPPUNIT_ASSERT_EQUAL(rp.no_nodes, uint(topo->interfaces.size()) );
        CPPUNIT_ASSERT_EQUAL(rp.no_nodes, uint(topo->router_ids.size()) );
        CPPUNIT_ASSERT_EQUAL(rp.no_nodes, uint(topo->routers.size()) );
	delete mapping;
    } 
    
    //======================================================================
    //======================================================================
    //! @brief Test create_torus of topoCreator
    //!
    //! see if the torus network have been created successfully  with correct 
    //! # of interfaces and routers  
    void test_create_torus_0()
    {
        //the paramters for ring network
        torus_init_params rp;
        rp.x_dim = 4;
        rp.y_dim = 4;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.link_width = 128;
	rp.ni_up_credits = 10;
	rp.ni_upstream_buffer_size = 5;
        
        Clock& clk = MasterClock;
        
        Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	vector<int> node_lps(rp.x_dim * rp.y_dim);
	for(int i=0; i<rp.x_dim*rp.y_dim; i++)
	    node_lps[i]=0;
        Torus<TerminalData>* topo = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, (SimulatedLen<TerminalData>*)0, 0, CREDIT_TYPE, &node_lps);
	const unsigned no_nodes = rp.x_dim * rp.y_dim;
        CPPUNIT_ASSERT_EQUAL(no_nodes, uint(topo->get_interface_id().size()) );
        CPPUNIT_ASSERT_EQUAL(no_nodes, uint(topo->interfaces.size()) );
        CPPUNIT_ASSERT_EQUAL(no_nodes, uint(topo->router_ids.size()) );
        CPPUNIT_ASSERT_EQUAL(no_nodes, uint(topo->routers.size()) );
	delete mapping;
    }    
     
    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("genericTopoCreatorTest");
	
	mySuite->addTest(new CppUnit::TestCaller<genericTopoCreatorTest>("test_create_ring_0", 
	                 &genericTopoCreatorTest::test_create_ring_0));
	mySuite->addTest(new CppUnit::TestCaller<genericTopoCreatorTest>("test_create_torus_0", 
	                 &genericTopoCreatorTest::test_create_torus_0));
	return mySuite;
    }
};

Clock genericTopoCreatorTest :: MasterClock(genericTopoCreatorTest :: MASTER_CLOCK_HZ);

int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( genericTopoCreatorTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


