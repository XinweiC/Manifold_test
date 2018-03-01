/*
 * =====================================================================================
 *    Description:  component tests for generic iris interface
 *
 *        Version:  1.0
 *        Created:  08/29/2011 
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
#include "../../interfaces/genericHeader.h"
#include "../../genericTopology/genericTopoCreator.h"
#include "../../genericTopology/ring.h"
#include "../../genericTopology/torus.h"
#include "../../genericTopology/CrossBar.h"
#include "genericIrisInterface.h"
#include "genOneVcIrisInterface.h"
#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;

using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################

#define CREDIT_PKT 123 // msg type for credit packets
#define MSG_PKT 456

class TerminalData {
public:
    int type;
    uint src;
    uint dst;
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    void set_dst_src (uint src_add, uint dst_add)
    {
        src = src_add;
        dst = dst_add;
    }
    uint get_src() { return src; }
    unsigned get_src_port() { return 0; }
    uint get_dst() { return dst; }
    uint get_dst_port() { return 0; }
    void set_dst_port(int p) {}
};


//this class emulate the packet src
class Mockterminal : public manifold::kernel::Component {
public:
    enum {PKTOUT};
    enum {PKTIN};
    
    Mockterminal(uint no_nodes, uint node_id, int e_id) :
        id(node_id), num_nodes(no_nodes)
    {   
        tot_pkt = no_nodes - 1;
        pkt_count = 0;
        enable_id = e_id;
        out_time.resize(no_nodes*10);
        
        for (int i =0; i < no_nodes*10; i++)
            out_time[i] = 0;
    }
    
    void tick ()
    {  
       if (pkt_count <= tot_pkt && ((id == enable_id) || (enable_id == -1)))
       {  
          TerminalData* td = new TerminalData();
	  td->type = MSG_PKT;
          td->set_dst_src(id, (pkt_count % num_nodes));
          
          if (td->dst != id)
          {
              out_time[pkt_count] = manifold::kernel::Manifold::NowTicks();
              Send(PKTOUT , td);
          } 
          
          pkt_count++;   
       } 
       
       return; 
    }
    
    void tock()
    {   
        return;
    }
    
    void rec_pkt(int port, TerminalData* td)
    {
        in_time.push_back (manifold::kernel::Manifold::NowTicks());
        recv_pkts.push_back (td);
    }

    unsigned get_id() { return id; }

    list<TerminalData*>& get_recv_pkts() { return recv_pkts; }
    list<int>& get_in_time() { return in_time; }
    vector<int>& get_out_time() { return out_time; }

private:

    const unsigned id;
    const unsigned num_nodes;

    uint pkt_count;
    unsigned tot_pkt;
    int enable_id;
    list<TerminalData*> recv_pkts; //received packets
    vector<int> out_time; //time of the pkt leaving terminal
    list<int> in_time; //time of the pkt arriving terminal

};


//####################################################################
//! Class NetworkInterfaceTest is the test class for class NetworkInterface. 
//####################################################################
class topoCreatorCompTest : public CppUnit::TestFixture {
private:

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    //======================================================================
    //======================================================================
    //! @brief verify the behavior of ring netwrok
    //!
    //! see if the packet has been correctly forwarded through the network 
    void ring_component_test_0()
    {
        MasterClock.unregisterAll();
        
        //the paramters for ring network
        ring_init_params rp;
        rp.no_nodes = 4;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.rc_method = RING_ROUTING;
        rp.link_width = 128;
	rp.ni_up_credits = 10000;
	rp.ni_upstream_buffer_size = 5;
        
        Clock& clk = MasterClock;
        
        //creat the ring network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

        Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, slen, vnet, CREDIT_PKT, 0, 0);
	vector <CompId_t> term_ids(rp.no_nodes);        
        vector <Mockterminal*> terms(rp.no_nodes);
        const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
        
        //creat the terminals
        for (int i = 0; i < inf_ids.size(); i ++)
        {
            term_ids[i] = Component :: Create<Mockterminal> (0, rp.no_nodes, i, -1);
            terms[i] = Component :: GetComponent<Mockterminal>(term_ids[i]);
            if (terms[i])
                Clock::Register(MasterClock, terms[i], &Mockterminal::tick, &Mockterminal::tock);
        }
        
        //connect terminal and interface together
        for (int i = 0; i < inf_ids.size(); i ++)
        {
            Manifold::Connect(term_ids[i], Mockterminal::PKTOUT, inf_ids[i], 
                          GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                          &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
            Manifold::Connect(inf_ids[i], GenNetworkInterface<TerminalData>::TERMINAL_PORT, term_ids[i], 
                          Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);               
        }
        
        Manifold::unhalt();
        Manifold::StopAt(1000);
        Manifold::Run();
        
        //verify
	for (int i = 0; i < inf_ids.size(); i ++) {
	    list<TerminalData*>& pkts = terms[i]->get_recv_pkts();
	    int msgs = 0;
	    int credits = 0;
	    for(list<TerminalData*>::iterator it = pkts.begin(); it != pkts.end(); ++it) {
	        if((*it)->type == MSG_PKT)
		    msgs++;
	        else if((*it)->type == CREDIT_PKT)
		    credits++;
		else {
		    assert(0);
		}

	    }
            CPPUNIT_ASSERT_EQUAL(int(rp.no_nodes -1), msgs);
            CPPUNIT_ASSERT_EQUAL(int(rp.no_nodes -1), credits);
	}
         
        for (int i = 0; i < inf_ids.size(); i++)  
        {   
	    list<TerminalData*>& pkts = terms[i]->get_recv_pkts();
	    for(list<TerminalData*>::iterator it = pkts.begin(); it != pkts.end(); ++it) {
	        if((*it)->get_type() == MSG_PKT)
		    CPPUNIT_ASSERT_EQUAL(terms[i]->get_id(), (*it)->dst);
	    }
        }          
    }


    //======================================================================
    //======================================================================
    //! @brief verify the behavior of ring netwrok
    //!
    //! see if the packet has correct time stamp
    void ring_component_test_1()
    {
        MasterClock.unregisterAll();
        
        //the paramters for ring network
        ring_init_params rp;
        rp.no_nodes = 4;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.rc_method = RING_ROUTING;
        rp.link_width = 128;
	rp.ni_up_credits = 1000000;
	rp.ni_upstream_buffer_size = 5;
        
        //calculate the total length
        uint no_bf = uint ((sizeof(TerminalData)*8)/rp.link_width);
        double compare =double (sizeof(TerminalData)*8)/ rp.link_width;
    
        if (compare > no_bf)
           no_bf ++;
           
        uint total = no_bf + 2;
        
        //local clock
        Clock& clk = MasterClock;
        
        //each time only one terminal is sending data to every other terminal
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

        for (int z = 0; z < rp.no_nodes; z++)
        {
            Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, slen, vnet, CREDIT_PKT, 0, 0);
	    vector <CompId_t> mt_ids;        
            vector <Mockterminal*> mt;
            const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
            mt_ids.resize(inf_ids.size());
       	    mt.resize(inf_ids.size());
        
            //creat the terminals
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                 mt_ids[i] = Component :: Create<Mockterminal> (0, rp.no_nodes, i, z);
                 mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
                 if (mt[i])
                     Clock::Register(MasterClock, mt[i], &Mockterminal::tick, &Mockterminal::tock);
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
        
            Manifold::unhalt();
            Manifold::StopAt(1000);
            Manifold::Run();
        
            //see if the ticket is right
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                if (i != z)
             	    CPPUNIT_ASSERT_EQUAL(int(mt[i]->get_recv_pkts().size()), 1); 
            }
            
            //see if the latency is correct
            for (int i = 0; i < inf_ids.size(); i++)  
            {   
            	if (i != z)
            	{
                    CPPUNIT_ASSERT_EQUAL(uint(mt[i]->get_recv_pkts().front()->dst), mt[i]->get_id());
                    CPPUNIT_ASSERT((mt[i]->get_in_time().front() - mt[z]->get_out_time()[i]) >= total);
                    CPPUNIT_ASSERT((mt[i]->get_in_time().front() - mt[z]->get_out_time()[i]) <= (total*(rp.no_nodes*2) + 4));
                    mt[i]->get_recv_pkts().pop_front();
            	} 
            }
            
            MasterClock.unregisterAll();
            delete topo;
        }
    }

    //======================================================================
    //======================================================================
    //! @brief verify the behavior of torus netwrok
    //!
    //! see if the packet has been correctly forwarded through the network   
    void torus_component_test_0()
    {
        MasterClock.unregisterAll();
    
        //the paramters for torus network
        torus_init_params rp;
        rp.x_dim = 4;
        rp.y_dim = 4;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.link_width = 128;
	rp.ni_up_credits = 10000;
	rp.ni_upstream_buffer_size = 5;
        
        Clock& clk = MasterClock;
        
        //creat torus network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

	vector<int> node_lps(rp.x_dim*rp.y_dim);
	for(int i=0; i<rp.x_dim*rp.y_dim; i++)
	    node_lps[i] = 0;
        Torus<TerminalData>* topo = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, slen, vnet, CREDIT_PKT, &node_lps);
	vector <CompId_t> mt_ids;        
        vector <Mockterminal*> terms;
        const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
        mt_ids.resize(inf_ids.size());
        terms.resize(inf_ids.size());
        
        //creat the terminals
        for (int i = 0; i < inf_ids.size(); i ++)
        {
            mt_ids[i] = Component :: Create<Mockterminal> (0, rp.x_dim*rp.y_dim, i, -1);
            terms[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
            if (terms[i])
                Clock::Register(MasterClock, terms[i], &Mockterminal::tick, &Mockterminal::tock);
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
        
        Manifold::unhalt();
        Manifold::StopAt(1000);
        Manifold::Run();
        
        //verify
	for (int i = 0; i < inf_ids.size(); i ++) {
	    list<TerminalData*>& pkts = terms[i]->get_recv_pkts();
	    int msgs = 0;
	    int credits = 0;
	    for(list<TerminalData*>::iterator it = pkts.begin(); it != pkts.end(); ++it) {
	        if((*it)->type == MSG_PKT)
		    msgs++;
	        else if((*it)->type == CREDIT_PKT)
		    credits++;
		else {
		    assert(0);
		}

	    }
            CPPUNIT_ASSERT_EQUAL(int(rp.x_dim * rp.y_dim -1), msgs);
            CPPUNIT_ASSERT_EQUAL(int(rp.x_dim * rp.y_dim -1), credits);
	}
         
        for (int i = 0; i < inf_ids.size(); i++)  
        {   
	    list<TerminalData*>& pkts = terms[i]->get_recv_pkts();
	    for(list<TerminalData*>::iterator it = pkts.begin(); it != pkts.end(); ++it) {
	        if((*it)->get_type() == MSG_PKT)
		    CPPUNIT_ASSERT_EQUAL(terms[i]->get_id(), (*it)->dst);
	    }
        }          
    }
 



    //======================================================================
    //======================================================================
    //! @brief verify the behavior of torus netwrok; x dimension not equal to y dimension
    //!
    //! see if the packet has been correctly forwarded through the network   
    void torus_component_test_1()
    {
        MasterClock.unregisterAll();
    
        //the paramters for torus network
        torus_init_params rp;
        rp.x_dim = 4;
        rp.y_dim = 5;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.link_width = 128;
	rp.ni_up_credits = 1000000;
	rp.ni_upstream_buffer_size = 5;
        
        Clock& clk = MasterClock;
        
        //creat torus network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

	vector<int> node_lps(rp.x_dim*rp.y_dim);
	for(int i=0; i<rp.x_dim*rp.y_dim; i++)
	    node_lps[i] = 0;
        Torus<TerminalData>* topo = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, slen, vnet, CREDIT_PKT, &node_lps);
	vector <CompId_t> mt_ids;        
        vector <Mockterminal*> mt;
        const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
        mt_ids.resize(inf_ids.size());
        mt.resize(inf_ids.size());
        
        //creat the terminals
        for (int i = 0; i < inf_ids.size(); i ++)
        {
            mt_ids[i] = Component :: Create<Mockterminal> (0, rp.x_dim*rp.y_dim, i, -1);
            mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
            if (mt[i])
                Clock::Register(MasterClock, mt[i], &Mockterminal::tick, &Mockterminal::tock);
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
        
        Manifold::unhalt();
        Manifold::StopAt(1000);
        Manifold::Run();
        
        //see if the ticket is right
        for (int i = 0; i < inf_ids.size(); i ++)
             CPPUNIT_ASSERT_EQUAL(uint(mt[i]->get_recv_pkts().size()), 2*(rp.x_dim*rp.y_dim - 1)); //for every pkt sent, there's a credit 
         
    }
 
 


    //======================================================================
    //======================================================================
    //! @brief verify the behavior of torus netwrok
    //!
    //! see if the packet has correct time stamp
    void torus_component_test_2()
    {
        MasterClock.unregisterAll();
        
        //the paramters for torus network
        torus_init_params rp;
        rp.x_dim = 4;
        rp.y_dim = 4;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.link_width = 128;
	rp.ni_up_credits = 1000000;
	rp.ni_upstream_buffer_size = 5;

        
	//calculate the total flits in a packet
        unsigned num_bytes = sizeof(TerminalData) + HeadFlit::HEAD_FLIT_OVERHEAD;
	unsigned num_flits = num_bytes*8/rp.link_width;
	if(num_bytes*8 % rp.link_width != 0)
	    num_flits++;
           
        unsigned total = num_flits;
	if(num_flits > 1)
	    total++; //if body flit is needed, then tail is also needed.

        
        //local clock
        Clock& clk = MasterClock;
        
        //each time only one terminal is sending data to every other terminal
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

        for (int z = 0; z < rp.x_dim*rp.y_dim; z++)
        {
	    vector<int> node_lps(rp.x_dim*rp.y_dim);
	    for(int i=0; i<rp.x_dim*rp.y_dim; i++)
		node_lps[i] = 0;
            Torus<TerminalData>* topo = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, slen, vnet, CREDIT_PKT, &node_lps);
	    vector <CompId_t> mt_ids;        
            vector <Mockterminal*> mt;
            const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
            mt_ids.resize(inf_ids.size());
       	    mt.resize(inf_ids.size());
        
            //creat the terminals
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                 mt_ids[i] = Component :: Create<Mockterminal> (0, rp.x_dim*rp.y_dim, i, z);
                 mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
                 if (mt[i])
                     Clock::Register(MasterClock, mt[i], &Mockterminal::tick, &Mockterminal::tock);
            }
        
            //connect terminal and interface together
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                //cout<<"mt id: "<<mt_ids[i]<<endl;
          	Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(i), 
                  	         GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                       	   	 &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
            	Manifold::Connect(inf_ids.at(i), GenNetworkInterface<TerminalData>::TERMINAL_PORT, mt_ids[i], 
                          	 Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);               
            }
        
            Manifold::unhalt();
            Manifold::StopAt(1000);
            Manifold::Run();
        
            //see if the ticket is right
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                if (i != z)
             	    CPPUNIT_ASSERT_EQUAL(int(mt[i]->get_recv_pkts().size()), 1); 
            }
            
            //see if the latency is correct
            for (int i = 0; i < inf_ids.size(); i++)  
            {   
            	if (i != z)
            	{
                    CPPUNIT_ASSERT_EQUAL(uint(mt[i]->get_recv_pkts().front()->dst), mt[i]->get_id());
                    CPPUNIT_ASSERT((mt[i]->get_in_time().front() - mt[z]->get_out_time()[i]) >= total);
                    CPPUNIT_ASSERT((mt[i]->get_in_time().front() - mt[z]->get_out_time()[i])  <= ((total*rp.x_dim*rp.y_dim*2) +4));
                    mt[i]->get_recv_pkts().pop_front();
            	} 
            }
            
            MasterClock.unregisterAll();
        }
    }

#if 0
    //======================================================================
    //======================================================================
    //! @brief verify the behavior of CrossBar netwrok
    //!
    //! see if the packet has been correctly forwarded through the network 
    void CrossBar_component_test_0()
    {
        MasterClock.unregisterAll();
        
        //the paramters for ring network
        CrossBar_init_params cb;
        cb.no_nodes = 4;
        cb.credits = 3;
        cb.link_width = 128;
        
        Clock& clk = MasterClock;
        
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
        CrossBar<TerminalData>* topo = topoCreator<TerminalData>::create_CrossBar(clk, &cb, mapping, slen, 0, 0);
	vector <CompId_t> mt_ids;        
        vector <Mockterminal*> mt;
        const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
        mt_ids.resize(inf_ids.size());
        mt.resize(inf_ids.size());
        
        //creat the terminals
        for (int i = 0; i < inf_ids.size(); i ++)
        {
            mt_ids[i] = Component :: Create<Mockterminal> (0, cb.no_nodes, i, -1);
            mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
            if (mt[i])
                Clock::Register(MasterClock, mt[i], &Mockterminal::tick, &Mockterminal::tock);
        }
        
        //connect terminal and interface together
        for (int i = 0; i < inf_ids.size(); i ++)
        {
            Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(i), 
                          GenOneVcIrisInterface<TerminalData>::TERMINAL_PORT,
                          &GenOneVcIrisInterface<TerminalData>::handle_new_packet_event,1);
            Manifold::Connect(inf_ids.at(i), GenOneVcIrisInterface<TerminalData>::TERMINAL_PORT, mt_ids[i], 
                          Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);               
        }
        
        Manifold::unhalt();
        Manifold::StopAt(1000);
        Manifold::Run();
        
        //see if the ticket is right
        for (int i = 0; i < inf_ids.size(); i ++)
             CPPUNIT_ASSERT_EQUAL(uint(mt[i]->local_td.size()), cb.no_nodes - 1); 
         
        for (int i = 0; i < inf_ids.size(); i++)  
        {   
            for (int j = 0; j < (inf_ids.size() - 1); j++)
            {
                 CPPUNIT_ASSERT_EQUAL(uint(mt[i]->local_td.front()->dest_id), mt[i]->id);
                 mt[i]->local_td.pop_front();
            } 
        }        
    }
     

    //======================================================================
    //======================================================================
    //! @brief verify the behavior of CrossBar netwrok
    //!
    //! see if the packet has correct time stamp
    void CrossBar_component_test_1()
    {
        MasterClock.unregisterAll();
        
        //the paramters for CrossBar network
        CrossBar_init_params cb;
        cb.no_nodes = 4;
        cb.credits = 3;
        cb.link_width = 128;
        
        //calculate the total length
        uint no_bf = uint ((sizeof(TerminalData)*8)/cb.link_width);
        double compare =double (sizeof(TerminalData)*8)/ cb.link_width;
    
        if (compare > no_bf)
           no_bf ++;
           
        uint total = no_bf + 2;
        
        //local clock
        Clock& clk = MasterClock;
        
        //each time only one terminal is sending data to every other terminal
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
        for (int z = 0; z < cb.no_nodes; z++)
        {
            CrossBar<TerminalData>* topo = topoCreator<TerminalData>::create_CrossBar(clk, &cb, mapping, slen, 0, 0);
	    vector <CompId_t> mt_ids;        
            vector <Mockterminal*> mt;
            const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
            mt_ids.resize(inf_ids.size());
       	    mt.resize(inf_ids.size());
        
            //creat the terminals
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                 mt_ids[i] = Component :: Create<Mockterminal> (0, cb.no_nodes, i, z);
                 mt[i] = Component :: GetComponent<Mockterminal>(mt_ids[i]);
                 if (mt[i])
                     Clock::Register(MasterClock, mt[i], &Mockterminal::tick, &Mockterminal::tock);
            }
        
            //connect terminal and interface together
            for (int i = 0; i < inf_ids.size(); i ++)
            {
          	Manifold::Connect(mt_ids[i], Mockterminal::PKTOUT, inf_ids.at(i), 
                  	         GenOneVcIrisInterface<TerminalData>::TERMINAL_PORT,
                       	   	 &GenOneVcIrisInterface<TerminalData>::handle_new_packet_event,1);
            	Manifold::Connect(inf_ids.at(i), GenOneVcIrisInterface<TerminalData>::TERMINAL_PORT, mt_ids[i], 
                          	 Mockterminal::PKTIN, &Mockterminal::rec_pkt,1);               
            }
        
            Manifold::unhalt();
            Manifold::StopAt(1000);
            Manifold::Run();
        
            //see if the ticket is right
            for (int i = 0; i < inf_ids.size(); i ++)
            {
                if (i != z)
             	    CPPUNIT_ASSERT_EQUAL(int(mt[i]->local_td.size()), 1); 
            }
            
            //see if the latency is correct
            for (int i = 0; i < inf_ids.size(); i++)  
            {   
            	if (i != z)
            	{
                    CPPUNIT_ASSERT_EQUAL(uint(mt[i]->local_td.front()->dest_id), mt[i]->id);
                    CPPUNIT_ASSERT((mt[i]->in_time.front() - mt[z]->out_time[i]) >= total);
                    CPPUNIT_ASSERT((mt[i]->in_time.front() - mt[z]->out_time[i]) <= (total*cb.no_nodes));
                    mt[i]->local_td.pop_front();
            	} 
            }
            
            MasterClock.unregisterAll();
            delete topo;
        }
    }

#endif


    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("topoCreatorTest");
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("ring_component_test_0",
	                                                               &topoCreatorCompTest::ring_component_test_0));
	mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("ring_component_test_1",
	                                                               &topoCreatorCompTest::ring_component_test_1));
	mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("torus_component_test_0",
	                                                               &topoCreatorCompTest::torus_component_test_0));
	mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("torus_component_test_1",
	                                                               &topoCreatorCompTest::torus_component_test_1));
	mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("torus_component_test_2",
	                                                               &topoCreatorCompTest::torus_component_test_2));
								       /*
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("CrossBar_component_test_0",
	                                                               &topoCreatorCompTest::CrossBar_component_test_0));
	mySuite->addTest(new CppUnit::TestCaller<topoCreatorCompTest>("CrossBar_component_test_1",
	                                                               &topoCreatorCompTest::CrossBar_component_test_1));
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


