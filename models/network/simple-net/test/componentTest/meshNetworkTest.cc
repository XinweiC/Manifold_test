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
    Ticks_t send_tick;
    Ticks_t recv_tick;
};


class MockTerminal : public Component {
public:
    enum { OUT=0};
    enum { IN=0};

    MockTerminal(int id) : m_id(id) {}

    void send_data(TerminalData* data)
    {
        data->send_tick = Manifold :: NowTicks();
    //cout << "@@ " << data->send_tick << " terminal " << m_id << " sending" << endl;
        Send(OUT, data);
    }

    void handle_interface(int, TerminalData* data)
    {
        data->recv_tick = Manifold :: NowTicks();
    //cout << "@@ " << data->recv_tick << " terminal " << m_id << " received." << endl;
        m_data.push_back(TerminalData(*data));
	delete data;
    }

    vector<TerminalData>& get_data() { return m_data; }

private:
    int m_id;

    vector<TerminalData> m_data;
};


//####################################################################
//####################################################################
class MeshNetworkTest : public CppUnit::TestFixture {
private:
    enum { MASTER_CLOCK_HZ = 10 };
    static Clock MasterClock;
    static const int DELAY_TERM_NI = 1; //delay between terminal and network interface

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }

    static int manhattan_distance(int id1, int id2, int x)
    {
	int x1 = id1 % x;
	int y1 = id1 / x;

	int x2 = id2 % x;
	int y2 = id2 / x;

	int xdiff = x1 - x2;
	if(xdiff < 0)
	    xdiff = -xdiff;
	int ydiff = y1 - y2;
	if(ydiff < 0)
	    ydiff = -ydiff;

	return xdiff + ydiff;
    }


    //======================================================================
    //======================================================================
    //! @brief Verify when there's no congestion, packet delay is expected from a
    //! mesh network.
    //!
    //! Create a mesh network with X * Y interfaces; X and Y are randomly generated.
    //! Connect a terminal to each interface; Randomly select a terminal, send a packet
    //! to all other terminals; verify the delay is expected from a mesh.
    void test_0()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

	int X = random() % 32 + 1;
	int Y = random() % 32 + 1;
	if(X == 1 && Y == 1)
	    X = 2;

	int N = X*Y;

	GenNI_flow_control_setting setting;
	setting.credits = random()%10 + 5;

MasterClock.unregisterAll();

	//create a network with N interfaces
	GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	//Use simple mapping since terminal ID is same as interface ID.
	Simple_terminal_to_net_mapping* tn_mapping = new Simple_terminal_to_net_mapping();

	MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, 0, MasterClock, ifcreator, tn_mapping);
	delete ifcreator;

	std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();
	//std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();

	//create terminals
        CompId_t term_cids[N];
        MockTerminal* terms[N];
	for(int i=0; i<N; i++) {
	    term_cids[i] = Component :: Create<MockTerminal>(0, i);
	    terms[i] =  Component :: GetComponent <MockTerminal>(term_cids[i]);
	}

	//connect terminals to interfaces
	for(int i=0; i<N; i++) {
	    //terminal to NI
	    Manifold :: Connect(term_cids[i], MockTerminal::OUT,
				ni_cids[i], GenNetworkInterface<TerminalData>::IN_FROM_TERMINAL,
				&GenNetworkInterface<TerminalData> :: handle_terminal, DELAY_TERM_NI);
	    //NI to terminal
	    Manifold :: Connect(ni_cids[i], GenNetworkInterface<TerminalData>::OUT_TO_TERMINAL,
	                        term_cids[i], MockTerminal::IN, &MockTerminal :: handle_interface, DELAY_TERM_NI);

	}


        //randomly select a source terminal
	int SRC = random() % N;

        //schedule events
	Manifold::unhalt();
	Ticks_t When = 1;

        vector<Ticks_t> scheduleAt;

        //source terminal sends a packet to all other terminals
	for(int i=0; i<N; i++) {
	    if(i != SRC) {
		TerminalData* data = new TerminalData;
		data->src = SRC;
		data->dst = i;

		Manifold::Schedule(When, &MockTerminal::send_data, terms[SRC], data);
		scheduleAt.push_back(When + MasterClock.NowTicks());
		When += 1;
	    }
	}

	Manifold::StopAt(When + 100);
	Manifold::Run();


	//verify the delays are expected from a mesh network.

	for(int i=0; i<N; i++) { //for each terminal i
	    if(i != SRC) {
		vector<TerminalData>& received_data = terms[i]->get_data();

		CPPUNIT_ASSERT_EQUAL(1, (int)received_data.size()); //each received 1 packet

		CPPUNIT_ASSERT(received_data[0].src != i); //src cannot be i 
		CPPUNIT_ASSERT_EQUAL(i, received_data[0].dst); //dst must be i

		//calculate inter-router delay that we'd expect from a mesh, given source and dest.
		int dst = received_data[0].dst;
		int router_delay = manhattan_distance(SRC, dst, X) * myNetwork->get_router_router_delay();

		//verify the delay is as expected.
		//since packet might be blocked at routers; the delay we calculate above is the lower bound.
		CPPUNIT_ASSERT(int(myNetwork->get_ni_router_delay() * 2 + router_delay + DELAY_TERM_NI * 2) <=
		                     int(received_data[0].recv_tick - received_data[0].send_tick));
	    }
	}
	delete myNetwork;
    }




    //======================================================================
    //======================================================================
    //! @brief Verify flow control.
    //!
    //! Create a 1x3 mesh network. Connect a terminal to each interface. Inject N
    //! packets, one per cycle, into both terminal 0 and 2, all destined to terminal 1.
    //! Verify terminal 1 receives at most 1 packet per cycle.  In this test case,
    //! 2 packets go into the network every cycle, but only one comes out. This
    //! verifies the flow control mechanism. Also, due to round-robin priority,
    //! all received packets should have their sources alternating between 0 and 2.
    //!
    //!        | term 0 |        | term 1 |        | term 2 |
    //!            |                 |                 |
    //!         | NI 0 |          | NI 1 |          | NI 2 |
    //!            |                 |                 |
    //!          | R0 |            | R1 |            | R2 |
    //!
    void test_1()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

	const int X = 3;
	const int Y = 1;

	const int N = X*Y;

	GenNI_flow_control_setting setting;
	setting.credits = random()%3 + 2;  //small buffer size

MasterClock.unregisterAll();

	//create a network with N interfaces
	GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	//Use simple mapping since terminal ID is same as interface ID.
	Simple_terminal_to_net_mapping* tn_mapping = new Simple_terminal_to_net_mapping();

	MeshNetwork* myNetwork = new MeshNetwork(X, Y, setting.credits, 0, MasterClock, ifcreator, tn_mapping);
	delete ifcreator;

	std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();
	//std::vector<NetworkInterfaceBase*>& interfaces = myNetwork->get_interfaces();

	//create terminals
        CompId_t term_cids[N];
        MockTerminal* terms[N];
	for(int i=0; i<N; i++) {
	    term_cids[i] = Component :: Create<MockTerminal>(0, i);
	    terms[i] =  Component :: GetComponent <MockTerminal>(term_cids[i]);
	}

	//connect terminals to interfaces
	for(int i=0; i<N; i++) {
	    //terminal to NI
	    Manifold :: Connect(term_cids[i], MockTerminal::OUT,
				ni_cids[i], GenNetworkInterface<TerminalData>::IN_FROM_TERMINAL,
				&GenNetworkInterface<TerminalData> :: handle_terminal, DELAY_TERM_NI);
	    //NI to terminal
	    Manifold :: Connect(ni_cids[i], GenNetworkInterface<TerminalData>::OUT_TO_TERMINAL,
	                        term_cids[i], MockTerminal::IN, &MockTerminal :: handle_interface, DELAY_TERM_NI);

	}


        //schedule events
	Manifold::unhalt();
	Ticks_t When = 1;

	const int Npkts = 100;

        //vector<Ticks_t> scheduleAt0(Npkts);
        //vector<Ticks_t> scheduleAt2(Npkts);

        //terminals 0 and 2 both send packets to terminal 1
	for(int i=0; i<Npkts; i++) {
	    TerminalData* data = new TerminalData;
	    data->src = 0;
	    data->dst = 1;

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[0], data);

	    //scheduleAt.push_back(When + MasterClock.NowTicks());

	    data = new TerminalData;
	    data->src = 2;
	    data->dst = 1;

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[2], data);

	    When += 1;
	}


        // If one packet comes out every cycle, we need at least 200 cycles for all
	// packets to reach the destination.
	Manifold::StopAt(When + Npkts + 10);
	Manifold::Run();


	vector<TerminalData>& received_data = terms[1]->get_data();

        //verify Npkts*2 packets have been received.
	CPPUNIT_ASSERT_EQUAL(Npkts*2, (int)received_data.size());

        //verify only 1 packet was received each tick
	for(unsigned i=0; i<received_data.size()-1; i++) {
	    CPPUNIT_ASSERT_EQUAL(received_data[i].recv_tick + 1, received_data[i+1].recv_tick);
	}

	//Verify received packets have sources [0] 2 0 2 0 2 ...
	for(unsigned i=0; i<received_data.size()-1; i++) {
	    if(received_data[i].src == 0)
		CPPUNIT_ASSERT_EQUAL(2, received_data[i+1].src);
	    else {
		CPPUNIT_ASSERT_EQUAL(2, received_data[i].src);
		CPPUNIT_ASSERT_EQUAL(0, received_data[i+1].src);
	    }

	}

	delete myNetwork;
    }





    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MeshNetworkTest");

	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("test_0", &MeshNetworkTest::test_0));
	mySuite->addTest(new CppUnit::TestCaller<MeshNetworkTest>("test_1", &MeshNetworkTest::test_1));

	return mySuite;
    }
};


Clock MeshNetworkTest::MasterClock(MASTER_CLOCK_HZ);



int main()
{
    Manifold::Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MeshNetworkTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


