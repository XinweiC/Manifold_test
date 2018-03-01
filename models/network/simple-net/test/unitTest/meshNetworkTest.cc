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
#include "interfaceCreator.h"
#include "network.h"
#include "networkInterface.h"
#include "router.h"

#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;

using namespace manifold::simple_net;

//####################################################################
// helper classes
//####################################################################
class TerminalData {
public:
    int src;
    int dst;
    int get_src() { return src; }
    int get_dst() { return dst; }
};


//####################################################################
//! Class MeshNetworkTest is the test class for class MeshNetwork. 
//####################################################################
class MeshNetworkTest : public CppUnit::TestFixture {
private:
    static Clock MasterClock;

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }


    //======================================================================
    //======================================================================
    //! @brief Test constructor: number of interfaces.
    //!
    //! Create a network with N interfaces; N is randomly generated.
    //! Verify N interfaces and N routers were created.
    void testConstructor_0()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int X = random() % 32 + 1;
	    int Y = random() % 32 + 1;
	    if(X==1 && Y==1)
	        X = 2;
	    const int N = X * Y;

	    GenNI_flow_control_setting setting;
	    setting.credits = random()%10 + 5;

	    //create a network with N interfaces
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();
	    std::vector<vector<MeshRouter*> >& routers = myNetwork->get_routers();

            //verify N interfaces have been created.
	    CPPUNIT_ASSERT_EQUAL(N, (int)interfaces.size());
	    CPPUNIT_ASSERT_EQUAL(N, (int)ni_cids.size());
	    CPPUNIT_ASSERT_EQUAL(Y, (int)routers.size());
	    CPPUNIT_ASSERT_EQUAL(X, (int)routers[0].size());
	    delete myNetwork;
	}
    }



    //======================================================================
    //======================================================================
    //! @brief Test constructor: buffer size.
    //!
    //! Create a network with router buffer size B, a random number.
    //! Verify the buffers size for both routers and nis are correctly initialized.
    void testConstructor_1()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int X = random() % 32 + 1;
	    int Y = random() % 32 + 1;
	    if(X==1 && Y==1)
	        X = 2;
	    const int N = X * Y;
	    const int CREDITS = random() % 100 + 1;

	    GenNI_flow_control_setting setting;
	    setting.credits = CREDITS;

	    //create a network with N interfaces
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();
	    std::vector<vector<MeshRouter*> >& routers = myNetwork->get_routers();

            //verify buffer size was set correctly
	    for(int r=0; r<Y; r++) {
		for(int c=0; c<X; c++) {
		    CPPUNIT_ASSERT_EQUAL(CREDITS, routers[r][c]->m_CREDITS);
		}
	    }
	    for(int k=0; k<N; k++) {
		CPPUNIT_ASSERT_EQUAL(CREDITS, dynamic_cast<GenNetworkInterface<TerminalData>*>(interfaces[k])->m_CREDITS);
	    }
	    delete myNetwork;
	}
    }



    //======================================================================
    //======================================================================
    //! @brief Test constructor: LP.
    //!
    //! Create a network with N interfaces on a random LP; verify all
    //! the N interfaces and N routers have the same LP as sepcified in the constructor.
    void testConstructor_2()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        //for(int i=0; i<100; i++) { //try 100 networks
        for(int i=0; i<10; i++) { //try 100 networks
	    int X = random() % 32 + 1;
	    int Y = random() % 32 + 1;
	    if(X==1 && Y==1)
	        X = 2;
	    const int N = X * Y;

	    int LP = random() % N;

	    GenNI_flow_control_setting setting;
	    setting.credits = random() % 10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, LP, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();
	    std::vector<vector<CompId_t> >& router_cids = myNetwork->get_routerCids();

            //verify all interfaces have the same LP as specified in constructor
	    for(int k=0; k<N; k++) {
		CPPUNIT_ASSERT_EQUAL(LP, (int)Component::GetComponentLP(ni_cids[k]));
	    }

            //verify all routers have the same LP as specified in constructor
	    for(int r=0; r<Y; r++) {
		for(int c=0; c<X; c++) {
		    CPPUNIT_ASSERT_EQUAL(LP, (int)Component::GetComponentLP(router_cids[r][c]));
		}
	    }

	    delete myNetwork;
	}
    }




    //======================================================================
    //======================================================================
    //! @brief Test constructor: network interfaces and routers.
    //!
    //! Create a network with N interfaces on LP 0; verify all
    //! the N interfaces are indeed related to the corresponding component id.
    //! And so are the routers.
    void testConstructor_3()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int X = random() % 32 + 1;
	    int Y = random() % 32 + 1;
	    if(X==1 && Y==1)
	        X = 2;
	    const int N = X * Y;

	    GenNI_flow_control_setting setting;
	    setting.credits = random() % 10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();
	    std::vector<vector<CompId_t> >& router_cids = myNetwork->get_routerCids();
	    std::vector<vector<MeshRouter*> >& routers = myNetwork->get_routers();

            //verify interfaces are related to the cids
	    for(int k=0; k<N; k++) {
	        CPPUNIT_ASSERT(interfaces[k] != 0);
		CPPUNIT_ASSERT(interfaces[k] == Component::GetComponent(ni_cids[k]));
	    }
            //verify routers are related to the cids
	    for(int r=0; r<Y; r++) {
		for(int c=0; c<X; c++) {
		    CPPUNIT_ASSERT(routers[r][c] != 0);
		    CPPUNIT_ASSERT(routers[r][c] == Component::GetComponent(router_cids[r][c]));
		}
	    }
	    delete myNetwork;
	}
    }


    //======================================================================
    //======================================================================
    //! @brief Test constructor: network interfaces and routers.
    //!
    //! Create a network with N interfaces on LP 0; verify all
    //! the N interfaces have ids from 0 to N-1. So do routers.
    void testConstructor_4()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<1; i++) { //try 100 networks
	    int X = random() % 32 + 1;
	    int Y = random() % 32 + 1;
	    if(X==1 && Y==1)
	        X = 2;
	    const int N = X * Y;

	    GenNI_flow_control_setting setting;
	    setting.credits = random() % 10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();
	    std::vector<vector<MeshRouter*> >& routers = myNetwork->get_routers();

            //verify interface ids are 0 to N-1
	    for(int k=0; k<N; k++) {
	        CPPUNIT_ASSERT(interfaces[k] != 0);
		CPPUNIT_ASSERT_EQUAL(k, interfaces[k]->get_id());
	    }

            //verify interface ids are 0 to N-1
	    for(int r=0; r<Y; r++) {
		for(int c=0; c<X; c++) {
		    CPPUNIT_ASSERT(routers[r][c] != 0);
		    CPPUNIT_ASSERT(r*X + c == routers[r][c]->get_id());
		}
	    }
	    delete myNetwork;
	}
    }


    //======================================================================
    //======================================================================
    //! @brief Test constructor: network interfaces.
    //!
    //! Create a network with N interfaces on LP 0; verify all
    //! the N interfaces have a pointer to the Network object.
    void testConstructor_5()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int X = random() % 32 + 1;
	    int Y = random() % 32 + 1;
	    if(X==1 && Y==1)
	        X = 2;
	    const int N = X * Y;

	    GenNI_flow_control_setting setting;
	    setting.credits = random() % 10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();

            //verify interface ids are 0 to N-1
	    for(int i=0; i<N; i++) {
	        CPPUNIT_ASSERT(interfaces[i] != 0);
		CPPUNIT_ASSERT(myNetwork == interfaces[i]->get_network());
	    }
	    delete myNetwork;
	}
    }


    //======================================================================
    //======================================================================
    //! @brief Test constructor: network interfaces and routers.
    //!
    //! Create a network with N interfaces on LP 0; verify all
    //! the N interfaces and their corresponding routers have same ID.
    void testConstructor_6()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int X = random() % 32 + 1;
	    int Y = random() % 32 + 1;
	    if(X==1 && Y==1)
	        X = 2;
	    //const int N = X * Y;

	    GenNI_flow_control_setting setting;
	    setting.credits = random() % 10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();
	    std::vector<std::vector<MeshRouter*> >& routers = myNetwork->get_routers();

            //verify interface ids are same as the router to which they are connected.
	    for(int i=0; i<Y; i++) {
		for(int j=0; j<X; j++) {
		    CPPUNIT_ASSERT(interfaces[i*X + j] != 0);
		    CPPUNIT_ASSERT_EQUAL(interfaces[i*X + j]->get_id(), routers[i][j]->get_id());
		}
	    }
	    delete myNetwork;
	}
    }



    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MeshNetworkTest");

	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("testConstructor_0", &MeshNetworkTest::testConstructor_0));
	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("testConstructor_1", &MeshNetworkTest::testConstructor_1));
	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("testConstructor_2", &MeshNetworkTest::testConstructor_2));
	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("testConstructor_3", &MeshNetworkTest::testConstructor_3));
	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("testConstructor_4", &MeshNetworkTest::testConstructor_4));
	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("testConstructor_5", &MeshNetworkTest::testConstructor_5));
	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("testConstructor_6", &MeshNetworkTest::testConstructor_6));

	return mySuite;
    }
};


Clock MeshNetworkTest::MasterClock(10);


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MeshNetworkTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


