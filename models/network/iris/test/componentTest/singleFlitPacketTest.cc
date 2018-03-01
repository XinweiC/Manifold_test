//This program tests sending/receiving single-flit packets.
#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>
#include <iostream>
#include <list>
#include <stdlib.h>
#include "../../genericTopology/genericTopoCreator.h"
#include "kernel/manifold.h"

using namespace manifold::kernel;
using namespace std;

using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################

#define CREDIT_PKT 123 //msg type for credit packets

class TerminalData {
public:
    static int Simu_len;
    int type;
    uint src;
    uint dst;
    int data;
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    uint get_src() { return src; }
    uint get_src_port() { return 0; }
    uint get_dst() { return dst; }
    uint get_dst_port() { return 0; }
    void set_dst_port(int p) { }
};

int TerminalData :: Simu_len = 0;

class MySimLen : public SimulatedLen<TerminalData> {
public:
    int get_simulated_len(TerminalData* d) { return d->Simu_len; }
};


class MyVnet : public VnetAssign<TerminalData> {
public:
    int get_virtual_net(TerminalData*) { return 0; }
};



//this class emulate the packet src
class MockTerminal : public manifold::kernel::Component {
public:
    enum {PORT_NI};

    MockTerminal(int id) : m_id(id) {}

    void send_data(TerminalData* data)
    {
        Send(PORT_NI, data);
    }

    void handle_interface(int, TerminalData* data)
    {
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
class topoCreatorCompTest : public CppUnit::TestFixture {
private:

public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    //======================================================================
    //======================================================================
    //! @brief Test ring network.
    //!
    //! Create a ring. Randomly select one as source. Send 1 packet from the
    //! source to each of the other terminals. Verify the other terminals
    //! receive the packet.
    void ring_test_0()
    {
        MasterClock.unregisterAll();
        
        //the paramters for ring network
        ring_init_params rp;
        rp.no_nodes = random() % 128 + 2; //between 2 and 129
        rp.no_vcs = 4; //must be 4
        rp.credits = random() % 5 + 3; //between 3 and 7
        rp.rc_method = RING_ROUTING;
        rp.link_width = 128;
	rp.ni_up_credits = 10;
	rp.ni_upstream_buffer_size = 5;

	const int N = rp.no_nodes;
        
        Clock& clk = MasterClock;
        
        //creat the ring network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new MySimLen;
	CPPUNIT_ASSERT(rp.link_width/8 > HeadFlit :: HEAD_FLIT_OVERHEAD);
	TerminalData :: Simu_len = random() % (rp.link_width/8 - HeadFlit::HEAD_FLIT_OVERHEAD) + 1;
	MyVnet* vnet = new MyVnet();

        Ring<TerminalData>* myNetwork = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, slen, vnet, CREDIT_PKT, 0, 0);

	vector <CompId_t> term_ids(N);
        vector <MockTerminal*> terms(N);
        const vector <manifold::kernel::CompId_t>& inf_ids = myNetwork->get_interface_id();
        
        //creat the terminals
        for (int i=0; i < inf_ids.size(); i++)
        {
            term_ids[i] = Component :: Create<MockTerminal> (0, i);
            terms[i] = Component :: GetComponent<MockTerminal>(term_ids[i]);
        }
        
        //connect terminal and interface together
        for (int i=0; i < inf_ids.size(); i++)
        {
            Manifold::Connect(term_ids[i], MockTerminal::PORT_NI, inf_ids[i], 
                          GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                          &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
            Manifold::Connect(inf_ids[i], GenNetworkInterface<TerminalData>::TERMINAL_PORT, term_ids[i], 
                          MockTerminal::PORT_NI, &MockTerminal::handle_interface, 1);               
        }

	//randomly select a source terminal
	int SRC = random() % N;

        //schedule events
	Manifold::unhalt();
	Ticks_t When = 1;

        vector<Ticks_t>  scheduleAt;

        //source sends a packet to all other terminals
	for(int i=0; i<N; i++) {
	    if(i != SRC) {
		TerminalData* data = new TerminalData;
		data->src = SRC;
		data->dst = i;
		data->data = i;

		Manifold::Schedule(When, &MockTerminal::send_data, terms[SRC], data);
		scheduleAt.push_back(When + MasterClock.NowTicks());
		When += 1;
	    }
	}

	Manifold::StopAt(When + 1000); //If test case fails, first try making the stop time larger.
	Manifold::Run();


	//verify the packets are received.
	//Is it possible to verify delays?

	for(int i=0; i<N; i++) { //for each terminal i
	    if(i != SRC) {
		vector<TerminalData>& received_data = terms[i]->get_data();

		CPPUNIT_ASSERT_EQUAL(1, (int)received_data.size()); //each received 1 packet.

		CPPUNIT_ASSERT(received_data[0].src != i); //src cannot be i 
		CPPUNIT_ASSERT_EQUAL(i, (int)received_data[0].dst); //dst must be i
		CPPUNIT_ASSERT_EQUAL(i, (int)received_data[0].data);

	    }
	}
	delete myNetwork;

    }


    //======================================================================
    //======================================================================
    //! @brief Test torus network.
    //!
    //! Create a torus. Randomly select one as source. Send 1 packet from the
    //! source to each of the other terminals. Verify the other terminals
    //! receive the packet.
    void torus_test_0()
    {
        MasterClock.unregisterAll();
        
        //the paramters for torus network
        torus_init_params rp;
        rp.x_dim = random() % 19 + 2; //between 2 and 20
        rp.y_dim = random() % 19 + 2; //between 2 and 20
        rp.no_vcs = 4; //must be 4
        rp.credits = random() % 5 + 3; //between 3 and 7
        rp.link_width = 128;
	rp.ni_up_credits = 10;
	rp.ni_upstream_buffer_size = 5;

	const int N = rp.x_dim * rp.y_dim;

	Scheduler* sch = Manifold::get_scheduler();
	//LbtsSyncAlg* alg = (LbtsSyncAlg*)(sch->m_syncAlg);
        
        
        Clock& clk = MasterClock;
        
        //creat the ring network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new MySimLen;
	CPPUNIT_ASSERT(rp.link_width/8 > HeadFlit :: HEAD_FLIT_OVERHEAD);
	TerminalData :: Simu_len = random() % (rp.link_width/8 - HeadFlit::HEAD_FLIT_OVERHEAD) + 1;
	MyVnet* vnet = new MyVnet();
	vector<int> node_lps(rp.x_dim*rp.y_dim);
	for(int i=0; i<rp.x_dim*rp.y_dim; i++)
	    node_lps[i] = 0;
        Torus<TerminalData>* myNetwork = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, slen, vnet, CREDIT_PKT, &node_lps);

	vector <CompId_t> term_ids(N);
        vector <MockTerminal*> terms(N);
        const vector <manifold::kernel::CompId_t>& inf_ids = myNetwork->get_interface_id();
        
        //creat the terminals
        for (int i=0; i < inf_ids.size(); i++)
        {
            term_ids[i] = Component :: Create<MockTerminal> (0, i);
            terms[i] = Component :: GetComponent<MockTerminal>(term_ids[i]);
        }
        
        //connect terminal and interface together
        for (int i=0; i < inf_ids.size(); i++)
        {
            Manifold::Connect(term_ids[i], MockTerminal::PORT_NI, inf_ids[i], 
                          GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                          &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
            Manifold::Connect(inf_ids[i], GenNetworkInterface<TerminalData>::TERMINAL_PORT, term_ids[i], 
                          MockTerminal::PORT_NI, &MockTerminal::handle_interface, 1);               
        }

	//randomly select a source terminal
	int SRC = random() % N;

//cout << "SRC ======= " << SRC << endl;
        //schedule events
	Manifold::unhalt();
	Ticks_t When = 1;

        vector<Ticks_t>  scheduleAt;

        //source sends a packet to all other terminals
	for(int i=0; i<N; i++) {
	    if(i != SRC) {
		TerminalData* data = new TerminalData;
		data->src = SRC;
		data->dst = i;
		data->data = i;

		Manifold::Schedule(When, &MockTerminal::send_data, terms[SRC], data);
		scheduleAt.push_back(When + MasterClock.NowTicks());
		When += 1;
	    }
	}

	Manifold::StopAt(When + 1000); //If test case fails, first try making the stop time larger.
	Manifold::Run();


	//verify the packets are received.
	//Is it possible to verify delays?

	for(int i=0; i<N; i++) { //for each terminal i
	    if(i != SRC) {
		vector<TerminalData>& received_data = terms[i]->get_data();
		CPPUNIT_ASSERT_EQUAL(1, (int)received_data.size()); //each received 1 packet.

		CPPUNIT_ASSERT(received_data[0].src != i); //src cannot be i 
		CPPUNIT_ASSERT_EQUAL(i, (int)received_data[0].dst); //dst must be i
		CPPUNIT_ASSERT_EQUAL(i, received_data[0].data);
	    }
	}
	//myNetwork->print_stats(cout);
	delete myNetwork;

    }



    
    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("topoCreatorTest");
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("ring_test_0", &topoCreatorCompTest::ring_test_0));
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("torus_test_0", &topoCreatorCompTest::torus_test_0));
	return mySuite;
    }
};

Clock topoCreatorCompTest :: MasterClock(topoCreatorCompTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( topoCreatorCompTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


