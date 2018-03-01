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
//####################################################################
class RingNetworkTest : public CppUnit::TestFixture {
private:
    enum { MASTER_CLOCK_HZ = 10 };
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
	    int N = random() % 64 + 2; //at least 2

            GenNI_flow_control_setting setting;
	    setting.credits = random()%10 + 5;

	    //create a network with N interfaces
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    RingNetwork* myNetwork = new RingNetwork(N, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();

            //verify N interfaces have been created.
	    CPPUNIT_ASSERT_EQUAL(N, (int)interfaces.size());
	    CPPUNIT_ASSERT_EQUAL(N, (int)ni_cids.size());
	    CPPUNIT_ASSERT_EQUAL(N, (int)myNetwork->get_routers().size());
	    delete myNetwork;
	}
    }



    //======================================================================
    //======================================================================
    //! @brief Test constructor: credits
    //!
    //! Create a network with credits B, a random number.
    //! Verify credits is correctly initialized.
    void testConstructor_1()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int CREDITS = random() % 100 + 1;
	    const int N = random() % 100 + 2;

            GenNI_flow_control_setting setting;
	    setting.credits = CREDITS;

	    //create a network with N interfaces
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    RingNetwork* myNetwork = new RingNetwork(N, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;

            vector<RingRouter*>& routers = myNetwork->get_routers();

	    CPPUNIT_ASSERT_EQUAL(N, (int)routers.size());

	    for(int k=0; k<N; k++) {
		CPPUNIT_ASSERT_EQUAL(CREDITS, routers[k]->m_CREDITS);
	    }

	    delete myNetwork;
	}
    }



    //======================================================================
    //======================================================================
    //! @brief Test constructor: LP
    //!
    //! Create a network with N interfaces on a random LP; verify all
    //! the interfaces and routers have the same LP as sepcified in the constructor.
    void testConstructor_2()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int N = random() % 64 + 2;

	    int LP = random() % N;

            GenNI_flow_control_setting setting;
	    setting.credits = random()%10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    RingNetwork* myNetwork = new RingNetwork(N, setting.credits, LP, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();
	    std::vector<CompId_t>& router_cids = myNetwork->get_routerCids();

            //verify all interfaces have the same LP as specified in constructor
	    for(int k=0; k<N; k++) {
		CPPUNIT_ASSERT_EQUAL(LP, (int)Component::GetComponentLP(ni_cids[k]));
		CPPUNIT_ASSERT_EQUAL(LP, (int)Component::GetComponentLP(router_cids[k]));
	    }

	    delete myNetwork;
	}
    }


    //======================================================================
    //======================================================================
    //! @brief Test constructor: network interfaces and routers.
    //!
    //! Create a network with N interfaces on LP 0; verify all
    //! the N interfaces are indeed related to the corresponding component id,
    //! and so are the routers.
    void testConstructor_3()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int N = random() % 64 + 2;

            GenNI_flow_control_setting setting;
	    setting.credits = random()%10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    RingNetwork* myNetwork = new RingNetwork(N, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();
	    std::vector<CompId_t>& router_cids = myNetwork->get_routerCids();
	    std::vector<RingRouter*>& routers = myNetwork->get_routers();

            //verify interfaces are related to the cids
	    for(int k=0; k<N; k++) {
	        CPPUNIT_ASSERT(interfaces[k] != 0);
		CPPUNIT_ASSERT(interfaces[k] == Component::GetComponent(ni_cids[k]));
	        CPPUNIT_ASSERT(routers[k] != 0);
		CPPUNIT_ASSERT(routers[k] == Component::GetComponent(router_cids[k]));
	    }
	    delete myNetwork;
	}
    }


    //======================================================================
    //======================================================================
    //! @brief Test constructor: network interfaces and routers.
    //!
    //! Create a network with N interfaces on LP 0; verify all
    //! the N interfaces have ids from 0 to N-1. And so are the routers.
    void testConstructor_4()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<1; i++) { //try 100 networks
	    int N = random() % 64 + 2;

            GenNI_flow_control_setting setting;
	    setting.credits = random()%10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    RingNetwork* myNetwork = new RingNetwork(N, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();
	    std::vector<RingRouter*>& routers = myNetwork->get_routers();

            //verify interface ids are 0 to N-1; as are routers.
	    for(int k=0; k<N; k++) {
	        CPPUNIT_ASSERT(interfaces[k] != 0);
		CPPUNIT_ASSERT_EQUAL(k, interfaces[k]->get_id());
	        CPPUNIT_ASSERT(routers[k] != 0);
		CPPUNIT_ASSERT_EQUAL(k, routers[k]->get_id());
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
	    int N = random() % 64 + 2;

            GenNI_flow_control_setting setting;
	    setting.credits = random()%10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    RingNetwork* myNetwork = new RingNetwork(N, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();

            //verify interface ids are 0 to N-1
	    for(int k=0; k<N; k++) {
	        CPPUNIT_ASSERT(interfaces[k] != 0);
		CPPUNIT_ASSERT(myNetwork == interfaces[k]->get_network());
	    }
	    delete myNetwork;
	}
    }


    //======================================================================
    //======================================================================
    //! @brief Test constructor: network interfaces and routers.
    //!
    //! @brief Create a network with N interfaces on LP 0; verify all
    //! the N interfaces have the same ID as their corresponding routers.
    void testConstructor_6()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

        for(int i=0; i<100; i++) { //try 100 networks
	    int N = random() % 64 + 2;

            GenNI_flow_control_setting setting;
	    setting.credits = random()%10 + 5;

	    //create a network with N interfaces in LP
	    GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	    RingNetwork* myNetwork = new RingNetwork(N, setting.credits, 0, MasterClock, ifcreator, 0);
	    delete ifcreator;
	    std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();
	    std::vector<RingRouter*>& routers = myNetwork->get_routers();

            //verify interface has same id as its router
	    for(int k=0; k<N; k++) {
	        CPPUNIT_ASSERT(interfaces[k] != 0);
		CPPUNIT_ASSERT_EQUAL(interfaces[k]->get_id(), routers[k]->get_id());
	    }
	    delete myNetwork;
	}
    }





    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MeshNetworkTest");

	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("testConstructor_0", &RingNetworkTest::testConstructor_0));
	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("testConstructor_1", &RingNetworkTest::testConstructor_1));
	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("testConstructor_2", &RingNetworkTest::testConstructor_2));
	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("testConstructor_3", &RingNetworkTest::testConstructor_3));
	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("testConstructor_4", &RingNetworkTest::testConstructor_4));
	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("testConstructor_5", &RingNetworkTest::testConstructor_5));
	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("testConstructor_6", &RingNetworkTest::testConstructor_6));

	return mySuite;
    }
};


Clock RingNetworkTest::MasterClock(MASTER_CLOCK_HZ);



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( RingNetworkTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


