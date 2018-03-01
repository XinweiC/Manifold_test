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
#include "genericIrisInterface.h"

#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;

using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################

#define CREDIT_PKT 123


class TerminalData {
public:
    int type;
    uint src;
    uint dest_id;
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    uint get_src() { return src; }
    unsigned get_src_port() { return 0; }
    uint get_dst() { return dest_id; }
    uint get_dst_port() { return 0; }
    void set_dst_port(int p) {}
};

//! This class emulates a terminal
class MockSink : public manifold::kernel::Component {
public:
    enum {IN=0};
    enum {CREDITOUT=0};

    void handle_incoming_pkt (int, TerminalData* pkt)
    {
        m_pkts.push_back(*pkt);
	delete pkt;
    }
    
    void handle_incoming_lnkdt (int, LinkData* ld)
    {
        switch (ld->type){
            case FLIT:
                {
                    m_lnkdt_flits.push_back(*ld);

		    //send credit back
		    LinkData* cdt = new LinkData;
		    cdt->type = CREDIT;
		    cdt->vc = ld->vc;
		    Send(CREDITOUT , cdt);           
                    break;
                }
            case CREDIT:
                {
                    m_lnkdt_credit.push_back(*ld);
                    break;
                } 
        }
        
        delete ld;
    }

    list<TerminalData>* get_pkts() { return &m_pkts; }
    list<LinkData>* get_lnkdt_flits() { return &m_lnkdt_flits; }
    list<LinkData>* get_lnkdt_credit() { return &m_lnkdt_credit; }
    
    list<TerminalData> m_pkts;
    list<LinkData> m_lnkdt_flits;
    list<LinkData> m_lnkdt_credit;
private:
    
};

//this class emulate the packet src
class MockpktGenerator : public manifold::kernel::Component {
public:
    enum {IN=0, PKTOUT};
    int cyc_count; //cycle count
    int pkt_count;
    const int FREQ;
    
    MockpktGenerator(int freq_c) : FREQ(freq_c)
    {   
        cyc_count = 0;
        pkt_count = 0;
    }
    
    void tick ()
    {  
       if (((cyc_count%FREQ) == 0) && (pkt_count <= 5))
       {  
          TerminalData* td = new TerminalData;
          td->src = this->GetComponentId();
          td->dest_id = 25;
          Send(PKTOUT , td);
          pkt_count++;
       }   
       cyc_count ++; 
    }
    
    void tock()
    {}
};

//this class emulate the linkdata src
class MocklkdGenerator : public manifold::kernel::Component {
public:
    enum {IN=0, LKDOUT};
    int cyc_count;
    int lkd_count;
    int total_f;
    int freq; //how often to generate a packet
    int no_vcs;
    int h_count;
    int t_count;
    int vc;
    int no_pkt;
    
    MocklkdGenerator(int freq_c, int total_length, int num_vc, int num_pkt)
    {   
        freq = freq_c;
        cyc_count = 0;
        lkd_count = 0;
        h_count = 0;
        t_count = 0;
        total_f = total_length;
        no_vcs = num_vc;
        vc = 0;
        no_pkt = num_pkt;
        //cout<<"pg tick init"<<cyc_count<<endl;
    }
    
    void tick ()
    {  
       if (((cyc_count%freq) == 0) && (lkd_count < (total_f * no_pkt)))
       {  
          if ((lkd_count%total_f) == 0)
          {
              h_count++;
              LinkData* ld =  new LinkData();
              ld->type = FLIT;
              ld->src = this->GetComponentId();
              HeadFlit* hf = new HeadFlit();
              hf->type = HEAD;
              hf->src_id = lkd_count%no_vcs;
              hf->dst_id = lkd_count%no_vcs;
              hf->pkt_length = total_f;
              ld->f = hf;
              vc = lkd_count%no_vcs;
              ld->vc = vc;
              Send(LKDOUT, ld);
          }
          else if ((lkd_count%total_f) == (total_f - 1))
          {
              t_count++;
              LinkData* ld =  new LinkData();
              ld->type = FLIT;
              ld->src = this->GetComponentId();
              TailFlit* tf = new TailFlit();
              tf->type = TAIL;
              tf->pkt_length = total_f;
              //ld->f->flit_id = lkd_count;
              ld->f = tf;
              ld->vc = vc;
              Send(LKDOUT, ld);
          }
          else
          {
              LinkData* ld =  new LinkData();
              ld->type = FLIT;
              ld->src = this->GetComponentId();
              ld->f = new BodyFlit();
              ld->f->type = BODY;
              ld->f->pkt_length = total_f;
              ld->vc = vc;
              Send(LKDOUT, ld);
          }
          lkd_count++;
       }   
       cyc_count ++; 
    }
    
    void tock()
    {}
};

//####################################################################
//! Class NetworkInterfaceTest is the test class for class NetworkInterface. 
//####################################################################
class GenNetworkInterfaceTest : public CppUnit::TestFixture {
private:

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };
    
    //======================================================================
    //======================================================================
    //! @brief verify the behavior of downstream link (pktgen -> interface -> sink(lkd)) of interface
    //!
    //! Create a generic network interface and connect it to a MockpktGenerator and a MockSink  
    //! see if all the packets generated by MockpktGenerator received correctly at MockSink
    void component_test_0 ()
    {
	//clean all registered object
        MasterClock.unregisterAll();
        
        //initialzing the interface
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10000;
	i_p->upstream_buffer_size = 5;
	i_p->up_credit_msg_type = CREDIT_PKT;

	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

	NIInit<TerminalData> init(mapping, slen, vnet);

        CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, 0, init, i_p);
        CompId_t ms_cid = Component :: Create<MockSink> (0);
	const int CPP = random() % 5 + 2; //2 to 6 cycles per packet
        CompId_t pg_cid = Component :: Create<MockpktGenerator> (0, CPP); //generate a pkt every CPP cycles
        
        GenNetworkInterface<TerminalData>* GnI = Component :: GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
        MockSink* MsinK = Component :: GetComponent<MockSink>(ms_cid);
        MockpktGenerator* MpG = Component :: GetComponent<MockpktGenerator>(pg_cid);
        
        Clock::Register(MasterClock, GnI, &GenNetworkInterface<TerminalData>::tick, &GenNetworkInterface<TerminalData>::tock);
        Clock::Register(MasterClock, MpG, &MockpktGenerator::tick, &MockpktGenerator::tock);
        
        
        //connect the components together
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT, ms_cid, 
                          0, &MockSink::handle_incoming_lnkdt,1);
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::TERMINAL_PORT, ms_cid, 
                          2, &MockSink::handle_incoming_pkt,1);                 
        Manifold::Connect(pg_cid, MockpktGenerator::PKTOUT, ni_cid, GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                          &GenNetworkInterface<TerminalData>::handle_new_packet_event,1);
        Manifold::Connect(ms_cid, MockSink::CREDITOUT, ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT,
                          &GenNetworkInterface<TerminalData>::handle_router,1);
                          
        Manifold::unhalt();
        Manifold::StopAt(1000);
        Manifold::Run();
        
	//calculate the total flits in a packet
        unsigned num_bytes = sizeof(TerminalData) + HeadFlit::HEAD_FLIT_OVERHEAD;
	unsigned num_flits = num_bytes*8/i_p->linkWidth;
	if(num_bytes*8 % i_p->linkWidth != 0)
	    num_flits++;
           
        unsigned total = num_flits;
	if(num_flits > 1)
	    total++; //if body flit is needed, then tail is also needed.

	//verify all flits have been received.
        CPPUNIT_ASSERT_EQUAL(MpG->pkt_count * total, (unsigned)MsinK->m_lnkdt_flits.size());
    }
 
    //======================================================================
    //======================================================================
    //! @brief verify the behavior of dupstream link (lkdgen -> interface -> sink(pkt)) of interface
    //!
    //! Create a generic network interface and connect it to a MocklkdGenerator and a MockSink  
    //! see if all the LinkData generated by MocklkdGenerator have been successfully received, 
    //! transfered to packet and sent to MockSink   
    void component_test_1 ()
    {
        int num_pkt = 25;
	//clean all registered object
        MasterClock.unregisterAll();
        
        //initialzing the interface
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 1000;
	i_p->upstream_buffer_size = 5;
	i_p->up_credit_msg_type = CREDIT_PKT;
        
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	VnetAssign<TerminalData>* vnet = new VnetAssign<TerminalData>();

	NIInit<TerminalData> init(mapping, slen, vnet);

        CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, 0, init, i_p);
        CompId_t ms_cid = Component :: Create<MockSink> (0);
        GenNetworkInterface<TerminalData>* GnI = Component :: GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
        MockSink* MsinK = Component :: GetComponent<MockSink>(ms_cid);
        Clock::Register(MasterClock, GnI, &GenNetworkInterface<TerminalData>::tick, &GenNetworkInterface<TerminalData>::tock);
        

        
	//calculate the total flits in a packet
        unsigned num_bytes = sizeof(TerminalData) + HeadFlit::HEAD_FLIT_OVERHEAD;
	unsigned num_flits = num_bytes*8/i_p->linkWidth;
	if(num_bytes*8 % i_p->linkWidth != 0)
	    num_flits++;
           
        unsigned total = num_flits;
	if(num_flits > 1)
	    total++; //if body flit is needed, then tail is also needed.
 
        CompId_t ldg_cid = Component :: Create<MocklkdGenerator> (0, 2, total, i_p->num_vc, num_pkt);        
        MocklkdGenerator* MldG = Component :: GetComponent<MocklkdGenerator>(ldg_cid);
        Clock::Register(MasterClock, MldG, &MocklkdGenerator::tick, &MocklkdGenerator::tock);        
                
        //connect the components together
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT, ms_cid, 
                          0, &MockSink::handle_incoming_lnkdt,1);
        //Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::SIGNALOUT, ms_cid, 
         //                 1, &MockSink::handle_incoming_lnkdt,1); 
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::TERMINAL_PORT, ms_cid, 
                          2, &MockSink::handle_incoming_pkt,1);                 
        Manifold::Connect(ldg_cid, MocklkdGenerator::LKDOUT, ni_cid, GenNetworkInterface<TerminalData>::TERMINAL_PORT,
                          &GenNetworkInterface<TerminalData>::handle_router,1);
        Manifold::Connect(ms_cid, MockSink::CREDITOUT, ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT,
                          &GenNetworkInterface<TerminalData>::handle_router,1);
                          
        Manifold::unhalt();
        Manifold::StopAt(1000);
        Manifold::Run();
        
        CPPUNIT_ASSERT_EQUAL(num_pkt, int(MsinK->m_pkts.size()));
    }
    
    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("NnewIrisInterfaceTest");
        mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("component_test_0",
	                                                                   &GenNetworkInterfaceTest::component_test_0));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("component_test_1",
	                                                                   &GenNetworkInterfaceTest::component_test_1));
	return mySuite;
    }
};

Clock GenNetworkInterfaceTest :: MasterClock(GenNetworkInterfaceTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( GenNetworkInterfaceTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


