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

class TerminalData {
public:
    int type;
    uint src;
    uint dst;
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    uint get_src() { return src; }
    unsigned get_src_port() { return 0; }
    uint get_dst() { return dst; }
    uint get_dst_port() { return 0; }
    void set_dst_port(int p) {}
};


//this class emulate the packet src
class MockTerminal : public manifold::kernel::Component {
public:
    enum {PORT_NI};

    MockTerminal(int id) : m_id(id), np(0) {}

    void send_data(TerminalData* data)
    {
//        cout << "terminal " << m_id << " sending packet type " << data->type << " at " <<  Manifold :: NowTicks() << endl;
        Send(PORT_NI, data);
	np++;
    }

    void handle_interface(int, TerminalData* data)
    {
        //data->recv_tick = Manifold :: NowTicks();
 //       cout << "terminal " << m_id << " got packet type " << data->type << " at " <<  Manifold :: NowTicks() << endl;
        m_data.push_back(TerminalData(*data));
	delete data;
    }

    vector<TerminalData>& get_data() { return m_data; }

int np;
private:
    int m_id;

    vector<TerminalData> m_data;

};



#define CREDIT_PKT 123


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
    //! Create a ring. Randomly select a source and a destination. Send N
    //! packets. Since the destination interface's upstream credit C <= N,
    //! only C packets are received.
    //!
    void test_ring_flow_control_0()
    {
        MasterClock.unregisterAll();
        
        //the paramters for ring network
        ring_init_params rp;
        rp.no_nodes = random() % 128 + 2; //between 2 and 129
        rp.no_vcs = 4; //must be 4.
        rp.credits = random() % 5 + 3; //between 3 and 7
        rp.rc_method = RING_ROUTING;
        rp.link_width = 128;
	rp.ni_up_credits = random() % 90 + 10; //between 10 and 99.
	rp.ni_upstream_buffer_size = 5;

	const int N = rp.no_nodes;
        
        Clock& clk = MasterClock;
        
        //creat the ring network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();
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

	//randomly select a source and a destination terminal
	int SRC = random() % N;
	int DST;
	while((DST = random() % N) == SRC);

	const int Npkts = random() % 20 + 20 + rp.ni_up_credits; //number of pkts > upstream_credits
//cout << "Npkts= " << Npkts << "\n";
//cout << "up credits= " << rp.ni_up_credits << "\n";
        //schedule events
	Manifold::unhalt();
	Ticks_t When = 1;

        vector<Ticks_t>  scheduleAt;

        //source sends Npkts packets to destination
	for(int i=0; i<Npkts; i++) {
	    TerminalData* data = new TerminalData;
	    data->src = SRC;
	    data->dst = DST;

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[SRC], data);
	    //scheduleAt.push_back(When + MasterClock.NowTicks());
	    When += 1;
	}

	Manifold::StopAt(When + rp.no_nodes + 1000); //If test case fails, first try making the stop time larger.
	Manifold::Run();


	//verify the number of packets received at destination
	vector<TerminalData>& received_data = terms[DST]->get_data();

	CPPUNIT_ASSERT_EQUAL((int)rp.ni_up_credits, (int)received_data.size());

        //const vector<GenNetworkInterface<TerminalData>*>& infs = myNetwork->get_interfaces();
	//for(int i=0; i<infs.size(); i++)
	    //infs[i]->print_stats(cout);

	//verify the number of credits received at the source
	vector<TerminalData>& src_recved_credits = terms[SRC]->get_data();

	CPPUNIT_ASSERT_EQUAL(Npkts, (int)src_recved_credits.size());

	delete mapping;
	delete slen;
	delete myNetwork;

    }



    //======================================================================
    //======================================================================
    //! @brief Test ring network.
    //!
    //! Create a ring. Randomly select a source and a destination. Send N
    //! packets. Since the destination interface's upstream credit C <= N,
    //! only C packets are received.
    //!
    void test_ring_flow_control_1()
    {
        MasterClock.unregisterAll();
        
        //the paramters for ring network
        ring_init_params rp;
        rp.no_nodes = random() % 128 + 2; //between 2 and 129
        //rp.no_vcs = random() % 6 + 2; //between 2 and 7
        rp.no_vcs = 4; //must be 4.
        rp.credits = random() % 5 + 3; //between 3 and 7
        rp.rc_method = RING_ROUTING;
        rp.link_width = 128;
	rp.ni_up_credits = random() % 90 + 10; //between 10 and 99.
	rp.ni_upstream_buffer_size = 5;

	const int N = rp.no_nodes;
        
        Clock& clk = MasterClock;
        
        //creat the ring network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();
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

	//randomly select a source and a destination terminal
	int SRC = random() % N;
	int DST;
	while((DST = random() % N) == SRC);

	const int DELTA = 20;
	const int Npkts = random() % 20 + DELTA + rp.ni_up_credits; //number of pkts > upstream_credits
	                                                            // Npkts - upstream_credits >= DELTA.
//cout << "Npkts= " << Npkts << "\n";
//cout << "up credits= " << rp.ni_up_credits << "\n";
//cout << "SRC= " << SRC << " DST= " << DST << "\n";
        //schedule events
	Manifold::unhalt();
	Ticks_t When = 1;

        vector<Ticks_t>  scheduleAt;

        //source sends Npkts packets to destination
	for(int i=0; i<Npkts; i++) {
	    TerminalData* data = new TerminalData;
	    data->src = SRC;
	    data->dst = DST;

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[SRC], data);
	    //scheduleAt.push_back(When + MasterClock.NowTicks());
	    When += 1;
	}
	When +=  rp.no_nodes + 10; //If test case fails, first try making this bigger.

        //number of credits to send back: at least 2
	int Ncredits = random() % (Npkts - rp.ni_up_credits);
	if(Ncredits < 2)
	    Ncredits = 2;

//cout << "Ncredits = " << Ncredits << endl;

	When += 500; //wait until enough packets have been delivered to destination terminal.
	             //If test case files, first try making this bigger.
	for(int i=0; i<Ncredits; i++) {
	    TerminalData* credit = new TerminalData;
	    credit->set_type(CREDIT_PKT);

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[DST], credit);
	    When += 1;
	}
	Manifold::StopAt(When + 5000); //If test case files, first try making this bigger.
	Manifold::Run();


	//verify the number of packets received at destination.
	vector<TerminalData>& received_data = terms[DST]->get_data();

	CPPUNIT_ASSERT_EQUAL((int)rp.ni_up_credits + Ncredits, (int)received_data.size());

	//verify the number of credits received at the source
	vector<TerminalData>& src_recved_credits = terms[SRC]->get_data();

	//Due to backpressure, the credits received by source terminal may not equal the number
	//of packets it sent; so comment out the next line.
	//CPPUNIT_ASSERT_EQUAL(Npkts, (int)src_recved_credits.size());


	delete mapping;
	delete slen;
	delete myNetwork;

    }



    //======================================================================
    //======================================================================
    //! @brief Test torus network.
    //!
    //! Create a ring. Randomly select a source and a destination. Send N
    //! packets. Since the destination interface's upstream credit C <= N,
    //! only C packets are received.
    //!
    void test_torus_flow_control_0()
    {
        MasterClock.unregisterAll();
        
        //the paramters for torus network
        torus_init_params rp;
        rp.x_dim = random() % 19 + 2; //between 2 and 20
        rp.y_dim = random() % 19 + 2; //between 2 and 20
        //rp.no_vcs = random() % 6 + 2; //between 2 and 7
        rp.no_vcs = 4; //must be 4.
        rp.credits = random() % 5 + 3; //between 3 and 7
        rp.link_width = 128;
	rp.ni_up_credits = random() % 90 + 10; //between 10 and 99.
	rp.ni_upstream_buffer_size = 5;

	const int N = rp.x_dim * rp.y_dim;
        
        Clock& clk = MasterClock;
        
        //creat the torus network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();
	vector<int> node_lps(N);
	for(int i=0; i<N; i++)
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

	//randomly select a source and a destination terminal
	int SRC = random() % N;
	int DST;
	while((DST = random() % N) == SRC);

	const int Npkts = random() % 20 + 20 + rp.ni_up_credits; //number of pkts >= upstream_credits

//cout << "up credits= " << rp.ni_up_credits << "\n";
        //schedule events
	Manifold::unhalt();
	Ticks_t When = 1;

        vector<Ticks_t>  scheduleAt;

        //source sends Npkts packets to destination
	for(int i=0; i<Npkts; i++) {
	    TerminalData* data = new TerminalData;
	    data->src = SRC;
	    data->dst = DST;

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[SRC], data);
	    //scheduleAt.push_back(When + MasterClock.NowTicks());
	    When += 1;
	}

	//Manifold::StopAt(When + rp.x_dim + rp.y_dim + 10); //If test case fails, first try making the stop time larger.
	Manifold::StopAt(When + 1000);  //If test case fails, first try making this bigger.
	Manifold::Run();


//cout << "SRC= " << SRC << "  DST= " << DST << endl;
	//verify the number of packets received.

	vector<TerminalData>& received_data = terms[DST]->get_data();

	CPPUNIT_ASSERT_EQUAL((int)rp.ni_up_credits, (int)received_data.size());

        //const vector<GenNetworkInterface<TerminalData>*>& infs = myNetwork->get_interfaces();
	//for(int i=0; i<infs.size(); i++)
	    //infs[i]->print_stats(cout);

	//verify the number of credits received at the source
	vector<TerminalData>& src_recved_credits = terms[SRC]->get_data();

	//Due to backpressure, the credits received by source terminal may not equal the number
	//of packets it sent; so comment out the next line.
	//CPPUNIT_ASSERT_EQUAL(Npkts, (int)src_recved_credits.size());

	delete mapping;
	delete slen;
	delete myNetwork;

    }



    //======================================================================
    //======================================================================
    //! @brief Test torus network.
    //!
    //! Create a ring. Randomly select a source and a destination. Send N
    //! packets. Since the destination interface's upstream credit C <= N,
    //! only C packets are received.
    //!
    void test_torus_flow_control_1()
    {
        MasterClock.unregisterAll();
        
        //the paramters for torus network
        torus_init_params rp;
        rp.x_dim = random() % 19 + 2; //between 2 and 20
        rp.y_dim = random() % 19 + 2; //between 2 and 20
        //rp.no_vcs = random() % 6 + 2; //between 2 and 7
        rp.no_vcs = 4; //must be 4.
        rp.credits = random() % 5 + 3; //between 3 and 7
        rp.link_width = 128;
	rp.ni_up_credits = random() % 90 + 10; //between 10 and 99.
	rp.ni_upstream_buffer_size = 5;

	const int N = rp.x_dim * rp.y_dim;
        
        Clock& clk = MasterClock;
        
        //creat the ring network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();
	vector<int> node_lps(N);
	for(int i=0; i<N; i++)
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

	//randomly select a source and a destination terminal
	int SRC = random() % N;
	int DST;
	while((DST = random() % N) == SRC);

	const int DELTA = 20;
	const int Npkts = random() % 20 + DELTA + rp.ni_up_credits; //number of pkts > upstream_credits
	                                                            // Npkts - upstream_credits >= DELTA.
//cout << "Npkts= " << Npkts << "\n";
//cout << "up credits= " << rp.ni_up_credits << "\n";
//cout << "SRC= " << SRC << " DST= " << DST << "\n";
        //schedule events
	Manifold::unhalt();
	Ticks_t When = 1;

        vector<Ticks_t>  scheduleAt;

        //source sends Npkts packets to destination
	for(int i=0; i<Npkts; i++) {
	    TerminalData* data = new TerminalData;
	    data->src = SRC;
	    data->dst = DST;

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[SRC], data);
	    //scheduleAt.push_back(When + MasterClock.NowTicks());
	    When += 1;
	}

        //number of credits to send back: at least 2
	int Ncredits = random() % (Npkts - rp.ni_up_credits);
	if(Ncredits < 2)
	    Ncredits = 2;


	When += 500; //wait until enough packets have been delivered to destination terminal.
	             //If test case files, first try making this bigger.
	for(int i=0; i<Ncredits; i++) {
	    TerminalData* credit = new TerminalData;
	    credit->set_type(CREDIT_PKT);

	    Manifold::Schedule(When, &MockTerminal::send_data, terms[DST], credit);
	    When += 1;
	}
	Manifold::StopAt(When + 500); //If test case files, first try making this bigger.
	Manifold::Run();


	//verify the number of packets received at destination.
	vector<TerminalData>& received_data = terms[DST]->get_data();

	CPPUNIT_ASSERT_EQUAL((int)rp.ni_up_credits + Ncredits, (int)received_data.size());

	//verify the number of credits received at the source
	vector<TerminalData>& src_recved_credits = terms[SRC]->get_data();

	//Due to backpressure, the credits received by source terminal may not equal the number
	//of packets it sent; so comment out the next line.
	//CPPUNIT_ASSERT_EQUAL(Npkts, (int)src_recved_credits.size());


	delete mapping;
	delete slen;
	delete myNetwork;

    }





    
    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("topoCreatorTest");
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("test_ring_flow_control_0", &topoCreatorCompTest::test_ring_flow_control_0));
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("test_ring_flow_control_1", &topoCreatorCompTest::test_ring_flow_control_1));
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("test_torus_flow_control_0", &topoCreatorCompTest::test_torus_flow_control_0));
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("test_torus_flow_control_1", &topoCreatorCompTest::test_torus_flow_control_1));
	/*
	*/
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


