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
    void set_dst_port(int t) {}
};


//####################################################################
//! Class NetworkInterfaceTest is the test class for class NetworkInterface. 
//####################################################################
class torusTest : public CppUnit::TestFixture {
private:

public:   
    //! Initialization function. Inherited from the CPPUnit framework.
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };
    
    //======================================================================
    //======================================================================
    //! @brief Test constructor function of torus
    //!
    //! see if paramter has been correctly received and generate corresponding
    //! interfaces and routers
    void test_constructor_0()
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
        
	int CREDIT_TYPE = 123;
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	vector<int> node_lps(rp.x_dim * rp.y_dim);
	for(int i=0; i<rp.x_dim * rp.y_dim; i++)
	    node_lps[i] = 0;
        Torus<TerminalData>* topo = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, (SimulatedLen<TerminalData>*)0, 0, CREDIT_TYPE, &node_lps);
       
        //the parameters in torus
        CPPUNIT_ASSERT_EQUAL(rp.x_dim, topo->x_dim);
        CPPUNIT_ASSERT_EQUAL(rp.y_dim, topo->y_dim);
        
        //see if all the parameters have been passed to router and interface correctly
        for (int i = 0; i < rp.x_dim*rp.y_dim; i++)
        {
            //the parameters for interface
            CPPUNIT_ASSERT_EQUAL(rp.link_width , topo->interfaces[i]->LINK_WIDTH);
            CPPUNIT_ASSERT_EQUAL(rp.credits , topo->interfaces[i]->credits);
            CPPUNIT_ASSERT_EQUAL(rp.no_vcs , topo->interfaces[i]->no_vcs);
        
            //the parameters for router
            SimpleRouter* rt = static_cast<SimpleRouter*>(topo->routers[i]);
            CPPUNIT_ASSERT_EQUAL(5, (int)rt->ports);
            CPPUNIT_ASSERT_EQUAL(rp.credits , rt->CREDITS);
            CPPUNIT_ASSERT_EQUAL(rp.no_vcs , rt->vcs);
            CPPUNIT_ASSERT_EQUAL(rp.x_dim*rp.y_dim , rt->no_nodes);
            CPPUNIT_ASSERT_EQUAL(rp.x_dim , rt->grid_size);
            CPPUNIT_ASSERT_EQUAL(TORUS_ROUTING , rt->rc_method);
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
	
	mySuite->addTest(new CppUnit::TestCaller<torusTest>("test_constructor_0", 
	                 &torusTest::test_constructor_0));
	return mySuite;
    }
};

Clock torusTest :: MasterClock(torusTest :: MASTER_CLOCK_HZ);

int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( torusTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


