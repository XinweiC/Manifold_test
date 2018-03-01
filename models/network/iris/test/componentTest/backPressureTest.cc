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

const int CREDIT_INT = 4; //msg type for credit to terminals

struct terminal_param{
    uint id;
    int e_id;
    int dst_id;
    int num_pkt_can_send;
};

class TerminalData {
public:
    static const int SIZE = 8;
    static int SIM_LEN;
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

    uint get_src() { return src; }
    uint get_dst() { return dest_id; }
    int get_src_port() {return src_port; }
    int get_type() {return type; }
    //int get_simulated_len() { return sizeof(TerminalData); }
    int get_simulated_len()
    {
        if(SIM_LEN <= 0)
	    return sizeof(TerminalData);
	else
	    return SIM_LEN;
    }
};

int TerminalData :: SIM_LEN = -1;


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
       if ((pkt_count < num_pkt) && (id == enable_id))
       {
          TerminalData* td = new TerminalData();
          td->set_dst_src(id, dst_id);

          if (td->dest_id != id)
	  {
              Send(PKTOUT , td);
	  }
	  else
	      delete td;

          pkt_count++;
       }
    }

    void tock()
    {
        return;
    }

    void rec_pkt(int port, TerminalData* td)
    {
	if (td->type != CREDIT_INT)
	  local_td.push_back (td);
	
	TerminalData* td_out = new TerminalData();
	td_out->set_type(CREDIT_INT);
	//Send(PKTOUT , td);
	delete td_out;
    }
};

//####################################################################
//####################################################################
class backPressureTest : public CppUnit::TestFixture {
private:

    //calculate number of flits required for a packet; copied from genericIrisInterface.h
    int get_packet_flits(int lk_width)
    {
        TerminalData temp;
	unsigned num_bytes = HeadFlit :: HEAD_FLIT_OVERHEAD + temp.get_simulated_len();
	unsigned num_flits = num_bytes * 8 / lk_width;
	if(num_bytes * 8 % lk_width != 0)
	    num_flits++;

	unsigned tot_flits = num_flits;
	if(num_flits > 1)
	    tot_flits++; //If data doesn't fit in headflit, we also need tail flit.

	    return tot_flits;
    }

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 1000};

    //======================================================================
    //======================================================================
    //! @brief verify the back pressure behavior of ring network
    //!
    //! Create a ring. Send N (N == ni_up_credits + ni_upstream_buffer_size) packets
    //! from terminal 0 to terminal 1. Terminal 1 never sends credit to the interface.
    //! Verify:
    //! 1. terminal 1 got ni_up_credits packets.
    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
    //! 3. interface 1's proc_in_buffer is empty.
    //! 4. interface 1's router_in_buffer is empty.
    //! 5. router 1's in_buffer is empty.
    //! 6. router 0's in_buffer is empty.
    //! 7. interface 0's router_out_buffer is empty.
    //! 8. interface 0's proc_out_buffer is empty.
    //! 9. interface 0's input_pkt_buffer is empty.
    void basic_BP_test_0()
        {
            MasterClock.unregisterAll();
	    Manifold::CreateScheduler(Manifold::TICKED);

            //the paramters for ring network
            ring_init_params rp;
            rp.no_nodes = 2;
            rp.no_vcs = 4;
            rp.credits = random() % 5 + 2; //between 2 and 6
	    rp.ni_up_credits = random() % 4 + 3; //between 3 and 6
            rp.rc_method = RING_ROUTING;
            rp.link_width = 128;
	    rp.ni_upstream_buffer_size = random()%7 + 2; //between 2 and 8

//cout << "credits= " << rp.credits << " ni credits= " << rp.ni_up_credits << " ni up buffer= " << rp.ni_upstream_buffer_size << endl;
            Clock& clk = MasterClock;
	    
            //creat the ring network
            Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	    SimulatedLen<TerminalData>* slen = new MySimLen;
	    MyVnet* vnet = new MyVnet();
	    
            Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, slen, vnet, CREDIT_INT, 0, 0);
            const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
            vector <CompId_t> mt_ids(inf_ids.size());
            vector <Mockterminal*> mt(inf_ids.size());

            //creat the terminals
            for (int i = 0; i < inf_ids.size(); i ++)
            {
		terminal_param t_p;
		t_p.num_pkt_can_send = (rp.ni_up_credits + rp.ni_upstream_buffer_size); //packets to send = ni_up_credits + ni_upstream_buffer_size
		t_p.e_id = 0; //terminal 0 is sender
		t_p.dst_id = 1; //sends to terminal 1
		t_p.id = i;
	      
                mt_ids[i] = Component :: Create<Mockterminal> (0, &t_p);
                mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
                if (mt[i])
                    Clock::Register(clk, mt[i], &Mockterminal::tick, &Mockterminal::tock);
            }

            //connect terminal and interface together
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(i),
                              GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                              &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
                Manifold::Connect(inf_ids.at(i), GenNetworkInterface<TerminalData>::TERMINAL_PORT, mt_ids[i],
                              Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);
            }
            
            //check to see if the buffer is empty in interface
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                GenNetworkInterface<TerminalData>* Gnt_inf =
		          Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[i]);
		if(Gnt_inf)
		    CPPUNIT_ASSERT_EQUAL(0, int (Gnt_inf->output_pkt_buffer.size()));
            }
            
            //check to see if the buffer is empty in router
            for (int i = 0; i < topo->router_ids.size(); i++)
	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
		for (int j = 0; j < Sr->in_buffers.size(); j++)
		{
		    for (int z = 0; z < rp.no_vcs; z++)
			CPPUNIT_ASSERT_EQUAL(0, int (Sr->in_buffers[j]->get_occupancy(z)));
		}
	    }

            Manifold::unhalt();
            Manifold::StopAt(1000);
            Manifold::Run();
	    

	    //check the backpressure after run()

	    //! 1. terminal 1 got ni_up_credits packets.
            {
	        Mockterminal* term1 = Component :: GetComponent< Mockterminal >(mt_ids[1]);
		CPPUNIT_ASSERT_EQUAL(int(rp.ni_up_credits), int (term1->local_td.size()));
	    }

	    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
	    //! 3. interface 1's proc_in_buffer is empty.
	    //! 4. interface 1's router_in_buffer is empty.
            {
                GenNetworkInterface<TerminalData>* ni1 = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[1]);
		CPPUNIT_ASSERT_EQUAL(int(rp.ni_upstream_buffer_size), int (ni1->output_pkt_buffer.size()));
		for (int j = 0; j < rp.no_vcs; j++)
		{
		    CPPUNIT_ASSERT_EQUAL(0, int (ni1->proc_in_buffer[j].size()));
		    CPPUNIT_ASSERT_EQUAL(0, int (ni1->router_in_buffer.get_occupancy(j)));
		}
	    }
	    
	    //! 5. router 1's in_buffer is empty.
	    //! 6. router 0's in_buffer is empty.
	    for (int i = 0; i < 1; i++)
	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
		for (int j = 0; j < Sr->in_buffers.size(); j++)
		{
		  for (int z = 0; z < rp.no_vcs; z++)
		  {
		      CPPUNIT_ASSERT_EQUAL(0, int (Sr->in_buffers[j]->get_occupancy(z)));
		  }
		}
	    }

	    //! 7. interface 0's router_out_buffer is empty.
	    //! 8. interface 0's proc_out_buffer is empty.
	    //! 9. interface 0's input_pkt_buffer is empty.
            {
                GenNetworkInterface<TerminalData>* ni0 = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[0]);
            
		for (int j = 0; j < rp.no_vcs; j++)
		{
		    CPPUNIT_ASSERT_EQUAL(0, int (ni0->router_out_buffer.get_occupancy(j)));
		    CPPUNIT_ASSERT_EQUAL(0, int (ni0->proc_out_buffer[j].size()));
		}
		CPPUNIT_ASSERT_EQUAL(0, int (ni0->input_pkt_buffer.size()));
	    }
	    
	    delete topo;
        }
        

    //======================================================================
    //======================================================================
    //! @brief verify the back pressure behavior of ring network
    //!
    //! Create a ring. Send N (N == ni_up_credits + ni_upstream_buffer_size + 1) packets
    //! from terminal 0 to terminal 1. Terminal 1 never sends credit to the interface.
    //! The last packet will be in terminal 1's proc_in_buffer.
    //! Verify:
    //! 1. terminal 1 got ni_up_credits packets.
    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
    //! 3*. interface 1's proc_in_buffer has the last packet.
    //! 4. interface 1's router_in_buffer is empty.
    //! 5. router 1's in_buffer is empty.
    //! 6. router 0's in_buffer is empty.
    //! 7. interface 0's router_out_buffer is empty.
    //! 8. interface 0's proc_out_buffer is empty.
    //! 9. interface 0's input_pkt_buffer is empty.
    //! * has flits
    void basic_BP_test_1()
        {
            MasterClock.unregisterAll();
	    Manifold::CreateScheduler(Manifold::TICKED);

            //the paramters for ring network
            ring_init_params rp;
            rp.no_nodes = 2;
            rp.no_vcs = 4;
            rp.credits = random() % 5 + 2; //between 2 and 6
	    rp.ni_up_credits = random() % 4 + 3; //between 3 and 6
            rp.rc_method = RING_ROUTING;
            rp.link_width = 128;
	    rp.ni_upstream_buffer_size = random()%7 + 2; //between 2 and 8

//cout << "credits= " << rp.credits << " ni credits= " << rp.ni_up_credits << " ni up buffer= " << rp.ni_upstream_buffer_size << endl;
            Clock& clk = MasterClock;
	    
            //creat the ring network
            Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	    SimulatedLen<TerminalData>* slen = new MySimLen;
	    MyVnet* vnet = new MyVnet();
	    
            Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, slen, vnet, CREDIT_INT, 0, 0);
            const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
            vector <CompId_t> mt_ids(inf_ids.size());
            vector <Mockterminal*> mt(inf_ids.size());

            //creat the terminals
            for (int i = 0; i < inf_ids.size(); i ++)
            {
		terminal_param t_p;
		t_p.num_pkt_can_send = (rp.ni_up_credits + rp.ni_upstream_buffer_size + 1); //packets to send = ni_up_credits + ni_upstream_buffer_size + 1
		t_p.e_id = 0; //terminal 0 is sender
		t_p.dst_id = 1; //sends to terminal 1
		t_p.id = i;
	      
                mt_ids[i] = Component :: Create<Mockterminal> (0, &t_p);
                mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
                if (mt[i])
                    Clock::Register(clk, mt[i], &Mockterminal::tick, &Mockterminal::tock);
            }

            //connect terminal and interface together
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(i),
                              GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                              &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
                Manifold::Connect(inf_ids.at(i), GenNetworkInterface<TerminalData>::TERMINAL_PORT, mt_ids[i],
                              Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);
            }
            
            //check to see if the buffer is empty in interface
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                GenNetworkInterface<TerminalData>* Gnt_inf =
		          Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[i]);
		if(Gnt_inf)
		    CPPUNIT_ASSERT_EQUAL(0, int (Gnt_inf->output_pkt_buffer.size()));
            }
            
            //check to see if the buffer is empty in router
            for (int i = 0; i < topo->router_ids.size(); i++)
	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
		for (int j = 0; j < Sr->in_buffers.size(); j++)
		{
		    for (int z = 0; z < rp.no_vcs; z++)
			CPPUNIT_ASSERT_EQUAL(0, int (Sr->in_buffers[j]->get_occupancy(z)));
		}
	    }

            Manifold::unhalt();
            Manifold::StopAt(1000);
            Manifold::Run();
	    

	    //check the backpressure after run()

	    //! 1. terminal 1 got ni_up_credits packets.
            {
	        Mockterminal* term1 = Component :: GetComponent< Mockterminal >(mt_ids[1]);
		CPPUNIT_ASSERT_EQUAL(int(rp.ni_up_credits), int (term1->local_td.size()));
	    }

	    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
	    //! 3. interface 1's proc_in_buffer has the last packet.
	    //! 4. interface 1's router_in_buffer is empty.
            {
                GenNetworkInterface<TerminalData>* ni1 = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[1]);
		CPPUNIT_ASSERT_EQUAL(int(rp.ni_upstream_buffer_size), int (ni1->output_pkt_buffer.size()));

		int n=0; //number of none-empty proc_in_buffers
		for (int j = 0; j < rp.no_vcs; j++)
		{
		    if(ni1->proc_in_buffer[j].size() != 0) {
			n++;
		    }
		    CPPUNIT_ASSERT_EQUAL(0, int (ni1->router_in_buffer.get_occupancy(j)));
		}
		CPPUNIT_ASSERT_EQUAL(1, n); //only one VC has flits
	    }
	    
	    //! 5. router 1's in_buffer is empty.
	    //! 6. router 0's in_buffer is empty.
	    for (int i = 0; i < 1; i++)
	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
		for (int j = 0; j < Sr->in_buffers.size(); j++) //for each port
		{
		  for (int z = 0; z < rp.no_vcs; z++)
		  {
		      CPPUNIT_ASSERT_EQUAL(0, int (Sr->in_buffers[j]->get_occupancy(z)));
		  }
		}
	    }

	    //! 7. interface 0's router_out_buffer is empty.
	    //! 8. interface 0's proc_out_buffer is empty.
	    //! 9. interface 0's input_pkt_buffer is empty.
            {
                GenNetworkInterface<TerminalData>* ni0 = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[0]);
            
		for (int j = 0; j < rp.no_vcs; j++)
		{
		    CPPUNIT_ASSERT_EQUAL(int (ni0->router_out_buffer.get_occupancy(j)),int(0));
		    CPPUNIT_ASSERT_EQUAL(int (ni0->proc_out_buffer[j].size()),int(0));
		}
		CPPUNIT_ASSERT_EQUAL(int (ni0->input_pkt_buffer.size()),int(0));
	    }
	    
	    delete topo;
        }
        

    //======================================================================
    //======================================================================
    //! @brief verify the back pressure behavior of ring network
    //!
    //! Create a ring. Send N (N == ni_up_credits + ni_upstream_buffer_size + 2) packets
    //! from terminal 0 to terminal 1. Terminal 1 never sends credit to the interface.
    //! The last packet will be in router 1's in_buffer, and the 2nd last in
    //! proc_in_buffer.
    //! Verify:
    //! 1. terminal 1 got ni_up_credits packets.
    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
    //! 3*. interface 1's proc_in_buffer has the (N-1)th packet.
    //! 4. interface 1's router_in_buffer is empty.
    //! 5*. router 1's in_buffer has flits.
    //! 6*. router 0's in_buffer has flits IF nf > credits, nf is the number of flits of a packet..
    //! 7*. interface 0's router_out_buffer has flits IF nf > 2*credits.
    //! 8. interface 0's proc_out_buffer is empty.
    //! 9. interface 0's input_pkt_buffer is empty.
    //! * has flits
    void basic_BP_test_2()
        {
            //the paramters for ring network
            ring_init_params rp;
            rp.no_nodes = 2;
            rp.no_vcs = 4;
            rp.credits = random() % 5 + 2; //between 2 and 6
	    rp.ni_up_credits = random() % 4 + 3; //between 3 and 6
            rp.rc_method = RING_ROUTING;
            rp.link_width = 128;
	    rp.ni_upstream_buffer_size = random()%7 + 2; //between 2 and 8

	    basic_BP_test_2_common(rp);
        }

    //======================================================================
    //======================================================================
    //! @brief verify the back pressure behavior of ring network
    //!
    //! Same as BT_test_2, except we set the simulated length to a value such that the
    //! number of flits of a packet is > 2*credits. Then then last packet would stretch
    //! over router 1, router 0, and interface 0's router_out_buffer.
    //!
    //! Verify:
    //! 1. terminal 1 got ni_up_credits packets.
    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
    //! 3*. interface 1's proc_in_buffer has the (N-1)th packet.
    //! 4. interface 1's router_in_buffer is empty.
    //! 5*. router 1's in_buffer has flits.
    //! 6*. router 0's in_buffer has flits.
    //! 7*. interface 0's router_out_buffer has flits.
    //! 8. interface 0's proc_out_buffer is empty.
    //! 9. interface 0's input_pkt_buffer is empty.
    //! * has flits
    void basic_BP_test_2_1()
        {
            //the paramters for ring network
            ring_init_params rp;
            rp.no_nodes = 2;
            rp.no_vcs = 4;
            rp.credits = random() % 5 + 2; //between 2 and 6
	    rp.ni_up_credits = random() % 4 + 3; //between 3 and 6
            rp.rc_method = RING_ROUTING;
            rp.link_width = 128;
	    rp.ni_upstream_buffer_size = random()%7 + 2; //between 2 and 8

	    TerminalData :: SIM_LEN = (2*rp.credits) * (rp.link_width/8);
            //the number of flits would be SIM_LEN * 8 /rp.link_width + 2; so it must be > 2*credits.

	    basic_BP_test_2_common(rp);

	    TerminalData :: SIM_LEN = -1;
	}
        

    void basic_BP_test_2_common(ring_init_params& rp)
        {
            MasterClock.unregisterAll();
	    Manifold::CreateScheduler(Manifold::TICKED);

//cout << "credits= " << rp.credits << " ni credits= " << rp.ni_up_credits << " ni up buffer= " << rp.ni_upstream_buffer_size << endl;
            Clock& clk = MasterClock;
	    
            //creat the ring network
            Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	    SimulatedLen<TerminalData>* slen = new MySimLen;
	    MyVnet* vnet = new MyVnet();
	    
            Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, slen, vnet, CREDIT_INT, 0, 0);
            const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
            vector <CompId_t> mt_ids(inf_ids.size());
            vector <Mockterminal*> mt(inf_ids.size());

            //creat the terminals
            for (int i = 0; i < inf_ids.size(); i ++)
            {
		terminal_param t_p;
		t_p.num_pkt_can_send = (rp.ni_up_credits + rp.ni_upstream_buffer_size + 2); //packets to send = ni_up_credits + ni_upstream_buffer_size + 2
		t_p.e_id = 0; //terminal 0 is sender
		t_p.dst_id = 1; //sends to terminal 1
		t_p.id = i;
	      
                mt_ids[i] = Component :: Create<Mockterminal> (0, &t_p);
                mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
                if (mt[i])
                    Clock::Register(clk, mt[i], &Mockterminal::tick, &Mockterminal::tock);
            }

            //connect terminal and interface together
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(i),
                              GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                              &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
                Manifold::Connect(inf_ids.at(i), GenNetworkInterface<TerminalData>::TERMINAL_PORT, mt_ids[i],
                              Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);
            }
            
            //check to see if the buffer is empty in interface
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                GenNetworkInterface<TerminalData>* Gnt_inf =
		          Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[i]);
		if(Gnt_inf)
		    CPPUNIT_ASSERT_EQUAL(0, int (Gnt_inf->output_pkt_buffer.size()));
            }
            
            //check to see if the buffer is empty in router
            for (int i = 0; i < topo->router_ids.size(); i++)
	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
		for (int j = 0; j < Sr->in_buffers.size(); j++)
		{
		    for (int z = 0; z < rp.no_vcs; z++)
			CPPUNIT_ASSERT_EQUAL(0, int (Sr->in_buffers[j]->get_occupancy(z)));
		}
	    }

            Manifold::unhalt();
            Manifold::StopAt(1000);
            Manifold::Run();
	    

	    //check the backpressure after run()

	    //! 1. terminal 1 got ni_up_credits packets.
            {
	        Mockterminal* term1 = Component :: GetComponent< Mockterminal >(mt_ids[1]);
		CPPUNIT_ASSERT_EQUAL(int(rp.ni_up_credits), int (term1->local_td.size()));
	    }

	    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
	    //! 3. interface 1's proc_in_buffer has the last packet.
	    //! 4. interface 1's router_in_buffer is empty.
            {
                GenNetworkInterface<TerminalData>* ni1 = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[1]);
		CPPUNIT_ASSERT_EQUAL(int(rp.ni_upstream_buffer_size), int (ni1->output_pkt_buffer.size()));

		int np=0; //number of non-empty proc_in_buffers
		for (int j = 0; j < rp.no_vcs; j++)
		{
#ifdef DBG
cout << "NI 1 proc_in_buffer[" << j << "]: " << ni1->proc_in_buffer[j].size() << endl;
#endif
		    if(ni1->proc_in_buffer[j].size() != 0) {
			np++;
		    }
		    CPPUNIT_ASSERT_EQUAL(0, int (ni1->router_in_buffer.get_occupancy(j)));
		}
		CPPUNIT_ASSERT_EQUAL(1, np); //only one VC has flits
	    }
	    
	    //! 5. router 1's in_buffer has flits.
	    const int N_flits = get_packet_flits(rp.link_width); //no. of flits in the last packet

	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[1]);
		int n=0; //non-empty buffers
		for (int j = 0; j < Sr->in_buffers.size(); j++) //for each port
		{
		    for (int z = 0; z < rp.no_vcs; z++)
		    {
#ifdef DBG
cout << "router 1 in_buffer " << j << " vc " << z << ": " << Sr->in_buffers[j]->get_occupancy(z) << endl;
#endif
		        if(Sr->in_buffers[j]->get_occupancy(z)) {
			    int val = rp.credits;
			    if(val > N_flits)
			        val = N_flits;
		            CPPUNIT_ASSERT_EQUAL(val, (int)Sr->in_buffers[j]->get_occupancy(z));
		            n++;
			}
		    }
#ifdef DBG
cout << "\n";
#endif
		}
		CPPUNIT_ASSERT_EQUAL(1, n);
	    }

	    //! 6. router 0's in_buffer is empty.
	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[0]);
		int n=0;
		for (int j = 0; j < Sr->in_buffers.size(); j++) //for each port
		{
		    for (int z = 0; z < rp.no_vcs; z++)
		    {
#ifdef DBG
cout << "router 0 in_buffer " << j << " vc " << z << ": " << Sr->in_buffers[j]->get_occupancy(z) << endl;
#endif
		        if(0 != int(Sr->in_buffers[j]->get_occupancy(z)))
			    n++;
		    }
#ifdef DBG
cout << "\n";
#endif
		}
//cout << "N_flits= " << N_flits << endl;
		if(N_flits > rp.credits)
		    CPPUNIT_ASSERT_EQUAL(1, n); //the last packet is stretched over router 1 and 0.
		else
		    CPPUNIT_ASSERT_EQUAL(0, n);
	    }

	    //! 7. interface 0's router_out_buffer: empty if N_flits <= 2*credits
	    //! 8. interface 0's proc_out_buffer is empty.
	    //! 9. interface 0's input_pkt_buffer is empty.
            {
                GenNetworkInterface<TerminalData>* ni0 = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[0]);
            
		int n=0;
		for (int j = 0; j < rp.no_vcs; j++)
		{
#ifdef DBG
cout << "NI 0 router_out_buffer " << j << ": " << ni0->router_out_buffer.get_occupancy(j) << endl;
#endif
		    if(ni0->router_out_buffer.get_occupancy(j) != 0)
		        n++;
		    CPPUNIT_ASSERT_EQUAL(0, int (ni0->proc_out_buffer[j].size()));
		}
		if(N_flits > 2*rp.credits)
		    CPPUNIT_ASSERT_EQUAL(1, n); //the last packet is stretched over router 1 and 0.
		else
		    CPPUNIT_ASSERT_EQUAL(0, n);

		CPPUNIT_ASSERT_EQUAL(int (ni0->input_pkt_buffer.size()),int(0));
	    }
	    
	    delete topo;
        }
        


    //======================================================================
    //======================================================================
    //! @brief verify the back pressure behavior of ring network
    //!
    //! Create a ring. Send N (N == ni_up_credits + ni_upstream_buffer_size + 3) packets
    //! from terminal 0 to terminal 1. Terminal 1 never sends credit to the interface.
    //! The (N-2)th packet is in interface 1's proc_in_buffer; the (N-1)th gets as far as
    //! router 1; the Nth gets as far as router 0.
    //!
    //! Verify:
    //! 1. terminal 1 got ni_up_credits packets.
    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
    //! 3*. interface 1's proc_in_buffer has the (N-2)th packet.
    //! 4. interface 1's router_in_buffer is empty.
    //! 5*. router 1's in_buffer has flits ((N-1)th packet) ######## This is key! router 1 should only have flits belonging
    //!     to the (N-1)th packet ###########
    //! 6*. router 0's in_buffer has flits: Nth, possibly (N-1)th as well.
    //! 7*. interface 0's router_out_buffer has flits IF nf > 2*credits.
    //! 8. interface 0's proc_out_buffer is empty.
    //! 9. interface 0's input_pkt_buffer is empty.
    //! * has flits
    void basic_BP_test_3()
        {
            //the paramters for ring network
            ring_init_params rp;
            rp.no_nodes = 2;
            rp.no_vcs = 4;
            //rp.credits = random() % 5 + 2; //between 2 and 6
            rp.credits = 2;
	    rp.ni_up_credits = random() % 4 + 3; //between 3 and 6
            rp.rc_method = RING_ROUTING;
            rp.link_width = 128;
	    rp.ni_upstream_buffer_size = random()%7 + 2; //between 2 and 8

            MasterClock.unregisterAll();
	    Manifold::CreateScheduler(Manifold::TICKED);

//cout << "credits= " << rp.credits << " ni credits= " << rp.ni_up_credits << " ni up buffer= " << rp.ni_upstream_buffer_size << endl;
            Clock& clk = MasterClock;
	    
            //creat the ring network
            Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	    SimulatedLen<TerminalData>* slen = new MySimLen;
	    MyVnet* vnet = new MyVnet();
	    
            Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, slen, vnet, CREDIT_INT, 0, 0);
            const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
            vector <CompId_t> mt_ids(inf_ids.size());
            vector <Mockterminal*> mt(inf_ids.size());

            //creat the terminals
            for (int i = 0; i < inf_ids.size(); i ++)
            {
		terminal_param t_p;
		t_p.num_pkt_can_send = (rp.ni_up_credits + rp.ni_upstream_buffer_size + 3); //packets to send = ni_up_credits + ni_upstream_buffer_size + 3
		t_p.e_id = 0; //terminal 0 is sender
		t_p.dst_id = 1; //sends to terminal 1
		t_p.id = i;
	      
                mt_ids[i] = Component :: Create<Mockterminal> (0, &t_p);
                mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
                if (mt[i])
                    Clock::Register(clk, mt[i], &Mockterminal::tick, &Mockterminal::tock);
            }

            //connect terminal and interface together
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(i),
                              GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                              &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
                Manifold::Connect(inf_ids.at(i), GenNetworkInterface<TerminalData>::TERMINAL_PORT, mt_ids[i],
                              Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);
            }
            
            //check to see if the buffer is empty in interface
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                GenNetworkInterface<TerminalData>* Gnt_inf =
		          Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[i]);
		if(Gnt_inf)
		    CPPUNIT_ASSERT_EQUAL(0, int (Gnt_inf->output_pkt_buffer.size()));
            }
            
            //check to see if the buffer is empty in router
            for (int i = 0; i < topo->router_ids.size(); i++)
	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[i]);
		for (int j = 0; j < Sr->in_buffers.size(); j++)
		{
		    for (int z = 0; z < rp.no_vcs; z++)
			CPPUNIT_ASSERT_EQUAL(0, int (Sr->in_buffers[j]->get_occupancy(z)));
		}
	    }

            Manifold::unhalt();
            Manifold::StopAt(1000);
            Manifold::Run();
	    

	    //check the backpressure after run()

	    //! 1. terminal 1 got ni_up_credits packets.
            {
	        Mockterminal* term1 = Component :: GetComponent< Mockterminal >(mt_ids[1]);
		CPPUNIT_ASSERT_EQUAL(int(rp.ni_up_credits), int (term1->local_td.size()));
	    }

	    //! 2. interface 1's output_pkt_buffer has ni_upstream_buffer_size packets.
	    //! 3. interface 1's proc_in_buffer has the last packet.
	    //! 4. interface 1's router_in_buffer is empty.
            {
                GenNetworkInterface<TerminalData>* ni1 = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[1]);
		CPPUNIT_ASSERT_EQUAL(int(rp.ni_upstream_buffer_size), int (ni1->output_pkt_buffer.size()));

		int np=0; //number of non-empty proc_in_buffers
		for (int j = 0; j < rp.no_vcs; j++)
		{
#ifdef DBG
cout << "NI 1 proc_in_buffer[" << j << "]: " << ni1->proc_in_buffer[j].size() << endl;
#endif
		    if(ni1->proc_in_buffer[j].size() != 0) {
			np++;
		    }
		    CPPUNIT_ASSERT_EQUAL(0, int (ni1->router_in_buffer.get_occupancy(j)));
		}
		CPPUNIT_ASSERT_EQUAL(1, np); //only one VC has flits
	    }
	    
	    //! 5. router 1's in_buffer has flits: only (N-1)th packet's flits
	    const int N_flits = get_packet_flits(rp.link_width); //no. of flits in the last packet

	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[1]);
		int n=0; //non-empty buffers
		for (int j = 0; j < Sr->in_buffers.size(); j++) //for each port
		{
		    for (int z = 0; z < rp.no_vcs; z++)
		    {
#ifdef DBG
cout << "router 1 in_buffer " << j << " vc " << z << ": " << Sr->in_buffers[j]->get_occupancy(z) << endl;
#endif
		        if(Sr->in_buffers[j]->get_occupancy(z)) {
			    int val = rp.credits;
			    if(val > N_flits)
			        val = N_flits;
		            CPPUNIT_ASSERT_EQUAL(val, (int)Sr->in_buffers[j]->get_occupancy(z));
		            n++;
			}
		    }
cout << "\n";
		}
		CPPUNIT_ASSERT_EQUAL(1, n); //############ this is key! ###########
	    }

	    //! 6. router 0's in_buffer is not empty.
	    {
		SimpleRouter* Sr = Component :: GetComponent< SimpleRouter >(topo->router_ids[0]);
		int n=0;
		for (int j = 0; j < Sr->in_buffers.size(); j++) //for each port
		{
		    for (int z = 0; z < rp.no_vcs; z++)
		    {
#ifdef DBG
cout << "router 0 in_buffer " << j << " vc " << z << ": " << Sr->in_buffers[j]->get_occupancy(z) << endl;
#endif
		        if(0 != int(Sr->in_buffers[j]->get_occupancy(z)))
			    n++;
		    }
cout << "\n";
		}

#ifdef DBG
		for(int p=0; p<Sr->downstream_credits.size(); p++) {
		    for(int v=0; v<Sr->downstream_credits[0].size(); v++) {
		    cout << " router 0  port " << p << " vc " << v << " dstream credits= " << Sr->downstream_credits[p][v] << endl;
		    }
		}
#endif
//cout << "N_flits= " << N_flits << endl;
		    CPPUNIT_ASSERT(n>0); //the last packet has reached router 0; possibly some flits of (N-1)th packet too.
	    }

	    //! 7. interface 0's router_out_buffer: empty if N_flits <= 2*credits
	    //! 8. interface 0's proc_out_buffer is empty.
	    //! 9. interface 0's input_pkt_buffer is empty.
            {
                GenNetworkInterface<TerminalData>* ni0 = Component :: GetComponent< GenNetworkInterface<TerminalData> >(inf_ids[0]);
            
		int n=0;
		for (int j = 0; j < rp.no_vcs; j++)
		{
#ifdef DBG
cout << "NI 0 router_out_buffer " << j << ": " << ni0->router_out_buffer.get_occupancy(j) << endl;
#endif
		    if(ni0->router_out_buffer.get_occupancy(j) != 0)
		        n++;
		    CPPUNIT_ASSERT_EQUAL(0, int (ni0->proc_out_buffer[j].size()));
		}

#ifdef DBG
		for (int j = 0; j < rp.no_vcs; j++) {
cout << "NI 0 proc_out_buffer " << j << ": " << ni0->proc_out_buffer[j].size() << endl;
		}
		for (int j = 0; j < rp.no_vcs; j++) {
cout << "NI 0 downstream credits " << j << ": " << ni0->downstream_credits[j] << endl;
		}
#endif
		if(N_flits > rp.credits)
		    CPPUNIT_ASSERT(n > 0); //the last packet has some flits in the interface

		CPPUNIT_ASSERT_EQUAL(0, int (ni0->input_pkt_buffer.size()));
	    }
	    
	    delete topo;
        }
        
        
    //! Build a test suite.
    static CppUnit::Test* suite()
    {
    	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("backPressureTest");
        mySuite->addTest(new CppUnit::TestCaller<backPressureTest>("basic_BP_test_0",
	                                                              &backPressureTest::basic_BP_test_0));
	mySuite->addTest(new CppUnit::TestCaller<backPressureTest>("basic_BP_test_1",
	                                                              &backPressureTest::basic_BP_test_1));
	mySuite->addTest(new CppUnit::TestCaller<backPressureTest>("basic_BP_test_2",
	                                                              &backPressureTest::basic_BP_test_2));
	mySuite->addTest(new CppUnit::TestCaller<backPressureTest>("basic_BP_test_2_1",
	                                                              &backPressureTest::basic_BP_test_2_1));
	mySuite->addTest(new CppUnit::TestCaller<backPressureTest>("basic_BP_test_3",
	                                                              &backPressureTest::basic_BP_test_3));
	return mySuite;
    }
};



Clock backPressureTest :: MasterClock(backPressureTest :: MASTER_CLOCK_HZ);

int main()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    srandom(t.tv_usec);

    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( backPressureTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}



