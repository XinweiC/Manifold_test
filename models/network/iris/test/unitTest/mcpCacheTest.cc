//! This program tests the Coh_mem_req structure, which is sent by the
//! MCP-cache to the network, is correctly converted to flits and back.
#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "genericIrisInterface.h"

#include "kernel/manifold.h"
#include "mcp-cache/coh_mem_req.h"


using namespace manifold::kernel;
using namespace manifold::uarch;
using namespace std;

using namespace manifold::iris;
using namespace manifold::mcp_cache_namespace;

//####################################################################
// helper classes
//####################################################################

const int MY_COH_TYPE = 123;
const int MY_MEM_TYPE = 456;
const int MY_CREDIT_TYPE = 789;

class MySimLen : public manifold::iris::SimulatedLen<manifold::uarch::NetworkPacket> {
public:
    MySimLen(int cacheLineSize, int coh_msg_type, int mem_msg_type, int credit_msg_type) :
          m_cacheLineSize(cacheLineSize), m_COH(coh_msg_type), m_MEM(mem_msg_type), m_CREDIT(credit_msg_type)
    {}

    int get_simulated_len(manifold::uarch::NetworkPacket* pkt);
private:
    const int m_cacheLineSize;
    const int m_COH; //coherence message type
    const int m_MEM; //memory message type
    const int m_CREDIT; //credit message type
};


int MySimLen :: get_simulated_len(NetworkPacket* pkt)
{
    if(pkt->type == m_COH) { //coherence msg between MCP caches
	Coh_msg& msg = *((Coh_msg*)pkt->data);
	const int SZ_NO_DATA = 34; //32B addr + 2B overhead.
	const int SZ_WITH_DATA = 34 + m_cacheLineSize; //32B addr + 2B overhead + line

		switch(msg.msg) {
		    case MESI_CM_I_to_E:
		    case MESI_CM_I_to_S:
		    case MESI_CM_E_to_I:
		    case MESI_CM_M_to_I:
		    case MESI_CM_UNBLOCK_I:
			return SZ_NO_DATA;

		    case MESI_CM_UNBLOCK_I_DIRTY:
			return SZ_WITH_DATA;

		    case MESI_CM_UNBLOCK_E:
		    case MESI_CM_UNBLOCK_S:
		    case MESI_CM_CLEAN:
			return SZ_NO_DATA;

		    case MESI_CM_WRITEBACK:
		    case MESI_MC_GRANT_E_DATA:
		    case MESI_MC_GRANT_S_DATA:
			return SZ_WITH_DATA;

		    case MESI_MC_GRANT_I:
		    case MESI_MC_FWD_E:
		    case MESI_MC_FWD_S:
		    case MESI_MC_DEMAND_I:
			return SZ_NO_DATA;

		    case MESI_CC_E_DATA:
		    case MESI_CC_M_DATA:
		    case MESI_CC_S_DATA:
			return SZ_WITH_DATA;
		    default:
			assert(0);
			break;
		}
    }
    else if(pkt->type == m_MEM) { //memory msg between cache and MC
		const int SZ_NO_DATA = 36; //32B addr + 4B overhead.
		const int SZ_WITH_DATA = 36 + m_cacheLineSize; //32B addr + 4B overhead + line

		Mem_msg& msg = *((Mem_msg*)pkt->data);

		if(msg.type == Mem_msg :: MEM_REQ) {
		    if(msg.is_read()) //LOAD request
			return SZ_NO_DATA;
		    else
			return SZ_WITH_DATA;
		}
		else if(msg.type == Mem_msg :: MEM_RPLY) {
		    return SZ_WITH_DATA;
		}
		else {
		    assert(0);
		}
    }
    else {
	assert(0);
    }

}


class MyVnet : public manifold::iris::VnetAssign<manifold::uarch::NetworkPacket> {
public:
    MyVnet(int coh_msg_type, int mem_msg_type, int credit_msg_type) :
          m_COH(coh_msg_type), m_MEM(mem_msg_type), m_CREDIT(credit_msg_type)
    {}

    int get_virtual_net(manifold::uarch::NetworkPacket* pkt);

private:
    const int m_COH; //coherence message type
    const int m_MEM; //memory message type
    const int m_CREDIT; //credit message type
};


int MyVnet :: get_virtual_net(NetworkPacket* pkt)
{
    if(pkt->type == m_COH) { //coherence msg between MCP caches
	Coh_msg& msg = *((Coh_msg*)pkt->data);
        if(msg.type == Coh_msg :: COH_REQ)
            return 0;
        else if(msg.type == Coh_msg :: COH_RPLY)
            return 1;
        else {
            assert(0);
        }
    }
    else if(pkt->type == m_MEM) { //memory msg between cache and MC
	Mem_msg& msg = *((Mem_msg*)pkt->data);

	if(msg.type == Mem_msg :: MEM_REQ) {
            return 0;
	}
	else if(msg.type == Mem_msg :: MEM_RPLY) {
            return 1;
	}
	else {
	    assert(0);
	}
    }
    else {
	assert(0);
    }
}


//####################################################################
//####################################################################
class GenNetworkInterfaceTest : public CppUnit::TestFixture {
private:

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };
    
    //======================================================================
    //======================================================================
    //! @brief Test to_flit_level_packet() and from_flit_level_packet() with Coh_msg.
    //!
    //! For each MESI coherence message, call to_flit_level_packet() to create a flit level packet. Verify the
    //! number of body flits created is as expected, and the packet is correctly copied into the body flits.
    //! Call from_flit_level_packet() to verify the original packet can be reconstructed
    //! from the flit level packet.
    void test_to_flit_level_packet_0()
    {
        inf_init_params i_p;
        i_p.num_vc = 4;
        i_p.linkWidth = 128;
        i_p.num_credits = 3;
	i_p.upstream_credits = 10;
	i_p.up_credit_msg_type = MY_CREDIT_TYPE;
	i_p.upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	const unsigned IF_ID = random() % 1024; //interface ID

	const int CacheLine = (0x1 << ( random() % 3 + 4)); // randomly pick 16, 32, or 64
	MySimLen* slen = new MySimLen(CacheLine, MY_COH_TYPE, MY_MEM_TYPE, MY_CREDIT_TYPE);
	MyVnet* vnet = new MyVnet(MY_COH_TYPE, MY_MEM_TYPE, MY_CREDIT_TYPE);

	NIInit<NetworkPacket> init(mapping,slen,vnet);
        GenNetworkInterface<NetworkPacket>* GnI = new GenNetworkInterface<NetworkPacket>(IF_ID, init, &i_p);

        for(int mt=0; mt<19; mt++) { //19 msg types in total
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = MY_COH_TYPE;

	    Coh_msg& msg = *((Coh_msg*)(pkt->data));
	    msg.addr = random();
	    msg.forward_id = random() % 1024;
	    msg.msg = mt;
	    msg.rw = random() % 2;

	    pkt->src = random() % 1024;
	    pkt->src_port = random() % 1024;
	    pkt->dst = random() % 1024;
	    pkt->dst_port = random() % 1024;


	    unsigned n_bytes = HeadFlit::HEAD_FLIT_OVERHEAD + slen->get_simulated_len(pkt);
	    unsigned n_flits = n_bytes * 8 / i_p.linkWidth;
	    if(n_bytes * 8 % i_p.linkWidth != 0)
		n_flits++;

            unsigned total = n_flits;
            if(n_flits > 1) //n_flits is the number of Head and Body flits; if more than 1, then a Tail is needed.
	        total++;


	    FlitLevelPacket* flp = new FlitLevelPacket;
	    GnI->to_flit_level_packet(flp, pkt, 0);

	    //verify the number of body flits is as expected.
	    CPPUNIT_ASSERT_EQUAL(total, flp->size()); //one head and one tail.

	    //see if the dest and src id passed correctly
	    CPPUNIT_ASSERT_EQUAL(mapping->terminal_to_net(pkt->dst), (int)flp->dst_id);
	    CPPUNIT_ASSERT_EQUAL(IF_ID, flp->src_id); //FlipLevelPacket's src_node must be the interface id.

        
	    //verify the flits are generated correctly
	    for (unsigned i = 0; i < flp->pkt_length; i++) {
		if (i == 0)
		   CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, HEAD);
		else if (i == (flp->pkt_length - 1))
		   CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, TAIL);
		else
		   CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, BODY);
	    }

	    //verify data in the flits are correct
	    char buf[sizeof(NetworkPacket)];

	    int count = 0;

	    for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
	       if(count < sizeof(NetworkPacket)) {
		   buf[count] = static_cast<HeadFlit*>(flp->flits[0])->data[j];
		   count++;
	       }
	       else
		   break;
	    }

	    for(int i=0; i<n_flits-1; i++) {
	       for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
		   if(count < sizeof(NetworkPacket)) {
		       buf[count] = static_cast<BodyFlit*>(flp->flits[i+1])->data[j];
		       count++;
		   }
		   else
		       break;
	       }
	       if(count >= sizeof(NetworkPacket))
		   break;
	    }

	    //reconstruct a Coh_msg object from the body flits.
	    NetworkPacket* recv_pkt1 = (NetworkPacket*)buf;
	    Coh_msg* td1 = (Coh_msg*)(recv_pkt1->data);

	    //also test from_flit_level_packet
	    NetworkPacket* recv_pkt2 = GnI->from_flit_level_packet(flp);
	    Coh_msg* td2 = (Coh_msg*)(recv_pkt2->data);

	    CPPUNIT_ASSERT_EQUAL(pkt->src, recv_pkt1->src);
	    CPPUNIT_ASSERT_EQUAL(pkt->src_port, recv_pkt1->src_port);
	    CPPUNIT_ASSERT_EQUAL(pkt->dst, recv_pkt1->dst);
	    CPPUNIT_ASSERT_EQUAL(pkt->dst_port, recv_pkt1->dst_port);

	    CPPUNIT_ASSERT_EQUAL(pkt->src, recv_pkt2->src);
	    CPPUNIT_ASSERT_EQUAL(pkt->src_port, recv_pkt2->src_port);
	    CPPUNIT_ASSERT_EQUAL(pkt->dst, recv_pkt2->dst);
	    CPPUNIT_ASSERT_EQUAL(pkt->dst_port, recv_pkt2->dst_port);


	    CPPUNIT_ASSERT_EQUAL(msg.forward_id, td1->forward_id);
	    CPPUNIT_ASSERT_EQUAL(msg.addr, td1->addr);
	    CPPUNIT_ASSERT_EQUAL(msg.msg, td1->msg);
	    CPPUNIT_ASSERT_EQUAL(msg.rw, td1->rw);

	    CPPUNIT_ASSERT_EQUAL(msg.addr, td2->addr);
	    CPPUNIT_ASSERT_EQUAL(msg.forward_id, td2->forward_id);
	    CPPUNIT_ASSERT_EQUAL(msg.msg, td2->msg);
	    CPPUNIT_ASSERT_EQUAL(msg.rw, td2->rw);

	    delete pkt;
	    delete recv_pkt2;
	    delete flp;
	}//for

	delete mapping;
	delete slen;
	delete GnI;
    }



    //======================================================================
    //======================================================================
    //! @brief Test to_flit_level_packet() and from_flit_level_packet() with Mem_msg.
    //!
    //! Test the following cases:
    //! - Read request
    //! - Write request
    //! - Read reply
    //!
    //! For each case, call to_flit_level_packet() to create a flit level packet. Verify the
    //! number of body flits created is as expected, and the packet is correctly copied into the body flits.
    //! Call from_flit_level_packet() to verify the original packet can be reconstructed
    //! from the flit level packet.
    void test_to_flit_level_packet_1()
    {
        inf_init_params i_p;
        i_p.num_vc = 4;
        i_p.linkWidth = 128;
        i_p.num_credits = 3;
	i_p.upstream_credits = 10;
	i_p.up_credit_msg_type = MY_CREDIT_TYPE;
	i_p.upstream_buffer_size = 5;
    
	Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
	const unsigned IF_ID = random() % 1024; //interface ID

	const int CacheLine = (0x1 << ( random() % 3 + 4)); // randomly pick 16, 32, or 64
	MySimLen* slen = new MySimLen(CacheLine, MY_COH_TYPE, MY_MEM_TYPE, MY_CREDIT_TYPE);
	MyVnet* vnet = new MyVnet(MY_COH_TYPE, MY_MEM_TYPE, MY_CREDIT_TYPE);

	NIInit<NetworkPacket> init(mapping,slen,vnet);
        GenNetworkInterface<NetworkPacket>* GnI = new GenNetworkInterface<NetworkPacket>(IF_ID, init, &i_p);

        for(int i=0; i<3; i++) {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = MY_MEM_TYPE;
	    pkt->src = random() % 1024;
	    pkt->src_port = random() % 1024;
	    pkt->dst = random() % 1024;
	    pkt->dst_port = random() % 1024;

	    Mem_msg& msg = *((Mem_msg*)(pkt->data));
	    msg.addr = random();

            switch(i) {
	        case 0:
		    msg.type = Mem_msg :: MEM_REQ;
		    msg.op_type = OpMemLd;
		    break;
	        case 1:
		    msg.type = Mem_msg :: MEM_REQ;
		    msg.op_type = OpMemSt;
		    break;
	        case 2:
		    msg.type = Mem_msg :: MEM_RPLY;
		    msg.op_type = OpMemLd;
		    break;
		default:
		    assert(0);
		    break;
	    }



	    unsigned n_bytes = HeadFlit::HEAD_FLIT_OVERHEAD + slen->get_simulated_len(pkt);
	    unsigned n_flits = n_bytes * 8 / i_p.linkWidth;
	    if(n_bytes * 8 % i_p.linkWidth != 0)
		n_flits++;

            unsigned total = n_flits;
            if(n_flits > 1) //n_flits is the number of Head and Body flits; if more than 1, then a Tail is needed.
	        total++;

	    FlitLevelPacket* flp = new FlitLevelPacket;
	    GnI->to_flit_level_packet(flp, pkt, 0);

	    //verify the number of body flits is as expected.
	    CPPUNIT_ASSERT_EQUAL(total, flp->size()); //one head and one tail.

	    //see if the dest and src id passed correctly
	    CPPUNIT_ASSERT_EQUAL(mapping->terminal_to_net(pkt->dst), (int)flp->dst_id);
	    CPPUNIT_ASSERT_EQUAL(IF_ID, flp->src_id); //FlipLevelPacket's src_node must be the interface id.

        
	    //verify the flits are generated correctly
	    for (unsigned i = 0; i < flp->pkt_length; i++) {
		if (i == 0)
		   CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, HEAD);
		else if (i == (flp->pkt_length - 1))
		   CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, TAIL);
		else
		   CPPUNIT_ASSERT_EQUAL(flp->flits[i]->type, BODY);
	    }

	    //verify data in the flits are correct
	    char buf[sizeof(NetworkPacket)];

	    int count = 0;

	    for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
	       if(count < sizeof(NetworkPacket)) {
		   buf[count] = static_cast<HeadFlit*>(flp->flits[0])->data[j];
		   count++;
	       }
	       else
		   break;
	    }

	    for(int i=0; i<n_flits-1; i++) {
	       for(int j=0; j<HeadFlit::MAX_DATA_SIZE; j++) {
		   if(count < sizeof(NetworkPacket)) {
		       buf[count] = static_cast<BodyFlit*>(flp->flits[i+1])->data[j];
		       count++;
		   }
		   else
		       break;
	       }
	       if(count >= sizeof(NetworkPacket))
		   break;
	    }

	    //reconstruct a Coh_msg object from the body flits.
	    NetworkPacket* recv_pkt1 = (NetworkPacket*)buf;
	    Mem_msg* td1 = (Mem_msg*)(recv_pkt1->data);

	    //also test from_flit_level_packet
	    NetworkPacket* recv_pkt2 = GnI->from_flit_level_packet(flp);
	    Mem_msg* td2 = (Mem_msg*)(recv_pkt2->data);

	    CPPUNIT_ASSERT_EQUAL(pkt->src, recv_pkt1->src);
	    CPPUNIT_ASSERT_EQUAL(pkt->src_port, recv_pkt1->src_port);
	    CPPUNIT_ASSERT_EQUAL(pkt->dst, recv_pkt1->dst);
	    CPPUNIT_ASSERT_EQUAL(pkt->dst_port, recv_pkt1->dst_port);

	    CPPUNIT_ASSERT_EQUAL(pkt->src, recv_pkt2->src);
	    CPPUNIT_ASSERT_EQUAL(pkt->src_port, recv_pkt2->src_port);
	    CPPUNIT_ASSERT_EQUAL(pkt->dst, recv_pkt2->dst);
	    CPPUNIT_ASSERT_EQUAL(pkt->dst_port, recv_pkt2->dst_port);


	    CPPUNIT_ASSERT_EQUAL(msg.addr, td1->addr);
	    CPPUNIT_ASSERT_EQUAL(msg.op_type, td1->op_type);

	    CPPUNIT_ASSERT_EQUAL(msg.addr, td2->addr);
	    CPPUNIT_ASSERT_EQUAL(msg.op_type, td2->op_type);

	    delete pkt;
	    delete recv_pkt2;
	    delete flp;
	}//for

	delete mapping;
	delete slen;
	delete GnI;
    }






    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("NnewIrisInterfaceTest");

	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_to_flit_level_packet_0", &GenNetworkInterfaceTest::test_to_flit_level_packet_0));
	mySuite->addTest(new CppUnit::TestCaller<GenNetworkInterfaceTest>("test_to_flit_level_packet_1", &GenNetworkInterfaceTest::test_to_flit_level_packet_1));
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


