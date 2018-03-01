/*
 * =====================================================================================
 *    Description:  Back pressure tests for iris network
 *
 *        Version:  1.0
 *        Created:  09/06/2012
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
#include "../../data_types/linkData.h"
#include "../../data_types/flit.h"
#include "../../components/genericBuffer.h"
#include "../../components/simpleRouter.h"
#include "../../interfaces/genericHeader.h"
#include "../../genericTopology/genericTopoCreator.h"
#include "../../genericTopology/ring.h"
#include "../../genericTopology/torus.h"
#include "genericIrisInterface.h"
#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;

using namespace manifold::iris;

//####################################################################
// helper classes and structures
//####################################################################

const int CREDIT_INT = 4;

struct terminal_param{
    uint id;
    int e_id;
    int dst_id;
    int num_pkt_can_send;
};

class TerminalData {
public:
    static const int SIZE = 8;
    int type;
    uint src; //must have this
    uint dest_id;
    int dst_port;
    int src_port;
    unsigned data[SIZE];  //do not use containers

    void set_dst_src (uint src_add, uint dst_add)
    {
        src = src_add;
        dest_id = dst_add;
	
	for (int i = 0; i < SIZE; i++)
	  data[i] = i;
    }
    
    void set_type (int type_o)
    {
      type = type_o;
    }
    
    void set_dst_port (int dst_port_o)
    {
      dst_port = dst_port_o;
    }
    
    void set_src_port (int src_port_o)
    {
      src_port = src_port_o;
    }

    uint get_src() { return src; } //must have
    uint get_dst() { return dest_id; }
    int get_src_port() {return src_port; }
    int get_type() {return type; }
    int get_simulated_len() { return sizeof(TerminalData); }
};

const int PKTS_PER_DST = 11; //no. of packets to send to each destination

class MySimLen : public SimulatedLen<TerminalData> {
public:
    int get_simulated_len(TerminalData* d) { return d->get_simulated_len(); }
};

class MyVnet : public VnetAssign<TerminalData> {
public:
    int get_virtual_net(TerminalData*) { return 0; }
};

//this class emulate the packet src
class Mockterminal : public manifold::kernel::Component {
public:
    enum {PKTOUT};
    enum {PKTIN};

    unsigned pkt_count;
    unsigned id;
    int enable_id;
    int dst_id;
    int num_pkt;
    list<TerminalData*> local_td;

    Mockterminal(terminal_param* tp)
    {
        id = tp->id;
        pkt_count = 0;
        enable_id = tp->e_id;
	dst_id = tp->dst_id;
	num_pkt = tp->num_pkt_can_send;
    }

    void tick ()
    {
       if ((pkt_count < num_pkt) && (id == enable_id) && (dst_id != id))
       {
          TerminalData* td = new TerminalData();
          td->set_dst_src(id, dst_id);

	  Send(PKTOUT , td);

          pkt_count++;
       }
    }

    void tock()
    {}

    void rec_pkt(int port, TerminalData* td)
    {
	if (td->type != CREDIT_INT)
	    local_td.push_back (td);
        else
	    delete td;
    }
};

//####################################################################
//! Class backPressureTest_v3 is back pressure test class
//####################################################################
class backPressureTest_v3 : public CppUnit::TestFixture {
private:

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 1000};

    //======================================================================
    //======================================================================
    //! @brief verify the back pressure behavior of ring netwrok
    //!
    //! Create a torus; For each pair of nodes i, j, i!=j, send packets from i to j; Each sender
    //! sends N = ni_upstream_credits + ni_upstream_buffer_size + 2 packets. The (N-1)th
    //! packet would be in interface j's proc_in_buffer, and the Nth would be inside the
    //! routers.
    //! In credits > no. of flits in a packet, then all the flits of the Nth packet are in
    //! router j.
    void torus_BP_test_0()
        {
	    int x = 4;
	    int y = 4;
	    int num_nodes = x*y;

	    //the paramters for torus network
	    torus_init_params rp;
	    rp.x_dim = x;
	    rp.y_dim = y;
	    rp.no_vcs = 4;
	    rp.link_width = 128;
	    
	    rp.ni_up_credits = 4;
	    rp.ni_upstream_buffer_size = 6;
	    
	    //calculate how many flits in one packet
	    unsigned pktLen=0;
	    {
		TerminalData temp;
		unsigned num_bytes = HeadFlit :: HEAD_FLIT_OVERHEAD + temp.get_simulated_len();
		unsigned num_flits = num_bytes * 8 / rp.link_width;
		if(num_bytes * 8 % rp.link_width != 0)
		    num_flits++;

		pktLen = num_flits;
		if(num_flits > 1)
		    pktLen++; //If data doesn't fit in headflit, we also need tail flit.
	    }
	    const unsigned PKT_LEN = pktLen;

	    rp.credits = PKT_LEN + 1;

	    
	    //test all src-dest pairs
	    for (int src = 0; src < num_nodes; src++)
	    {
		for (int dst = 0; dst < num_nodes; dst++)
		{
		    if (src == dst)
		        continue;
		    
		    MasterClock.unregisterAll();
		    Manifold::CreateScheduler(Manifold::TICKED);

		    
		    Clock& clk = MasterClock;
		    
		    //creat the torus network
		    Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
		    SimulatedLen<TerminalData>* slen = new MySimLen;
		    MyVnet* vnet = new MyVnet();
		    
		    vector<int> node_lps(rp.x_dim * rp.y_dim);
		    for(int i=0; i<rp.x_dim * rp.y_dim; i++)
		        node_lps[i] = 0;
		    Torus<TerminalData>* topo = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, slen, vnet, CREDIT_INT, &node_lps);
		    vector <CompId_t> mt_ids(2);
		    vector <Mockterminal*> mt(2);
		    const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();

		    //creat 2 terminals: src and dest
		    for (int i = 0; i < 2; i ++)
		    {
			terminal_param t_p;
			t_p.num_pkt_can_send = (rp.ni_up_credits + rp.ni_upstream_buffer_size + 2);
			t_p.e_id = src;
			t_p.dst_id = dst;
			t_p.id = ((i==0) ? src : dst);
		      
			mt_ids[i] = Component :: Create<Mockterminal> (0, &t_p);
			mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
			if (mt[i])
			    Clock::Register(clk, mt[i], &Mockterminal::tick, &Mockterminal::tock);
		    }

		    //connect terminal and interface together
		    for (int i = 0; i < 2; i ++)
		    {
		        int ni_idx = ((i==0) ? src : dst);
			Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(ni_idx),
				      GenNetworkInterface<TerminalData>::TERMINAL_PORT,
				      &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
			Manifold::Connect(inf_ids.at(ni_idx), GenNetworkInterface<TerminalData>::TERMINAL_PORT, mt_ids[i],
				      Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);
		    }
		    
		    //check to see if the buffer is empty in interface
		    for (int i = 0; i < inf_ids.size(); i ++)
		    {
			GenNetworkInterface<TerminalData>* Gnt_inf;
			Gnt_inf = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[i]);
			if (Gnt_inf->output_pkt_buffer.size() > 0)
			  CPPUNIT_ASSERT_EQUAL(int (Gnt_inf->output_pkt_buffer.size()), 0);
		    }
		    
		    //check to see if the buffer is empty in router
		    for (int i = 0; i < topo->router_ids.size(); i++)
		    {
			SimpleRouter* Sr;
			Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
			for (int j = 0; j < Sr->in_buffers.size(); j++)
			{
			    for (int z = 0; z < rp.no_vcs; z++)
			      CPPUNIT_ASSERT_EQUAL(int (Sr->in_buffers[j]->get_occupancy(z)), int(0));
			}
		    }

		    Manifold::unhalt();
		    Manifold::StopAt(500);
		    Manifold::Run();
		    
		    //check the buffer after run

		    //! 1. dst terminal got ni_up_credits packets.
		    {
			Mockterminal* termb = Component :: GetComponent< Mockterminal >(mt_ids[1]);
			CPPUNIT_ASSERT_EQUAL(int(rp.ni_up_credits), int (termb->local_td.size()));
		    }

		    
		    //! 2. interface dst's output_pkt_buffer has ni_upstream_buffer_size packets.
		    //! 3. interface dst's proc_in_buffer has the last packet.
		    //! 4. interface dst's router_in_buffer is empty.
		    {
			GenNetworkInterface<TerminalData>* nib = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[dst]);
			CPPUNIT_ASSERT_EQUAL(int(rp.ni_upstream_buffer_size), int (nib->output_pkt_buffer.size()));

			int np=0; //number of non-empty proc_in_buffers
			for (int j = 0; j < rp.no_vcs; j++) {
			    if(nib->proc_in_buffer[j].size() != 0) {
				np++;
			    }
			    CPPUNIT_ASSERT_EQUAL(0, int (nib->router_in_buffer.get_occupancy(j)));
			}
			CPPUNIT_ASSERT_EQUAL(1, np); //only one VC has flits
		    }


		    
		    
		    //5. since number of flits in a packet < credits, router dst has all the flits of the last packet.
		    //the other routers have no flits.
		    int num_vc_with_pending_flits = 0;
		    
		    for (int i = 0; i < topo->router_ids.size(); i++)
		    {
			SimpleRouter* Sr;
			Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
			for (int j = 0; j < Sr->in_buffers.size(); j++)
			{
			  for (int z = 0; z < rp.no_vcs; z++)
			  {
			    if (Sr->in_buffers[j]->get_occupancy(z) > 0)
			    {
			      num_vc_with_pending_flits ++;
			      CPPUNIT_ASSERT_EQUAL(int(PKT_LEN), int (Sr->in_buffers[j]->get_occupancy(z)));
			    }
			  }
			}
		    }
		    
		    CPPUNIT_ASSERT_EQUAL(1, int (num_vc_with_pending_flits));
		    delete topo;
		}
	    }
        }
        
    //======================================================================
    //======================================================================
    //! @brief verify the back pressure behavior of torus netwrok
    //!
    //! Create a torus; For each pair of nodes i, j, i!=j, send packets from i to j; Each sender
    //! sends N = ni_upstream_credits + ni_upstream_buffer_size + 2 packets. The (N-1)th
    //! packet would be in interface j's proc_in_buffer, and the Nth would be inside the
    //! routers.
    //! In credits < no. of flits in a packet, then the flits of the Nth packet are scattered
    //! over multiple routers.
    void torus_BP_test_1()
        {
	    int x = 4;
	    int y = 4;
	    int num_nodes = x*y;

	    //the paramters for torus network
	    torus_init_params rp;
	    rp.x_dim = x;
	    rp.y_dim = y;
	    rp.no_vcs = 4;
	    rp.link_width = 128;
	    
	    //calculate how many flits in one packet
	    unsigned pktLen=0;
	    {
		TerminalData temp;
		unsigned num_bytes = HeadFlit :: HEAD_FLIT_OVERHEAD + temp.get_simulated_len();
		unsigned num_flits = num_bytes * 8 / rp.link_width;
		if(num_bytes * 8 % rp.link_width != 0)
		    num_flits++;

		pktLen = num_flits;
		if(num_flits > 1)
		    pktLen++; //If data doesn't fit in headflit, we also need tail flit.
	    }

	    const unsigned PKT_LEN = pktLen;

	    
	    rp.credits = random() % (PKT_LEN-1) + 1; //credits between [1, pkt_len-1]
	    rp.ni_up_credits = random() % 4 + 2; // [2, 5]
	    rp.ni_upstream_buffer_size = 6;

	    
	    for (int src = 0; src < num_nodes; src++)
	    {
		for (int dst = 0; dst < num_nodes; dst++)
		{
		    if (src == dst)
		      continue;
		    
		    MasterClock.unregisterAll();
		    Manifold::CreateScheduler(Manifold::TICKED);

		    Clock& clk = MasterClock;
		    
		    //creat the torus network
		    Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
		    SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
		    MyVnet* vnet = new MyVnet();
		    
		    vector<int> node_lps(rp.x_dim * rp.y_dim);
		    for(int i=0; i<rp.x_dim * rp.y_dim; i++)
		        node_lps[i] = 0;
		    Torus<TerminalData>* topo = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, slen, vnet, CREDIT_INT, &node_lps);
		    vector <CompId_t> mt_ids(2);
		    vector <Mockterminal*> mt(2);
		    const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();

		    //creat 2 terminals
		    for (int i = 0; i < 2; i ++)
		    {
			terminal_param t_p;
			t_p.num_pkt_can_send = (rp.ni_up_credits + rp.ni_upstream_buffer_size + 2);
			t_p.e_id = src;
			t_p.dst_id = dst;
			t_p.id = ((i==0) ? src : dst);
		      
			mt_ids[i] = Component :: Create<Mockterminal> (0, &t_p);
			mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
			if (mt[i])
			    Clock::Register(clk, mt[i], &Mockterminal::tick, &Mockterminal::tock);
		    }

		    //connect terminal and interface together
		    for (int i = 0; i < 2; i ++)
		    {
			int ni_idx = ((i==0) ? src : dst);
			Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(ni_idx),
				      GenNetworkInterface<TerminalData>::TERMINAL_PORT,
				      &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
			Manifold::Connect(inf_ids.at(ni_idx), GenNetworkInterface<TerminalData>::TERMINAL_PORT, mt_ids[i],
				      Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);
		    }
		    
		    //check to see if the buffer is empty in interface
		    for (int i = 0; i < inf_ids.size(); i ++)
		    {
			GenNetworkInterface<TerminalData>* Gnt_inf;
			Gnt_inf = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[i]);
			if (Gnt_inf->output_pkt_buffer.size() > 0)
			  CPPUNIT_ASSERT_EQUAL(int (Gnt_inf->output_pkt_buffer.size()), 0);
		    }
		    
		    //check to see if the buffer is empty in router
		    for (int i = 0; i < topo->router_ids.size(); i++)
		    {
			SimpleRouter* Sr;
			Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
			for (int j = 0; j < Sr->in_buffers.size(); j++)
			{
			    for (int z = 0; z < rp.no_vcs; z++)
			    CPPUNIT_ASSERT_EQUAL(int (Sr->in_buffers[j]->get_occupancy(z)), int(0));
			}
		    }

		    Manifold::unhalt();
		    Manifold::StopAt(500);
		    Manifold::Run();
		    
		    //check the buffer after run

		    //! 1. terminal dst got ni_up_credits packets.
		    {
			Mockterminal* termb = Component :: GetComponent< Mockterminal >(mt_ids[1]);
			CPPUNIT_ASSERT_EQUAL(int(rp.ni_up_credits), int (termb->local_td.size()));
		    }

		    
		    //! 2. interface dst's output_pkt_buffer has ni_upstream_buffer_size packets.
		    //! 3. interface dst's proc_in_buffer has the last packet.
		    //! 4. interface dst's router_in_buffer is empty.
		    {
			GenNetworkInterface<TerminalData>* nib = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[dst]);
			CPPUNIT_ASSERT_EQUAL(int(rp.ni_upstream_buffer_size), int (nib->output_pkt_buffer.size()));

			int np=0; //number of non-empty proc_in_buffers
			for (int j = 0; j < rp.no_vcs; j++) {
			    if(nib->proc_in_buffer[j].size() != 0) {
				np++;
			    }
			    CPPUNIT_ASSERT_EQUAL(0, int (nib->router_in_buffer.get_occupancy(j)));
			}
			CPPUNIT_ASSERT_EQUAL(1, np); //only one VC has flits
		    }


		    
		    //5. since number of flits in a packet > credits, flits of last packet scattered over 2 routers.
		    int num_vc_with_pending_flits = 0;
		    int cumu_flits_in_network = 0;
		    
		    for (int i = 0; i < topo->router_ids.size(); i++)
		    {
			SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
			for (int j = 0; j < Sr->in_buffers.size(); j++)
			{
			  for (int z = 0; z < rp.no_vcs; z++)
			  {
			    if (Sr->in_buffers[j]->get_occupancy(z) > 0)
			    {
			      num_vc_with_pending_flits ++;
			      cumu_flits_in_network += int (Sr->in_buffers[j]->get_occupancy(z));
			    }
			  }
			}
		    }

		    GenNetworkInterface<TerminalData>* src_inf = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[src]);
		    for(int i=0; i<rp.no_vcs; i++) {
		        cumu_flits_in_network += src_inf->router_out_buffer.get_occupancy(i);
		    }

		    CPPUNIT_ASSERT(num_vc_with_pending_flits > 1);
		    CPPUNIT_ASSERT_EQUAL(int(PKT_LEN), cumu_flits_in_network);
		    delete topo;
		}
	    }
        }
        
    //! Build a test suite.
    static CppUnit::Test* suite()
    {
    	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("backPressureTest_v3");
        mySuite->addTest(new CppUnit::TestCaller<backPressureTest_v3>("torus_BP_test_0",
	                                                              &backPressureTest_v3::torus_BP_test_0));
	mySuite->addTest(new CppUnit::TestCaller<backPressureTest_v3>("torus_BP_test_1",
	                                                              &backPressureTest_v3::torus_BP_test_1));

	return mySuite;
    }
};



Clock backPressureTest_v3 :: MasterClock(backPressureTest_v3 :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( backPressureTest_v3::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}



