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
        Send(OUT, data);
    }

    void handle_interface(int, TerminalData* data)
    {
        data->recv_tick = Manifold :: NowTicks();
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
class RingNetworkTest : public CppUnit::TestFixture {
private:
    enum { MASTER_CLOCK_HZ = 10 };
    static Clock MasterClock;
    static const int DELAY_TERM_NI = 1; //delay between terminal and network interface

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }


    //======================================================================
    //======================================================================
    //! @brief Verify when there's no congestion, packet dealy is expected from a
    //! ring network.
    //!
    //! Create a ring network with N interfaces; N is randomly generated.
    //! Connect a terminal to each interface. Randomly select a node and send a packet
    //! to all other terminals; verify the delay is expected from a unidirectional
    //! ring.
    void test_0()
    {
    	struct timeval ts;
	gettimeofday(&ts, NULL);
	srandom(ts.tv_usec);

	int N = random() % 64 + 2;

        GenNI_flow_control_setting setting;
	setting.credits = random()%10 + 5;


MasterClock.unregisterAll();

	//create a network with N interfaces
	GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	//Use simple mapping since terminal ID is same as interface ID.
	Simple_terminal_to_net_mapping* tn_mapping = new Simple_terminal_to_net_mapping();

	RingNetwork* myNetwork = new RingNetwork(N, setting.credits, 0, MasterClock, ifcreator, tn_mapping);
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

        vector<Ticks_t>  scheduleAt;

        //each terminal sends a packet to all other terminals
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


	//verify the delays are expected from a ring network.

	for(int i=0; i<N; i++) { //for each terminal i
	    if(i != SRC) {
		vector<TerminalData>& received_data = terms[i]->get_data();

		CPPUNIT_ASSERT_EQUAL(1, (int)received_data.size()); //each received 1 packet.

		CPPUNIT_ASSERT(received_data[0].src != i); //src cannot be i 
		CPPUNIT_ASSERT_EQUAL(i, received_data[0].dst); //dst must be i

		//calculate inter-router delay that we'd expect from a ring, given source and dest.
		int src = received_data[0].src;
		int dst = received_data[0].dst;
		int router_delay = 0; 
		if(src < dst)
		    router_delay = (dst - src) * myNetwork->get_router_router_delay();
                else
		    router_delay = (dst + N - src) * myNetwork->get_router_router_delay();

		//verify the delay is as expected.
		CPPUNIT_ASSERT_EQUAL(int(myNetwork->get_ni_router_delay() * 2 + router_delay + DELAY_TERM_NI * 2),
		                     int(received_data[0].recv_tick - received_data[0].send_tick));
	    }
	}
	delete myNetwork;
    }




    //======================================================================
    //======================================================================
    //! @brief Verify flow control.
    //!
    //! Create a 3-node ring network. Connect a terminal to each interface. Inject N
    //! packets, one per cycle, into both terminal 0 and 1, all destined to terminal 2.
    //! Verify terminal 2 receives at most 1 packet per cycle.  In this test case,
    //! 2 packets go into the network every cycle, but only one comes out. This
    //! verifies the flow control mechanism. Also, due to round-robin priority,
    //! all received packets should have their sources alternating between 0 and 1.
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

	int N = 3;

        GenNI_flow_control_setting setting;
	setting.credits = random()%3 + 2; //small buffer size


MasterClock.unregisterAll();

	//create a network with N interfaces
	GenInterfaceCreator<GenNetworkInterface<TerminalData> >* ifcreator = new GenInterfaceCreator<GenNetworkInterface<TerminalData> >(setting);
	//Use simple mapping since terminal ID is same as interface ID.
	Simple_terminal_to_net_mapping* tn_mapping = new Simple_terminal_to_net_mapping();

	RingNetwork* myNetwork = new RingNetwork(N, setting.credits, 0, MasterClock, ifcreator, tn_mapping);
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

	//terminals 0 and 1 both send packets to terminal 2
	for(int i=0; i<Npkts; i++) {
	    TerminalData* data = new TerminalData;
	    data->src = 0;
	    data->dst = 2;

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[0], data);

	    data = new TerminalData;
	    data->src = 1;
	    data->dst = 2;

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[1], data);

	    When += 1;
	}

	Manifold::StopAt(When + Npkts + 10);
	Manifold::Run();


	vector<TerminalData>& received_data = terms[2]->get_data();

        //verify Npkts*2 packets have been received.
	CPPUNIT_ASSERT_EQUAL(Npkts*2, (int)received_data.size());

        //verify only 1 packet was received each tick
	for(unsigned i=0; i<received_data.size()-1; i++) {
	    CPPUNIT_ASSERT_EQUAL(received_data[i].recv_tick + 1, received_data[i+1].recv_tick);
	}



        //Verify received packets have sources [0] 1 0 1 0 1 ...
	for(unsigned i=0; i<received_data.size()-1; i++) {
	    if(received_data[i].src == 0)
		CPPUNIT_ASSERT_EQUAL(1, received_data[i+1].src);
	    else {
		CPPUNIT_ASSERT_EQUAL(1, received_data[i].src);
		CPPUNIT_ASSERT_EQUAL(0, received_data[i+1].src);
	    }

	}


	delete myNetwork;
    }





    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MeshNetworkTest");

	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("test_0", &RingNetworkTest::test_0));
	mySuite->addTest(new CppUnit::TestCaller<RingNetworkTest>("test_1", &RingNetworkTest::test_0));

	return mySuite;
    }
};


Clock RingNetworkTest::MasterClock(MASTER_CLOCK_HZ);



int main()
{
    Manifold::Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( RingNetworkTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


