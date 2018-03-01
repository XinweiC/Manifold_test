/*
 * =====================================================================================
 *    Description:  unit tests for generic iris interface
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

class TerminalData {
public:
    int type;
    uint src;
    uint dest_id;
    int data;
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    uint get_src() { return src; }
    uint get_src_port() { return 0; }
    uint get_dst() { return dest_id; }
    uint get_dst_port() { return 0; }
    void set_dst_port(int p) { }
};

//TerminalDataBig's size is > HeadFlit::MAX_DATA_SIZE; so the actual data
//would need more than 1 flit.
class TerminalDataBig {
public:
    static const int SIZE = HeadFlit :: MAX_DATA_SIZE + 10;
    static int Simu_len; //simulated length
    int type;
    uint src;
    uint dest_id;
    int data[SIZE];
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    uint get_src() { return src; }
    uint get_src_port() { return 0; }
    uint get_dst() { return dest_id; }
    uint get_dst_port() { return 0; }
    void set_dst_port(int p) { }
};

int TerminalDataBig :: Simu_len = 0;


class TerminalData2 {
public:
    static int Simu_len; //simulated length
    static const int SIZE = 10; //a small packet

    int type;
    char data[SIZE];
    int src;
    int dest;
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    int get_src() { return src; }
    int get_src_port() { return 0; }
    int get_dst() { return dest; }
    void set_dst_port(int p) { }
};

int TerminalData2 :: Simu_len = 0;


class MySimLenBig : public SimulatedLen<TerminalDataBig> {
public:
    int get_simulated_len(TerminalDataBig* d) { return d->Simu_len; }
};


class MySimLen2 : public SimulatedLen<TerminalData2> {
public:
    int get_simulated_len(TerminalData2* d) { return d->Simu_len; }
};



class MyVnet : public VnetAssign<TerminalData> {
public:
    int get_virtual_net(TerminalData*) { return 0; }
};

class MyVnetBig : public VnetAssign<TerminalDataBig> {
public:
    int get_virtual_net(TerminalDataBig*) { return 0; }
};

class MyVnet2 : public VnetAssign<TerminalData2> {
public:
    int get_virtual_net(TerminalData2*) { return 0; }
};





//! This class emulates a terminal
class MockSink : public manifold::kernel::Component {
public:
    enum {IN=0};

    void handle_incoming_pkt (int, TerminalData* pkt)
    {
        m_pkts.push_back(*pkt);
	//delete pkt;
    }
    
    void handle_incoming_lnkdt (int, LinkData* ld)
    {
        m_lnkdt.push_back(*ld);
        //delete ld;
    }

    list<TerminalData>* get_pkts() { return &m_pkts; }
    list<LinkData>* get_lnkdt() { return &m_lnkdt; }
private:
    list<TerminalData> m_pkts;
    //list<Ticks_t> m_ticks;
    
    list<LinkData> m_lnkdt;
    //list<Ticks_t> m_ticks;
    
};

class MockLinkDataGenerator : public manifold::kernel::Component {
public:
    enum {IN=0};

    void send_lnkdt_flit (int vc)
    {
        LinkData* ld = new LinkData;
        ld->type = FLIT;
        ld->src = this->GetComponentId();
        ld->f = new Flit;
        ld->f->type = HEAD;
        ld->f->pkt_length = 3;
        ld->vc = vc;
        Send(3 , ld);
    }
    
    void send_lnkdt_credit (int vc)
    {
        LinkData* ld = new LinkData;
        ld->type = CREDIT;
        ld->vc = vc;
        Send(3 , ld);
    }
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
    //! @brief Test init() of generic interface: vector size and initial value.
    //!
    //! create an interface and call init() function with given parameters
    //! Verify the vectors' size and the initial values.    
    void test_Interface_init_0()
    {
        //initialzing the interface and the arbiter will be init also
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;

	NIInit<TerminalData> init(0,0,0);
        GenNetworkInterface<TerminalData>* GnI = new GenNetworkInterface<TerminalData>(0, init, i_p);
        
        //tests whether the size are correct	
        CPPUNIT_ASSERT_EQUAL(uint(GnI->router_in_buffer.get_buffers().size()), GnI->no_vcs);
        CPPUNIT_ASSERT_EQUAL(uint(GnI->router_out_buffer.get_buffers().size()), GnI->no_vcs);
        CPPUNIT_ASSERT_EQUAL(uint(GnI->proc_out_buffer.size()), GnI->no_vcs);
        CPPUNIT_ASSERT_EQUAL(uint(GnI->proc_in_buffer.size()), GnI->no_vcs);
        CPPUNIT_ASSERT_EQUAL(uint(GnI->is_proc_out_buffer_free.size()), GnI->no_vcs);
        CPPUNIT_ASSERT_EQUAL(uint(GnI->downstream_credits.size()), GnI->no_vcs);
        //CPPUNIT_ASSERT_EQUAL(uint(GnI->router_ob_packet_complete.size()), GnI->no_vcs);
        
        //test whether the inital value are correct
        for (unsigned i = 0; i < GnI->no_vcs; i++)
        {
            CPPUNIT_ASSERT_EQUAL(bool(GnI->is_proc_out_buffer_free[i]), true);
            //CPPUNIT_ASSERT_EQUAL(bool(GnI->router_ob_packet_complete[i]), false); 
            CPPUNIT_ASSERT_EQUAL(uint(GnI->downstream_credits[i]), GnI->credits); 
        }
    }

    //======================================================================
    //======================================================================
    //! @brief Test handle_new_packet_event() of generic interface
    //!
    //! Verify whether the incoming pkt has been received and saved correctly
    void test_Interface_handle_new_packet_event_0 ()
    {
        //initialzing the interface and the arbiter will be init also
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	NIInit<TerminalData> init(0,0,0);
        GenNetworkInterface<TerminalData>* GnI = new GenNetworkInterface<TerminalData>(0, init, i_p);  
        
        //test      
        CPPUNIT_ASSERT_EQUAL(0,int(GnI->input_pkt_buffer.size()));
        TerminalData* td = new TerminalData;
        td->src = random()%10;
        td->dest_id = random()%10;
        GnI->handle_new_packet_event(GenNetworkInterface<TerminalData>::TERMINAL_PORT, td);
        TerminalData* td_1 = GnI->input_pkt_buffer.front();
        CPPUNIT_ASSERT_EQUAL(td->src, td_1->src);
        CPPUNIT_ASSERT_EQUAL(td->dest_id, td_1->dest_id);
    }
    
    //======================================================================
    //======================================================================
    //! @brief Test to_flit_level_packet() and from_flit_level_packet() of generic interface
    //!
    //! Create a packet and call to_flit_level_packet(). Verify the number body flits in the
    //! FLP is as expected. Call from_flit_level_packet() and verify the original packet can
    //! be correctly reconstructed.
    void test_to_flit_level_packet_0()
    {
        //initialzing the interface and the arbiter will be init also
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	const unsigned IF_ID = random() % 1024; //interface ID
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>;
	MyVnet* vnet = new MyVnet();

	NIInit<TerminalData> init(mapping, slen, vnet);
        GenNetworkInterface<TerminalData>* GnI = new GenNetworkInterface<TerminalData>(IF_ID, init, i_p);
        
        //calculate the total length
        unsigned num_bytes = sizeof(TerminalData) + HeadFlit::HEAD_FLIT_OVERHEAD;
	unsigned num_flits = num_bytes*8/i_p->linkWidth;
	if(num_bytes*8 % i_p->linkWidth != 0)
	    num_flits++;
           
        unsigned total = num_flits;
	if(num_flits > 1)
	    total++; //if body flit is needed, then tail is also needed.
        
        //test
        TerminalData* td = new TerminalData;
        td->src = random() % 1024;
        td->dest_id = random() % 1024;
	td->data = random() % (0x1 << 20);

        FlitLevelPacket* flp = new FlitLevelPacket;
	Ticks_t enter_net_time = random();
        GnI->to_flit_level_packet(flp, td, enter_net_time);
        //see if the dest and src id passed correctly
        CPPUNIT_ASSERT_EQUAL(total, flp->size());
        CPPUNIT_ASSERT_EQUAL(mapping->terminal_to_net(td->dest_id), (int)flp->dst_id);
        CPPUNIT_ASSERT_EQUAL(IF_ID, flp->src_id); //FlipLevelPacket's src_node must be the interface id.
        
        //see if the flits be generated correctly
        for (unsigned i = 0; i < flp->pkt_length; i++)
        {
            if (i == 0) {
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, HEAD);
               CPPUNIT_ASSERT_EQUAL(enter_net_time, ((HeadFlit*)(flp->flits[i]))->enter_network_time);
	   }
               
            else if (i == (flp->pkt_length - 1))
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, TAIL);
               
            else
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, BODY);
        }

	//call from_flit_level_packet() to reconstruct
	TerminalData* td2 = GnI->from_flit_level_packet(flp);
	CPPUNIT_ASSERT_EQUAL(td->src, td2->src);
	CPPUNIT_ASSERT_EQUAL(td->dest_id, td2->dest_id);
	CPPUNIT_ASSERT_EQUAL(td->data, td2->data);

	delete mapping;
	delete GnI;
	delete td;
	delete td2;
	delete flp;
    }


    //======================================================================
    //======================================================================
    //! @brief Test to_flit_level_packet() and from_flit_level_packet() of generic interface
    //!
    //! Use TerminalDataBig with simulated length set to the size of the data structure.
    //!
    //! Create a packet and call to_flit_level_packet(). Verify the number body flits in the
    //! FLP is as expected. Call from_flit_level_packet() and verify the original packet can
    //! be correctly reconstructed.
    void test_to_flit_level_packet_1()
    {
        //initialzing the interface and the arbiter will be init also
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	const unsigned IF_ID = random() % 1024; //interface ID
	SimulatedLen<TerminalDataBig>* slen = new MySimLenBig();
	TerminalDataBig :: Simu_len = sizeof(TerminalDataBig);
	MyVnetBig* vnet = new MyVnetBig();

	NIInit<TerminalDataBig> init(mapping, slen, vnet);
        GenNetworkInterface<TerminalDataBig>* GnI = new GenNetworkInterface<TerminalDataBig>(IF_ID, init, i_p);
        
        //calculate the total length
	TerminalDataBig temp;
        unsigned num_bytes = slen->get_simulated_len(&temp) + HeadFlit::HEAD_FLIT_OVERHEAD;
	unsigned num_flits = num_bytes*8/i_p->linkWidth;
	if(num_bytes*8 % i_p->linkWidth != 0)
	    num_flits++;
           
        unsigned total = num_flits;
	if(num_flits > 1)
	    total++; //if body flit is needed, then tail is also needed.
        
        //test
        TerminalDataBig* td = new TerminalDataBig;
        td->src = random() % 1024;
        td->dest_id = random() % 1024;
	for(int i=0; i<TerminalDataBig::SIZE; i++)
	    td->data[i] = random() % (0x1 << 20);

        FlitLevelPacket* flp = new FlitLevelPacket;
        GnI->to_flit_level_packet(flp, td, 0);
        //see if the dest and src id passed correctly
        CPPUNIT_ASSERT_EQUAL(total, flp->size());
        CPPUNIT_ASSERT_EQUAL(mapping->terminal_to_net(td->dest_id), (int)flp->dst_id);
        CPPUNIT_ASSERT_EQUAL(IF_ID, flp->src_id); //FlipLevelPacket's src_node must be the interface id.
        
        //see if the flits be generated correctly
        for (unsigned i = 0; i < flp->pkt_length; i++)
        {
            if (i == 0)
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, HEAD);
               
            else if (i == (flp->pkt_length - 1))
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, TAIL);
               
            else
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, BODY);
        }

	//call from_flit_level_packet() to reconstruct
	TerminalDataBig* td2 = GnI->from_flit_level_packet(flp);
	CPPUNIT_ASSERT_EQUAL(td->src, td2->src);
	CPPUNIT_ASSERT_EQUAL(td->dest_id, td2->dest_id);
	for(int i=0; i<TerminalDataBig::SIZE; i++)
	    CPPUNIT_ASSERT_EQUAL(td->data[i], td2->data[i]);

	delete mapping;
	delete GnI;
	delete td;
	delete td2;
	delete flp;
    }


    //======================================================================
    //======================================================================
    //! @brief Test to_flit_level_packet() and from_flit_level_packet() of generic interface
    //!
    //! Use TerminalDataBig with simulated length set to a value smaller than actual size.
    //!
    //! Create a packet and call to_flit_level_packet(). Verify the number body flits in the
    //! FLP is as expected. Call from_flit_level_packet() and verify the original packet can
    //! be correctly reconstructed.
    void test_to_flit_level_packet_1_1()
    {
        //initialzing the interface and the arbiter will be init also
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	const unsigned IF_ID = random() % 1024; //interface ID
	SimulatedLen<TerminalDataBig>* slen = new MySimLenBig();
	TerminalDataBig :: Simu_len = sizeof(TerminalDataBig) / 2;
	MyVnetBig* vnet = new MyVnetBig();

	NIInit<TerminalDataBig> init(mapping, slen, vnet);
        GenNetworkInterface<TerminalDataBig>* GnI = new GenNetworkInterface<TerminalDataBig>(IF_ID, init, i_p);
        
        //calculate the total length
	TerminalDataBig temp;
        unsigned num_bytes = slen->get_simulated_len(&temp) + HeadFlit::HEAD_FLIT_OVERHEAD;
	unsigned num_flits = num_bytes*8/i_p->linkWidth;
	if(num_bytes*8 % i_p->linkWidth != 0)
	    num_flits++;
           
        unsigned total = num_flits;
	if(num_flits > 1)
	    total++; //if body flit is needed, then tail is also needed.
        
        //test
        TerminalDataBig* td = new TerminalDataBig;
        td->src = random() % 1024;
        td->dest_id = random() % 1024;
	for(int i=0; i<TerminalDataBig::SIZE; i++)
	    td->data[i] = random() % (0x1 << 20);

        FlitLevelPacket* flp = new FlitLevelPacket;
        GnI->to_flit_level_packet(flp, td, 0);
        //see if the dest and src id passed correctly
        CPPUNIT_ASSERT_EQUAL(total, flp->size());
        CPPUNIT_ASSERT_EQUAL(mapping->terminal_to_net(td->dest_id), (int)flp->dst_id);
        CPPUNIT_ASSERT_EQUAL(IF_ID, flp->src_id); //FlipLevelPacket's src_node must be the interface id.
        
        //see if the flits be generated correctly
        for (unsigned i = 0; i < flp->pkt_length; i++)
        {
            if (i == 0)
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, HEAD);
               
            else if (i == (flp->pkt_length - 1))
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, TAIL);
               
            else
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, BODY);
        }

	//call from_flit_level_packet() to reconstruct
	TerminalDataBig* td2 = GnI->from_flit_level_packet(flp);
	CPPUNIT_ASSERT_EQUAL(td->src, td2->src);
	CPPUNIT_ASSERT_EQUAL(td->dest_id, td2->dest_id);
	for(int i=0; i<TerminalDataBig::SIZE; i++)
	    CPPUNIT_ASSERT_EQUAL(td->data[i], td2->data[i]);

	delete mapping;
	delete GnI;
	delete td;
	delete td2;
	delete flp;
    }


    //======================================================================
    //======================================================================
    //! @brief Test to_flit_level_packet() and from_flit_level_packet: packet
    //! whose simulated len is smaller than actual size.
    //!
    //! Create a packet whose simualted length is smaller than its actual size, and call
    //! to_flit_level_packet() to create a flit level packet. Verify the number of body
    //! flits created is as expected, and the packet is correctly copied into the body flits.
    //! Call from_flit_level_packet() to verify the original packet can be reconstructed
    //! from the flit level packet.
    void test_to_flit_level_packet_2()
    {
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	const unsigned IF_ID = random() % 1024; //interface ID
	MySimLen2* slen = new MySimLen2;
	MyVnet2* vnet = new MyVnet2();

	NIInit<TerminalData2> init(mapping, slen, vnet);
        GenNetworkInterface<TerminalData2>* GnI = new GenNetworkInterface<TerminalData2>(IF_ID, init, i_p);

        TerminalData2 :: Simu_len = random() % 5 + 1; //1 to 5 bytes.
        
        //calculate the expected number of flits.
        unsigned num_bytes = HeadFlit::HEAD_FLIT_OVERHEAD + TerminalData2 :: Simu_len;
        unsigned num_flits = num_bytes*8/i_p->linkWidth;
        if(num_bytes * 8 % i_p->linkWidth != 0)
           num_flits++;

        unsigned total_flits = num_flits;
	if(num_flits > 1)
	    total_flits++;
        
        //Create a TerminalData2 object.
        TerminalData2* td = new TerminalData2;
        td->src = random() % 1024;
        td->dest = random() % 1024;
	for(int i=0; i<TerminalData2::SIZE; i++)
	    td->data[i] = i;

        FlitLevelPacket* flp = new FlitLevelPacket;
        GnI->to_flit_level_packet(flp, td, 0);

	//verify the number of flits is as expected.
        CPPUNIT_ASSERT_EQUAL(total_flits, flp->size());

        //see if the dest and src id passed correctly
        CPPUNIT_ASSERT_EQUAL(mapping->terminal_to_net(td->dest), (int)flp->dst_id);
        CPPUNIT_ASSERT_EQUAL(IF_ID, flp->src_id); //FlipLevelPacket's src_node must be the interface id.
        
        //see if the flits are generated correctly
        for (unsigned i = 0; i < flp->pkt_length; i++)
        {
            if (i == 0)
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, HEAD);
               
            else if (i == (flp->pkt_length - 1))
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, TAIL);
               
            else
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, BODY);
        }

	//verify data in the flits are correct
	char buf[sizeof(TerminalData2)];

	int count = 0;

        for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
	    if(count < (int)sizeof(TerminalData2)) {
	        buf[count] = static_cast<HeadFlit*>(flp->flits[0])->data[j];
	        count++;
	    }
	    else
	        break;
        }

	for(unsigned i=0; i<num_flits-1; i++) {
	   for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
	       if(count < (int)sizeof(TerminalData2)) {
	           buf[count] = static_cast<BodyFlit*>(flp->flits[i+1])->data[j];
		   count++;
	       }
	       else
	           break;
	   }
	   if(count >= (int)sizeof(TerminalData2))
	       break;
	}

	//reconstruct a TerminalData2 object from the body flits.
	TerminalData2* td2 = (TerminalData2*)buf;

        //also test from_flit_level_packet
	TerminalData2* td3 = GnI->from_flit_level_packet(flp);


	//verify the reconstructed object is the same as the original
	//also verify the object created by from_flit_level_packet() is the same as the original
	for(int i=0; i<TerminalData2::SIZE; i++) {
	    CPPUNIT_ASSERT_EQUAL(td->data[i], td2->data[i]);
	    CPPUNIT_ASSERT_EQUAL(td->data[i], td3->data[i]);
	}
	CPPUNIT_ASSERT_EQUAL(td->src, td2->src);
	CPPUNIT_ASSERT_EQUAL(td->dest, td2->dest);
	CPPUNIT_ASSERT_EQUAL(td->src, td3->src);
	CPPUNIT_ASSERT_EQUAL(td->dest, td3->dest);

	delete mapping;
	delete GnI;
	delete td;
	delete td3;
	delete flp;
    }



    //======================================================================
    //======================================================================
    //! @brief Test to_flit_level_packet() and from_flit_level_packet: packet
    //! whose simulated len is smaller than actual size.
    //!
    //! Same as above, except here we verify only 1 flit is created.
    //!
    //! Create a packet whose simualted length is smaller than its actual size, and call
    //! to_flit_level_packet() to create a flit level packet. Verify the number of body
    //! flits created is as expected, and the packet is correctly copied into the body flits.
    //! Call from_flit_level_packet() to verify the original packet can be reconstructed
    //! from the flit level packet.
    void test_to_flit_level_packet_2_1()
    {
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	const unsigned IF_ID = random() % 1024; //interface ID
	MySimLen2* slen = new MySimLen2;
	MyVnet2* vnet = new MyVnet2();

	NIInit<TerminalData2> init(mapping, slen, vnet);
        GenNetworkInterface<TerminalData2>* GnI = new GenNetworkInterface<TerminalData2>(IF_ID, init, i_p);

        TerminalData2 :: Simu_len = random() % 5 + 1; //1 to 5 bytes.
        
        //calculate the expected number of flits.
        unsigned num_bytes = HeadFlit::HEAD_FLIT_OVERHEAD + TerminalData2 :: Simu_len;
        unsigned num_flits = num_bytes*8/i_p->linkWidth;
        if(num_bytes * 8 % i_p->linkWidth != 0)
           num_flits++;

	CPPUNIT_ASSERT_EQUAL(1, (int)num_flits);

        unsigned total_flits = num_flits;
        
        //Create a TerminalData2 object.
        TerminalData2* td = new TerminalData2;
        td->src = random() % 1024;
        td->dest = random() % 1024;
	for(int i=0; i<TerminalData2::SIZE; i++)
	    td->data[i] = i;

        FlitLevelPacket* flp = new FlitLevelPacket;
        GnI->to_flit_level_packet(flp, td, 0);

	//verify the number of flits is as expected.
        CPPUNIT_ASSERT_EQUAL(total_flits, flp->size());

        //see if the dest and src id passed correctly
        CPPUNIT_ASSERT_EQUAL(mapping->terminal_to_net(td->dest), (int)flp->dst_id);
        CPPUNIT_ASSERT_EQUAL(IF_ID, flp->src_id); //FlipLevelPacket's src_node must be the interface id.
        

        //see if the flits are generated correctly
	CPPUNIT_ASSERT_EQUAL(1, (int)flp->pkt_length);
	CPPUNIT_ASSERT_EQUAL(flp->flits[0]->type, HEAD);

	//verify data in the flits are correct
	char buf[sizeof(TerminalData2)];

	int count = 0;

        for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
	    if(count < (int)sizeof(TerminalData2)) {
	        buf[count] = static_cast<HeadFlit*>(flp->flits[0])->data[j];
	        count++;
	    }
	    else
	        break;
        }

	//reconstruct a TerminalData2 object from the body flits.
	TerminalData2* td2 = (TerminalData2*)buf;

        //also test from_flit_level_packet
	TerminalData2* td3 = GnI->from_flit_level_packet(flp);


	//verify the reconstructed object is the same as the original
	//also verify the object created by from_flit_level_packet() is the same as the original
	for(int i=0; i<TerminalData2::SIZE; i++) {
	    CPPUNIT_ASSERT_EQUAL(td->data[i], td2->data[i]);
	    CPPUNIT_ASSERT_EQUAL(td->data[i], td3->data[i]);
	}
	CPPUNIT_ASSERT_EQUAL(td->src, td2->src);
	CPPUNIT_ASSERT_EQUAL(td->dest, td2->dest);
	CPPUNIT_ASSERT_EQUAL(td->src, td3->src);
	CPPUNIT_ASSERT_EQUAL(td->dest, td3->dest);

	delete mapping;
	delete GnI;
	delete td;
	delete td3;
	delete flp;
    }





    //======================================================================
    //======================================================================
    //! @brief Test to_flit_level_packet() and from_flit_level_packet: packet
    //! whose simulated len is greater than actual size.
    //!
    //! Create a packet whose simualted length is smaller than its actual size, and call
    //! to_flit_level_packet() to create a flit level packet. Verify the number of body
    //! flits created is as expected, and the packet is correctly copied into the body flits.
    //! Call from_flit_level_packet() to verify the original packet can be reconstructed
    //! from the flit level packet.
    void test_to_flit_level_packet_2_2()
    {
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	const unsigned IF_ID = random() % 1024; //interface ID
	MySimLen2* slen = new MySimLen2;
	MyVnet2* vnet = new MyVnet2();

	NIInit<TerminalData2> init(mapping, slen, vnet);
        GenNetworkInterface<TerminalData2>* GnI = new GenNetworkInterface<TerminalData2>(IF_ID, init, i_p);

        TerminalData2 :: Simu_len = random() % (sizeof(TerminalData2)) + sizeof(TerminalData2) + 1; // between SIZE+1 and 2*SIZE
        
        //calculate the expected number of flits.
        unsigned num_bytes = HeadFlit::HEAD_FLIT_OVERHEAD + TerminalData2 :: Simu_len;
        unsigned num_flits = num_bytes*8/i_p->linkWidth;
        if(num_bytes * 8 % i_p->linkWidth != 0)
           num_flits++;

        unsigned total_flits = num_flits;
	if(num_flits > 1)
	    total_flits++;
        
        //Create a TerminalData2 object.
        TerminalData2* td = new TerminalData2;
        td->src = random() % 1024;
        td->dest = random() % 1024;
	for(int i=0; i<TerminalData2::SIZE; i++)
	    td->data[i] = i;

        FlitLevelPacket* flp = new FlitLevelPacket;
        GnI->to_flit_level_packet(flp, td, 0);

	//verify the number of body flits is as expected.
        CPPUNIT_ASSERT_EQUAL(total_flits, flp->size()); //one head and one tail.

        //see if the dest and src id passed correctly
        CPPUNIT_ASSERT_EQUAL(mapping->terminal_to_net(td->dest), (int)flp->dst_id);
        CPPUNIT_ASSERT_EQUAL(IF_ID, flp->src_id); //FlipLevelPacket's src_node must be the interface id.
        
        //see if the flits are generated correctly
        for (unsigned i = 0; i < flp->pkt_length; i++)
        {
            if (i == 0)
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, HEAD);
               
            else if (i == (flp->pkt_length - 1))
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, TAIL);
               
            else
               CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, BODY);
        }

	//verify data in the flits are correct
	char buf[sizeof(TerminalData2)];

	int count = 0;

        for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
	    if(count < (int)sizeof(TerminalData2)) {
	        buf[count] = static_cast<HeadFlit*>(flp->flits[0])->data[j];
	        count++;
	    }
	    else
	        break;
        }

	for(unsigned i=0; i<num_flits-1; i++) {
	   for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
	       if(count < (int)sizeof(TerminalData2)) {
	           buf[count] = static_cast<BodyFlit*>(flp->flits[i+1])->data[j];
		   count++;
	       }
	       else
	           break;
	   }
	   if(count >= (int)sizeof(TerminalData2))
	       break;
	}

	//reconstruct a TerminalData2 object from the body flits.
	TerminalData2* td2 = (TerminalData2*)buf;

        //also test from_flit_level_packet
	TerminalData2* td3 = GnI->from_flit_level_packet(flp);


	//verify the reconstructed object is the same as the original
	//also verify the object created by from_flit_level_packet() is the same as the original
	for(int i=0; i<TerminalData2::SIZE; i++) {
	    CPPUNIT_ASSERT_EQUAL(td->data[i], td2->data[i]);
	    CPPUNIT_ASSERT_EQUAL(td->data[i], td3->data[i]);
	}
	CPPUNIT_ASSERT_EQUAL(td->src, td2->src);
	CPPUNIT_ASSERT_EQUAL(td->dest, td2->dest);
	CPPUNIT_ASSERT_EQUAL(td->src, td3->src);
	CPPUNIT_ASSERT_EQUAL(td->dest, td3->dest);

	delete mapping;
	delete GnI;
	delete td;
	delete td3;
	delete flp;
    }




    //======================================================================
    //======================================================================
    //! @brief Test tick() of generic interface
    //!
    //! The tick() function has two parts: the 1st part handles terminal input, and
    //! the 2nd part handles router input. This test case tests the first part.
    //!
    //! Call tock() to move packets from pkt_input_buffer to proc_out_buffer. Then
    //! call tick() once, one flit from each proc_out_buffer with flits is moved to
    //! router_out_buffer. Then arbitration kicks in, and only one flit moves from
    //! router_out_buffer to the router.
    void test_Interface_tick_0_call_tock(GenNetworkInterface<TerminalData>* GnI, unsigned nflits)
    {
        for (unsigned i = 0; i < GnI->no_vcs ; i++)
        {
            GnI->tock(); //call tock() to move packets from input buffer to proc_out_buffer.
	    if(i%2 == 0)
		CPPUNIT_ASSERT_EQUAL(nflits, GnI->proc_out_buffer[i].size());
	    else
		CPPUNIT_ASSERT_EQUAL(0, (int)GnI->proc_out_buffer[i].size());
        }
        
    }

    void test_Interface_tick_0()
    {
        //initialzing the interface
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
        
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>();
	MyVnet* vnet = new MyVnet();

	NIInit<TerminalData> init(mapping, slen, vnet);
        CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, 0, init, i_p);
        CompId_t ms_cid = Component :: Create<MockSink> (0);
        
        GenNetworkInterface<TerminalData>* GnI = Component :: GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
        MockSink* term = Component :: GetComponent<MockSink>(ms_cid);
        
        
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT, ms_cid, 
                          0, &MockSink::handle_incoming_lnkdt,1);
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::TERMINAL_PORT, ms_cid, 
                          2, &MockSink::handle_incoming_pkt,1);
                          
        //calculate the total length
	TerminalData dummy;
        //calculate the total length
        unsigned num_bytes = slen->get_simulated_len(&dummy) + HeadFlit::HEAD_FLIT_OVERHEAD;
	unsigned num_flits = num_bytes*8/i_p->linkWidth;
	if(num_bytes*8 % i_p->linkWidth != 0)
	    num_flits++;
           
        unsigned total = num_flits;
	if(num_flits > 1)
	    total++; //if body flit is needed, then tail is also needed.

        
        //test
        for (unsigned i = 0; i < (GnI->no_vcs + 1); i++)
        {
            TerminalData* td = new TerminalData;
            td->src = i;
            td->dest_id = i;
            GnI->handle_new_packet_event(GenNetworkInterface<TerminalData>::TERMINAL_PORT,td);
        }

	//tock must be called at falling edge
	Manifold::ScheduleClockHalf(1, MasterClock, &GenNetworkInterfaceTest::test_Interface_tick_0_call_tock, this,
	                            GnI, total);
        
	#if 0
        for (unsigned i = 0; i < GnI->no_vcs ; i++)
        {
            GnI->tock(); //call tock() to move packets from input buffer to proc_out_buffer.
	    if(i%2 == 0)
		CPPUNIT_ASSERT_EQUAL(total, GnI->proc_out_buffer[i].size());
	    else
		CPPUNIT_ASSERT_EQUAL(0, (int)GnI->proc_out_buffer[i].size());
        }
	#endif
        
        //call tick() once:
	//1. one flit should go from proc_out_buffer to corresponding router_out_buffer.
	Manifold::ScheduleClockHalf(1, MasterClock, &GenNetworkInterface<TerminalData>::tick, GnI);
	//GnI->tick();
        Manifold::unhalt();
        Manifold::StopAt(10);
        Manifold::Run();       

        for (unsigned i = 0; i < GnI->no_vcs ; i++)
        {
	    if(i%2 == 0)
		CPPUNIT_ASSERT_EQUAL(total-1, GnI->proc_out_buffer[i].size());
	    else
		CPPUNIT_ASSERT_EQUAL(0, (int)GnI->proc_out_buffer[i].size());
        }
	//2. One vc wins arbitration and moves 1 flit from router_out_buffer to router
        list<LinkData>* link_data = term->get_lnkdt();
	CPPUNIT_ASSERT_EQUAL(1, (int)link_data->size());
	unsigned winner = link_data->front().vc;

        for (unsigned i = 0; i < GnI->no_vcs ; i++)
        {
	    if(i%2 == 0) {
	        if(i == winner)
		    CPPUNIT_ASSERT_EQUAL(0, (int)GnI->router_out_buffer.get_occupancy(i));
		else
		    CPPUNIT_ASSERT_EQUAL(1, (int)GnI->router_out_buffer.get_occupancy(i));
	    }
	    else
		CPPUNIT_ASSERT_EQUAL(0, (int)GnI->proc_out_buffer[i].size());
        }
        
    }



    //======================================================================
    //======================================================================
    //! @brief Test tick() of generic interface
    //!
    //! Test the 2nd part of tick(). Each tick, a flit is moved from router_in_buffer to
    //! the corresponding proc_in_buffer.
    //!
    void test_Interface_tick_1()
    {
        //initialzing the interface
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
        
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>();
	MyVnet* vnet = new MyVnet();

	NIInit<TerminalData> init(mapping, slen, vnet);
        CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, 0, init, i_p);
        CompId_t ms_cid = Component :: Create<MockSink> (0);
        
        GenNetworkInterface<TerminalData>* GnI = Component :: GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
        //MockSink* term = Component :: GetComponent<MockSink>(ms_cid);
        
        
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT, ms_cid, 
                          0, &MockSink::handle_incoming_lnkdt,1);
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::TERMINAL_PORT, ms_cid, 
                          2, &MockSink::handle_incoming_pkt,1);
                          
        
        
        //puch a entire pkt at each router in buffer
        for (unsigned i = 0; i < i_p->num_vc; i++)
        {
	    {
		HeadFlit* f = new HeadFlit();
		f->type = HEAD;
		f->pkt_length = 3;
		GnI->router_in_buffer.push(i, f);
	    }

	    {
		BodyFlit* f = new BodyFlit();
		f->type = BODY;
		f->pkt_length = 3;
		GnI->router_in_buffer.push(i, f);
	    }

	    {
		TailFlit* f = new TailFlit();
		f->type = TAIL;
		f->pkt_length = 3;
		GnI->router_in_buffer.push(i, f);
	    }
        }
        

	//verify proc_in_buffer is empty before calling tick().
        for (unsigned i = 0; i < i_p->num_vc; i++)
            CPPUNIT_ASSERT_EQUAL(0, int(GnI->proc_in_buffer[i].size()));

	GnI->tick();
        
        //Manifold::unhalt();
        //Manifold::StopAt(10);
        //Manifold::Run();       
        
	//verify proc_in_buffer has 1 flit after calling tick().
        for (unsigned i = 0; i < i_p->num_vc; i++)
            CPPUNIT_ASSERT_EQUAL(1, int(GnI->proc_in_buffer[i].size()));
    }



    //======================================================================
    //======================================================================
    //! @brief Test tick() of generic interface
    //!
    //! Test the 2nd part of tick(). Each tick, a flit is moved from router_in_buffer to
    //! the corresponding proc_in_buffer.
    //!
    //! Put flits of entire packets in router_in_buffer. Call tick() n times, n = # of flits in
    //! a packet. The flits are now in proc_in_buffer. Call tick() again, 1 packet is moved from
    //! proc_in_buffer to output_pkt_buffer.
    //!
    void test_Interface_tick_1_1()
    {
        //initialzing the interface
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
        
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>();
	MyVnet* vnet = new MyVnet();

	NIInit<TerminalData> init(mapping, slen, vnet);
        CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, 0, init, i_p);
        CompId_t ms_cid = Component :: Create<MockSink> (0);
        
        GenNetworkInterface<TerminalData>* GnI = Component :: GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
        MockSink* term = Component :: GetComponent<MockSink>(ms_cid);
        
        
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT, ms_cid, 
                          0, &MockSink::handle_incoming_lnkdt,1);
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::TERMINAL_PORT, ms_cid, 
                          2, &MockSink::handle_incoming_pkt,1);
                          
        
        
        //puch a entire pkt at each router in buffer
        for (unsigned i = 0; i < i_p->num_vc; i++)
        {
	    {
		HeadFlit* f = new HeadFlit();
		f->type = HEAD;
		f->pkt_length = 3;
		GnI->router_in_buffer.push(i, f);
	    }

	    {
		BodyFlit* f = new BodyFlit();
		f->type = BODY;
		f->pkt_length = 3;
		GnI->router_in_buffer.push(i, f);
	    }

	    {
		TailFlit* f = new TailFlit();
		f->type = TAIL;
		f->pkt_length = 3;
		GnI->router_in_buffer.push(i, f);
	    }
        }
        

	//verify proc_in_buffer is empty before calling tick().
        for (unsigned i = 0; i < i_p->num_vc; i++)
            CPPUNIT_ASSERT_EQUAL(0, int(GnI->proc_in_buffer[i].size()));

	//Call tick 3 times; flits in router_in_buffer are moved to proce_in_buffer.
	for(int i=0; i<3; i++)
	    GnI->tick();
        
	//verify proc_in_buffer has 3 flits after calling tick().
        for (unsigned i = 0; i < i_p->num_vc; i++) {
            CPPUNIT_ASSERT_EQUAL(3, int(GnI->proc_in_buffer[i].size()));
            CPPUNIT_ASSERT_EQUAL(true, bool(GnI->proc_in_buffer[i].has_whole_packet()));
	}


	//call tick() again, a packet is moved from proc_in_buffer to output_pkt_buffer, then
	//out to the terminal. Meantime, a credit is sent back to router
	GnI->tick();

        Manifold::unhalt();
        Manifold::StopAt(10);
        Manifold::Run();       
        
	CPPUNIT_ASSERT_EQUAL(0, int(GnI->proc_in_buffer[0].size()));
        for (unsigned i = 1; i < i_p->num_vc; i++) {
            CPPUNIT_ASSERT_EQUAL(3, int(GnI->proc_in_buffer[i].size()));
	}

	CPPUNIT_ASSERT_EQUAL(1, int(term->get_pkts()->size()));
	CPPUNIT_ASSERT_EQUAL(1, int(term->get_lnkdt()->size()));
	CPPUNIT_ASSERT_EQUAL(int(CREDIT), int(term->get_lnkdt()->front().type));
	
    }



    //======================================================================
    //======================================================================
    //! @brief Test tock() of generic interface
    //!
    //! The tock(0 function moves packets from input_pkt_buffer to proc_out_buffer.
    //! Put packets in input_pkt_buffer, then call tock(), verify packets are moved
    //! as expected.
    void test_Interface_tock_0()
    {
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>();
	MyVnet* vnet = new MyVnet();

	NIInit<TerminalData> init(mapping, slen, vnet);

        CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, 0, init, i_p);
        CompId_t ms_cid = Component :: Create<MockSink> (0);
        
        GenNetworkInterface<TerminalData>* GnI = Component :: GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
        //MockSink* term = Component :: GetComponent<MockSink>(ms_cid);
        
        
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT, ms_cid, 
                          0, &MockSink::handle_incoming_lnkdt,1);
        Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::TERMINAL_PORT, ms_cid, 
                          2, &MockSink::handle_incoming_pkt,1);

        //calculate the expected number of flits.
        unsigned num_bytes = HeadFlit::HEAD_FLIT_OVERHEAD + sizeof(TerminalData);
        unsigned num_flits = num_bytes*8/i_p->linkWidth;
        if(num_bytes * 8 % i_p->linkWidth != 0)
           num_flits++;

        unsigned total_flits = num_flits;
	if(num_flits > 1)
	    total_flits++;
        
        //put packets in input_pkt_buffer
	const unsigned Npkts = GnI->no_vcs + 1;
        for (unsigned i = 0; i < Npkts; i++)
        {
            TerminalData* td = new TerminalData;
            GnI->handle_new_packet_event((int)GenNetworkInterface<TerminalData>::TERMINAL_PORT,td);
        }
        
        //see if the handle_new_packet_event put all pkt into the buffer
        CPPUNIT_ASSERT_EQUAL(Npkts, (unsigned)GnI->input_pkt_buffer.size());

        
        GnI->tock();

        
	int count = 0; //number of packets moved out of input_pkt_buffer.
        for (unsigned i = 0; i < GnI->no_vcs; i++)
        {
	    if(i % 2 == 0) {
		CPPUNIT_ASSERT_EQUAL(false, bool(GnI->is_proc_out_buffer_free[i]));
		CPPUNIT_ASSERT_EQUAL(total_flits, GnI->proc_out_buffer[i].size());
		count++;
	    }
        }
        CPPUNIT_ASSERT_EQUAL((int)Npkts-count, int(GnI->input_pkt_buffer.size()));
    }


    //======================================================================
    //======================================================================
    //! @brief Test handle_router() of generic interface
    //!
    //! Verify the process to handle incoming LinkData    
    void test_Interface_handle_router_0()
    {
        inf_init_params* i_p = new inf_init_params;
        i_p->num_vc = 4;
        i_p->linkWidth = 128;
        i_p->num_credits = 3;
	i_p->upstream_credits = 10;
	i_p->up_credit_msg_type = 123;
	i_p->upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	SimulatedLen<TerminalData>* slen = new SimulatedLen<TerminalData>();
	MyVnet* vnet = new MyVnet();

	NIInit<TerminalData> init(mapping, slen, vnet);

        CompId_t ni_cid = Component :: Create<GenNetworkInterface<TerminalData> > (0, 10, init, i_p);
        CompId_t lg_cid = Component :: Create<MockLinkDataGenerator> (0);
        CompId_t ms_cid = Component :: Create<MockSink> (0);
        
        GenNetworkInterface<TerminalData>* GnI = Component :: GetComponent<GenNetworkInterface<TerminalData> >(ni_cid);
        MockLinkDataGenerator* Mlg = Component :: GetComponent<MockLinkDataGenerator>(lg_cid);

	Manifold::Connect(ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT, ms_cid, 
                          1, &MockSink::handle_incoming_lnkdt,1);
        Manifold::Connect(lg_cid, 3, ni_cid, GenNetworkInterface<TerminalData>::ROUTER_PORT,
                          &GenNetworkInterface<TerminalData>::handle_router,1);
                          
        for (unsigned i = 0; i < i_p->num_vc; i++)
        {
	    GnI->downstream_credits[i] = 0; //manually set credits to 0

            for (unsigned j = 0; j < i_p->num_credits; j++)
            {
                Mlg->send_lnkdt_flit (i);
                Mlg->send_lnkdt_credit (i);
            }
        } 
        
        Manifold::unhalt();
        Manifold::StopAt(10);
        Manifold::Run();  
        
        for (unsigned i = 0; i < i_p->num_vc; i++)
        {
            CPPUNIT_ASSERT_EQUAL(i_p->num_credits, uint(GnI->router_in_buffer.get_occupancy(i))); 
            CPPUNIT_ASSERT_EQUAL(i_p->num_credits, uint(GnI->downstream_credits[i]));
        }
    }
    
    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("NnewIrisInterfaceTest");

	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_Interface_init_0", 
	                 &GenNetworkInterfaceTest::test_Interface_init_0));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_Interface_handle_new_packet_event_0", 	                       &GenNetworkInterfaceTest::test_Interface_handle_new_packet_event_0));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_to_flit_level_packet_0", &GenNetworkInterfaceTest::test_to_flit_level_packet_0));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_to_flit_level_packet_1", &GenNetworkInterfaceTest::test_to_flit_level_packet_1));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_to_flit_level_packet_1_1", &GenNetworkInterfaceTest::test_to_flit_level_packet_1_1));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_to_flit_level_packet_2", &GenNetworkInterfaceTest::test_to_flit_level_packet_2));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_to_flit_level_packet_2_1", &GenNetworkInterfaceTest::test_to_flit_level_packet_2_1));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_to_flit_level_packet_2_2", &GenNetworkInterfaceTest::test_to_flit_level_packet_2_2));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_Interface_tick_0", &GenNetworkInterfaceTest::test_Interface_tick_0));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_Interface_tick_1", &GenNetworkInterfaceTest::test_Interface_tick_1));  
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_Interface_tick_1_1", &GenNetworkInterfaceTest::test_Interface_tick_1_1));  
        mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_Interface_tock_0", &GenNetworkInterfaceTest::test_Interface_tock_0));
        mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_Interface_handle_router_0", &GenNetworkInterfaceTest::test_Interface_handle_router_0));
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


