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
#include "genericIrisInterface.h"
#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;

using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################

#define CREDIT_TYPE 123


class TerminalData {
public:
    static const int SIZE = 8;
    int type;
    uint src;
    uint dest_id;
    unsigned data[SIZE];  //do not use containers
    
    void set_dst_src (uint src_add, uint dst_add)
    {
        src = src_add;
        dest_id = dst_add;
        
        for (int i = 0; i < SIZE; i++)
           data[i] = i;
    }
    
    int get_type() { return type; }
    void set_type(int t) { type = t; }

    uint get_src() { return src; }
    unsigned get_src_port() { return 0; }
    uint get_dst() { return dest_id; }
    uint get_dst_port() { return 0; }
    void set_dst_port(int p) {}
};


const int PKTS_PER_DST = 1000; //no. of packets to send to each destination

//this class emulate the packet src
class Mockterminal : public manifold::kernel::Component {
public:
    enum {PKTOUT};
    enum {PKTIN};

    Mockterminal(uint n_nodes, uint node_id, int e_id) :
          id(node_id), enable_id(e_id), no_nodes(n_nodes)
    {   
        pkt_count = 0;
    }
    
    void tick ()
    {  
       if ((pkt_count < no_nodes*PKTS_PER_DST) && ((id == enable_id) || (enable_id == -1)))
       {  
          TerminalData* td = new TerminalData();
          td->set_dst_src(id, (pkt_count % no_nodes));
          
          if (td->dest_id != id)
              Send(PKTOUT , td);
	  else
	      delete td;
          
          pkt_count++;   
       } 
    }
    
    void tock()
    {}
    
    void rec_pkt(int port, TerminalData* td)
    {
	if(td->get_type() == CREDIT_TYPE)
	    credit_pkts.push_back (td);
	else
	    local_td.push_back (td);
    }    

    unsigned get_id() { return id; }

    list<TerminalData*>& get_recv_pkts() { return local_td; }
    list<TerminalData*>& get_recv_credits() { return credit_pkts; }

private:
    const unsigned id;
    const int enable_id;
    const int no_nodes; //num of nodes

    uint pkt_count; // num of pkts that have been sent out
    list<TerminalData*> local_td;
    list<TerminalData*> credit_pkts; //every time a packet is sent and consumed by the NI, a credit is sent back to the terminal.

};


//####################################################################
//! Class NetworkInterfaceTest is the test class for class NetworkInterface. 
//####################################################################
class topoCreatorMassTest : public CppUnit::TestFixture {
private:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 1000};

public:

    //======================================================================
    //======================================================================
    //! @brief verify the behavior of ring netwrok
    //!
    //! Each node sends PKTS_PER_DST packets to each one of the other nodes.
    void ring_mass_test_0()
    {
        MasterClock.unregisterAll();
        
        //the paramters for ring network
        ring_init_params rp;
        rp.no_nodes = 4;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.rc_method = RING_ROUTING;
        rp.link_width = 128;
	rp.ni_up_credits = rp.no_nodes * PKTS_PER_DST;
	rp.ni_upstream_buffer_size = 5;
        
        Clock& clk = MasterClock;
        
        //creat the ring network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

        Ring<TerminalData>* topo = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, slen, vnet, CREDIT_TYPE, 0, 0);
	vector <CompId_t> mt_ids;        
        vector <Mockterminal*> mt;
        const vector <manifold::kernel::CompId_t>& inf_ids = topo->get_interface_id();
        mt_ids.resize(inf_ids.size());
        mt.resize(inf_ids.size());
        
        //creat the terminals
        for (int i = 0; i < inf_ids.size(); i ++)
        {
            mt_ids[i] = Component :: Create<Mockterminal> (0, rp.no_nodes, i, -1);
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
        
        Manifold::unhalt();
        Manifold::StopAt(100000);
        Manifold::Run();
        
        //verify
        for (unsigned i = 0; i < inf_ids.size(); i ++) {
             CPPUNIT_ASSERT_EQUAL((rp.no_nodes - 1)*PKTS_PER_DST, unsigned(mt[i]->get_recv_pkts().size()) );
	 }
         
        for (unsigned i = 0; i < inf_ids.size(); i++)  
        {   
	    list<TerminalData*>& recv_pkts = mt[i]->get_recv_pkts();

            for (list<TerminalData*>::iterator it = recv_pkts.begin(); it != recv_pkts.end(); ++it)
            {
                 CPPUNIT_ASSERT_EQUAL(mt[i]->get_id(), (*it)->dest_id);
                 for (unsigned k = 0; k < TerminalData::SIZE; k++)
                    CPPUNIT_ASSERT_EQUAL(k, (*it)->data[k]);
            } 
        }          
    }

    //======================================================================
    //======================================================================
    //! @brief verify the behavior of torus netwrok
    //!
    //! see if PKTS_PER_DST packets have been correctly forwarded through the network   
    void torus_mass_test_0()
    {
        MasterClock.unregisterAll();
    
        //the paramters for torus network
        torus_init_params rp;
        rp.x_dim = 4;
        rp.y_dim = 4;
        rp.no_vcs = 4;
        rp.credits = 3;
        rp.link_width = 128;
	rp.ni_up_credits = rp.x_dim * rp.y_dim * PKTS_PER_DST;
	rp.ni_upstream_buffer_size = 5;
        
        Clock& clk = MasterClock;
        
        //creat torus network
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

	vector<int> node_lps(rp.x_dim * rp.y_dim);
	for(int i=0; i<rp.x_dim * rp.y_dim; i++)
	    node_lps[i] = 0;
        Torus<TerminalData>* topo = topoCreator<TerminalData>::create_torus(clk, &rp, mapping, slen, vnet, CREDIT_TYPE, &node_lps);
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
        
        Manifold::unhalt();
        Manifold::StopAt(300000); //need 300K cycles for all packets to be received.
        Manifold::Run();
        
        //verify
        for (int i = 0; i < inf_ids.size(); i ++) {
//cout << i << ":  " << " recv pkts= " << mt[i]->get_recv_pkts().size() << endl;

             CPPUNIT_ASSERT_EQUAL( (rp.x_dim*rp.y_dim - 1)*PKTS_PER_DST,  uint(mt[i]->get_recv_pkts().size()) );
	 }
         
        for (int i = 0; i < inf_ids.size(); i++)  
        {   
	    list<TerminalData*>& recv_pkts = mt[i]->get_recv_pkts();

            for (list<TerminalData*>::iterator it = recv_pkts.begin(); it != recv_pkts.end(); ++it)
            {
                 CPPUNIT_ASSERT_EQUAL(mt[i]->get_id(), (*it)->dest_id);
                 for (int k = 0; k < TerminalData::SIZE; k++)
                    CPPUNIT_ASSERT_EQUAL(k, (int)(*it)->data[k]);
            } 
        }
    }



#if 0
    //======================================================================
    //======================================================================
    //! @brief verify the behavior of CrossBar netwrok
    //!
    //! see if PKTS_PER_DST packets have been correctly forwarded through the network 
    void CrossBar_mass_test_0()
    {
        MasterClock.unregisterAll();
        
        //the paramters for CrossBar network
        CrossBar_init_params cb;
        cb.no_nodes = 4;
        cb.credits = 3;
        cb.link_width = 128;
        
        Clock& clk = MasterClock;
        
        //creat the CrossBar network
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
                Clock::Register(clk, mt[i], &Mockterminal::tick, &Mockterminal::tock);
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
        Manifold::StopAt(100000);
        Manifold::Run();
        
        //see if the ticket is right
        for (unsigned i = 0; i < inf_ids.size(); i ++)
             CPPUNIT_ASSERT_EQUAL( (cb.no_nodes - 1)*PKTS_PER_DST,  uint(mt[i]->get_recv_pkts().size()) );
         
        for (unsigned i = 0; i < inf_ids.size(); i++)  
        {   
	    list<TerminalData*>& recv_pkts = mt[i]->get_recv_pkts();

            for (list<TerminalData*>::iterator it = recv_pkts.begin(); it != recv_pkts.end(); ++it)
            {
                 CPPUNIT_ASSERT_EQUAL(mt[i]->get_id(), (*it)->dest_id);
                 for (unsigned k = 0; k < TerminalData::SIZE; k++)
                    CPPUNIT_ASSERT_EQUAL(k, (*it)->data[k]);
            } 
        }
        //cout<<topo->cb_switch->print_stats () <<endl;          
    }
#endif



    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("topoCreatorTest");
        mySuite->addTest(new CppUnit::TestCaller<topoCreatorMassTest>("ring_mass_test_0",
	                                                               &topoCreatorMassTest::ring_mass_test_0));
	mySuite->addTest(new CppUnit::TestCaller<topoCreatorMassTest>("torus_mass_test_0",
	                                                               &topoCreatorMassTest::torus_mass_test_0));
  //      mySuite->addTest(new CppUnit::TestCaller<topoCreatorMassTest>("CrossBar_mass_test_0",
//	                                                               &topoCreatorMassTest::CrossBar_mass_test_0));
	return mySuite;
    }
};

Clock topoCreatorMassTest :: MasterClock(topoCreatorMassTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( topoCreatorMassTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


