#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "interfaceCreator.h"
#include "network.h"
#include "networkInterface.h"

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

//! This class emulates a terminal
class MockTerminal : public manifold::kernel::Component {
public:
    enum {IN=0};

    void handle_incoming(int, TerminalData* pkt)
    {
        m_pkts.push_back(*pkt);
        m_ticks.push_back(Manifold :: NowTicks());
	delete pkt;
    }

    list<TerminalData>& get_pkts() { return m_pkts; }
    list<Ticks_t>& get_ticks() { return m_ticks; }
private:
    list<TerminalData> m_pkts;
    list<Ticks_t> m_ticks;
};



//! This class emulates a router
class MockRouter : public manifold::kernel::Component {
public:
    enum {IN=0};

    void handle_incoming(int, NetworkPkt* pkt)
    {
        if(pkt->type == NetworkPkt::DATA)
	    m_pkts.push_back(*pkt);
	else
	    m_credits.push_back(*pkt);
        //m_ticks.push_back(Manifold :: NowTicks());
	delete pkt;
    }

    list<NetworkPkt>& get_pkts() { return m_pkts; }
    list<NetworkPkt>& get_credits() { return m_credits; }
    //list<Ticks_t>& get_ticks() { return m_ticks; }
private:
    list<NetworkPkt> m_pkts;
    list<NetworkPkt> m_credits;
    //list<Ticks_t> m_ticks;
};


// The handle_terminal() function requires a network object, but I don't
// want to create network objects such as MeshNetwork which creates routers
// and register with clock, etc. So this dummy network class is created.
class DummyNetwork : public SimpleNetwork {
public:
    DummyNetwork(int n, manifold::kernel::LpId_t lp, InterfaceCreator* ic, Terminal_to_net_mapping* m) :
	SimpleNetwork(n, lp, ic, m) {}
};



void test_rising_edge_helper(GenNetworkInterface<TerminalData>* interface)
{
    interface->rising_edge();
}



//####################################################################
//! Class NetworkInterfaceTest is the test class for class NetworkInterface. 
//####################################################################
class NetworkInterfaceTest : public CppUnit::TestFixture {
private:
    //latencies
    static const int NI_TO_TERMINAL = 1;

    static const int X_DIM = 4;
    static const int Y_DIM = 2;
    //static const int NUM_NODES = X_DIM * Y_DIM;

public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }




    //======================================================================
    //======================================================================
    //! @brief Test constructor: id.
    //!
    //! Create N network interface objects with random IDs; verify the ID
    //! is what's in the constructor.
    void testConstructor_0()
    {
        GenNI_flow_control_setting setting;
	setting.credits = 10;

        for(int i=0; i<1000; i++) {
	    int id = random() % 1000;
	    GenNetworkInterface<TerminalData>* ni = new GenNetworkInterface<TerminalData>(id, setting);

	    CPPUNIT_ASSERT_EQUAL(id, ni->get_id());
	    delete ni;
	}

    }



    //======================================================================
    //======================================================================
    //! @brief Test constructor: flow control setting.
    //!
    //! Create N network interface objects with random flow control settings;
    //! verify the flow control parameters are initialized correctly.
    void testConstructor_1()
    {
        for(int i=0; i<1000; i++) {
	    GenNI_flow_control_setting setting;
	    setting.credits = random() % 1000;

	    GenNetworkInterface<TerminalData>* ni = new GenNetworkInterface<TerminalData>(1, setting);

	    CPPUNIT_ASSERT_EQUAL(setting.credits, ni->m_CREDITS);
	    CPPUNIT_ASSERT_EQUAL(setting.credits, ni->m_credits_router);
	    delete ni;
	}

    }



    //======================================================================
    //======================================================================
    //! @brief Verify handle_terminal() puts packet in the correct input buffer.
    //!
    //! Create a network interface; call handle_terminal; verify the data is
    //! put in a packet and appended to the input buffer.
    void testHandle_terminal_0()
    {
        GenNI_flow_control_setting setting;
	setting.credits = 10;

        //handle_terminal() requires pointer to network, so create a dummy network
	GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	DummyNetwork* myNetwork = new DummyNetwork(2, 0, ifcreator, mapping); //network creates all the network interfaces.
	delete ifcreator;

        //create a network interface
	GenNetworkInterface<TerminalData>* ni = new GenNetworkInterface<TerminalData>(1, setting);
	ni->set_network(myNetwork);

	const int SRC = 111;
	const int DST = 222;
	TerminalData* data = new TerminalData;
	data->src = SRC;
	data->dst = DST;

        //call handle_terminal()
	ni->handle_terminal(GenNetworkInterface<TerminalData> :: IN_FROM_TERMINAL, data);
	NetworkPkt* pkt = ni->m_input_from_terminal.back();

	TerminalData* ptr = (TerminalData*)(pkt->data);

	CPPUNIT_ASSERT_EQUAL(SRC, ptr->src);
	CPPUNIT_ASSERT_EQUAL(DST, ptr->dst);

        delete ni;
	delete myNetwork;
    }




    //======================================================================
    //======================================================================
    //! @brief Verify handle_router() puts packet in the correct input buffer.
    //!
    //! Create a network interface; call handle_router; verify the packet
    //! is appended to the input buffer.
    void testHandle_router_0()
    {
        GenNI_flow_control_setting setting;
	setting.credits = 10;

        const int NI_ID = 7;

        GenNetworkInterface<TerminalData>* ni = new GenNetworkInterface<TerminalData>(NI_ID, setting);

	NetworkPkt* pkt = new NetworkPkt;
	pkt->type = NetworkPkt :: DATA;
	pkt->dst = NI_ID;

	const int SRC = 111;
	const int DST = 222;
	TerminalData data;
	data.src = SRC;
	data.dst = DST;

	//put the data into the packet
	uint8_t* ptr = (uint8_t*)&data;
	for(unsigned i=0; i<sizeof(TerminalData); i++) {
	    pkt->data[i] = *ptr;
	    ptr++;
	}


        //call handle_router()
	ni->handle_router(GenNetworkInterface<TerminalData> :: IN_FROM_ROUTER, pkt);

	TerminalData* dptr = (TerminalData*)(ni->m_input_from_router.back()->data);

	CPPUNIT_ASSERT_EQUAL(SRC, dptr->src);
	CPPUNIT_ASSERT_EQUAL(DST, dptr->dst);

	delete ni;
    }



    //======================================================================
    //======================================================================
    //! @brief Verify handle_router() puts packet in the correct input buffer.
    //!
    //! Create a network interface; call handle_router 2 times; verify the packets
    //! are put in the buffer in correct order
    void testHandle_router_1()
    {
        GenNI_flow_control_setting setting;
	setting.credits = 10;

        const int NI_ID = 7;

        GenNetworkInterface<TerminalData>* ni = new GenNetworkInterface<TerminalData>(NI_ID, setting);


	TerminalData data[2];
	for(int i=0; i<2; i++) {
	    data[i].src = random() % 1000;
	    data[i].dst = random() % 1000;
        }

        NetworkPkt* pkts[2];

	for(int i=0; i<2; i++) {
	    pkts[i] = new NetworkPkt;
	    pkts[i]->type = NetworkPkt :: DATA;
	    pkts[i]->dst = NI_ID;

	    //put the data into the packet
	    uint8_t* ptr = (uint8_t*)&data[i];
	    for(unsigned b=0; b<sizeof(TerminalData); b++) {
		pkts[i]->data[b] = *ptr;
		ptr++;
	    }

	    //call handle_router()
	    ni->handle_router(GenNetworkInterface<TerminalData> :: IN_FROM_ROUTER, pkts[i]);
	}

	TerminalData* dptr0 = (TerminalData*)(ni->m_input_from_router.front()->data);
	TerminalData* dptr1 = (TerminalData*)(ni->m_input_from_router.back()->data);

	CPPUNIT_ASSERT_EQUAL(data[0].src, dptr0->src);
	CPPUNIT_ASSERT_EQUAL(data[0].dst, dptr0->dst);

	CPPUNIT_ASSERT_EQUAL(data[1].src, dptr1->src);
	CPPUNIT_ASSERT_EQUAL(data[1].dst, dptr1->dst);

	delete ni;
    }




    //======================================================================
    //======================================================================
    //! @brief Verify for credits B, handle_router() can be called B times.
    //!
    //! Create a network interface with credits B set to a random value;
    //! call handle_router() B times; verify the input buffer size. Repeat N times.
    void testHandle_router_2()
    {
        const int NI_ID = 7;

        for(int i=0; i<1000; i++) {
	    GenNI_flow_control_setting setting;
	    setting.credits = random() % 100 + 1; //avoid 0

	    GenNetworkInterface<TerminalData>* ni = new GenNetworkInterface<TerminalData>(NI_ID, setting);

            //can accept at most setting.credits packets from router
	    for(int i=0; i<setting.credits; i++) {
		NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: DATA;
		pkt->dst = NI_ID;

		//call handle_router()
		ni->handle_router(GenNetworkInterface<TerminalData> :: IN_FROM_ROUTER, pkt);
	    }

	    CPPUNIT_ASSERT_EQUAL(setting.credits, (int)(ni->m_input_from_router.size()) );
	    delete ni;
	}

    }




    //======================================================================
    //======================================================================
    //! @brief Verify handle_router() correctly updates credit.
    //!
    //! Create a network interface with router credit C set to a random value;
    //! Manually reset the interface's router credit to 0; then send N (0<=N<=C) credit
    //! packets to the interface by calling handle_router(); verify the interface's router
    //! credit becomes N. Repeat a number of times.
    void testHandle_router_3()
    {
        const int NI_ID = 7; //interface's ID

        for(int i=0; i<1000; i++) { //repeat 1000 times
	    GenNI_flow_control_setting setting;
	    setting.credits = random() % 1000;

	    GenNetworkInterface<TerminalData>* ni = new GenNetworkInterface<TerminalData>(NI_ID, setting);

            //manually set the credit to 0
	    ni->m_credits_router = 0;

            //Send N credits, 0<= N < initial credit
	    int N = random() % setting.credits;

	    for(int i=0; i<N; i++) {
		NetworkPkt* pkt = new NetworkPkt;
		pkt->type = NetworkPkt :: CREDIT; //type must be CREDIT
		pkt->dst = NI_ID;

		//call handle_router()
		ni->handle_router(GenNetworkInterface<TerminalData> :: IN_FROM_ROUTER, pkt);
	    }

            //verify credit is N
	    CPPUNIT_ASSERT_EQUAL(N, ni->m_credits_router);
	    delete ni;
	}
    }



    //======================================================================
    //======================================================================
    //! @brief Create a mock terminal and a mock router and connect the network interface
    //! to them. Generate N packets from the terminal. Then call rising_edge() N times;
    //! veryify after each call the credit is decremented until it becomes 0.
    void testRising_edge_0()
    {

	GenNI_flow_control_setting setting;
	setting.credits = random() % 100 + 1; //randomly set the initial router credit; avoid 0


        const int NI_ID = 7; //interface's ID

        //handle_terminal() requires pointer to network, so create a dummy network
	GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	DummyNetwork* myNetwork = new DummyNetwork(2, 0, ifcreator, mapping); //network creates all the network interfaces.
	delete ifcreator;



        //create a network interface
	CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, NI_ID, setting);
	GenNetworkInterface<TerminalData>* interface = Component::GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
	interface->set_network(myNetwork);

        //create a terminal
	CompId_t terminal_cid = Component :: Create<MockTerminal>(0);
	MockTerminal* terminal = Component::GetComponent<MockTerminal>(terminal_cid);

        //create a router
	CompId_t router_cid = Component :: Create<MockRouter>(0);
	MockRouter* router = Component::GetComponent<MockRouter>(router_cid);


        //connect the components
	//interface to terminal
	Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::OUT_TO_TERMINAL, terminal_cid, MockTerminal::IN,
			  &MockTerminal::handle_incoming, NI_TO_TERMINAL);
	//interface to router
	Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::OUT_TO_ROUTER, router_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, NI_TO_TERMINAL);




        //create N packets from the terminal
        int Npkts = setting.credits * 2;

	for(int i=0; i<Npkts; i++) {
	    TerminalData* data = new TerminalData;
	    //handle_terminal() would delete data and create a NetworkPkt.
	    interface->handle_terminal(GenNetworkInterface<TerminalData> :: IN_FROM_TERMINAL, data);
        }

        int credits = interface->m_credits_router;

        //verify every time rising_edge() sends a packet to router, credit is decremented
	for(int i=0; i<Npkts; i++) {
	    interface->rising_edge();
	    if(credits > 0)
		credits--;
	    CPPUNIT_ASSERT_EQUAL(credits, interface->m_credits_router);
	}

	Manifold::unhalt();

	Manifold::StopAt(1);
	Manifold::Run(); //get the events processed

        //verify although rising_edge() were called N times, only setting.credit_router packets
	//were sent; the rest were not sent because the credit was 0
	CPPUNIT_ASSERT_EQUAL(setting.credits, (int)router->get_pkts().size());

	delete myNetwork;
	delete interface;
	delete terminal;
	delete router;

    }


    //======================================================================
    //======================================================================
    //! @brief Test packet forwarding: head packets from the 2 input buffers are forwarded in the
    //! same cycle.
    //!
    //! Create a mock terminal and a mock router and connect the network interface
    //! to them. Inject 1 packet from the terminal and 1 from the router. Then schedule for
    //! rising_edge() to be called once.
    //! veryify both packets were delivered.
    void testRising_edge_1()
    {
	GenNI_flow_control_setting setting;
	setting.credits = random() % 100 + 1; //randomly set the initial router credit; avoid 0


        const int NI_ID = 7; //interface's ID

        //handle_terminal() requires pointer to network, so create a dummy network
	GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	DummyNetwork* myNetwork = new DummyNetwork(2, 0, ifcreator, mapping); //network creates all the network interfaces.
	delete ifcreator;



        //create a network interface
	CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, NI_ID, setting);
	GenNetworkInterface<TerminalData>* interface = Component::GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
	interface->set_network(myNetwork);

        //create a terminal
	CompId_t terminal_cid = Component :: Create<MockTerminal>(0);
	MockTerminal* terminal = Component::GetComponent<MockTerminal>(terminal_cid);

        //create a router
	CompId_t router_cid = Component :: Create<MockRouter>(0);
	MockRouter* router = Component::GetComponent<MockRouter>(router_cid);


        //connect the components
	//interface to terminal
	Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::OUT_TO_TERMINAL, terminal_cid, MockTerminal::IN,
			  &MockTerminal::handle_incoming, NI_TO_TERMINAL);
	//interface to router
	Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::OUT_TO_ROUTER, router_cid, MockRouter::IN,
			  &MockRouter::handle_incoming, NI_TO_TERMINAL);


	//call handle_terminal() once to inject a packet from terminal.
	TerminalData* data = new TerminalData;
	//handle_terminal() would delete data and create a NetworkPkt.
	interface->handle_terminal(GenNetworkInterface<TerminalData> :: IN_FROM_TERMINAL, data);

        //call handle_router() once to inject a packet from router
	NetworkPkt* pkt = new NetworkPkt;
	pkt->type = NetworkPkt::DATA;
	pkt->dst = NI_ID;
	interface->handle_router(GenNetworkInterface<TerminalData> :: IN_FROM_ROUTER, pkt);


        //schedule for rising_edge() to be called once.
	Manifold::unhalt();

	Ticks_t When = 1;

	//call rising_edge()
	Manifold::Schedule(When, test_rising_edge_helper, interface);
	When += 1;

	Manifold::StopAt(When);
	Manifold::Run(); //get the events processed

        //verify terminal and router each got one packet.
	CPPUNIT_ASSERT_EQUAL(1, (int)terminal->get_pkts().size());
	CPPUNIT_ASSERT_EQUAL(1, (int)router->get_pkts().size());

	delete myNetwork;
	delete interface;
	delete terminal;
	delete router;

    }






    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("NetworkInterfaceTest");

/*
	*/
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testConstructor_0", &NetworkInterfaceTest::testConstructor_0));
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testConstructor_1", &NetworkInterfaceTest::testConstructor_1));
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testHandle_terminal_0", &NetworkInterfaceTest::testHandle_terminal_0));
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testHandle_router_0", &NetworkInterfaceTest::testHandle_router_0));
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testHandle_router_1", &NetworkInterfaceTest::testHandle_router_1));
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testHandle_router_2", &NetworkInterfaceTest::testHandle_router_2));
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testHandle_router_3", &NetworkInterfaceTest::testHandle_router_3));
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testRising_edge_0", &NetworkInterfaceTest::testRising_edge_0));
	mySuite->addTest(new CppUnit::TestCaller<NetworkInterfaceTest>("testRising_edge_1", &NetworkInterfaceTest::testRising_edge_1));

	return mySuite;
    }
};


Clock NetworkInterfaceTest :: MasterClock(NetworkInterfaceTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold::Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( NetworkInterfaceTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


