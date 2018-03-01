// ###############################################################################
// To do:
//
// - read miss; manager in E; owner in M
// ###############################################################################




//! This program tests MESI_L2_cache as a component. We use a Mock
//! component and connect the MESI_L2_cache to the component.
//!
//!
//!       -----------         -----------         -----------
//!      | processor |       | processor |       | processor |
//!       -----------         -----------         -----------
//!            |                   |                   |
//!     ---------------     ---------------     ---------------
//!    | MESI_L1_cache |   | MESI_L1_cache |   | MESI_L1_cache |
//!     ---------------     ---------------     ---------------
//!            |                   |                   |
//!          ---------------------------------------------      -----
//!         |                                             |----| MC1 |
//!         |                                             |     -----  
//!         |                   Network                   |
//!         |                                             |     -----  
//!         |                                             |----| MC2 |
//!          ---------------------------------------------      -----
//!                       |                    |
//!                 ---------------     ---------------
//!                | MESI_L2_cache |   | MESI_L2_cache |
//!                 ---------------     ---------------
//!

#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <list>
#include <algorithm>
#include "MESI_L1_cache.h"
#include "MESI_L2_cache.h"
#include "coherence/MESI_client.h"
#include "coherence/MESI_manager.h"

#include "kernel/manifold-decl.h"
#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace std;
using namespace manifold::kernel;
using namespace manifold::uarch;
using namespace manifold::mcp_cache_namespace;

//####################################################################
// helper classes
//####################################################################

static const int NODEID_MC1 = 0;
static const int NODEID_MC2 = 1;
static const int NODEID_L1_1 = 2;
static const int NODEID_L1_2 = 3;
static const int NODEID_L1_3 = 4;
static const int NODEID_L2_1 = 5;
static const int NODEID_L2_2 = 6;

const int COH_TYPE= 123;
const int MEM_TYPE= 456;
const int CREDIT_TYPE = 789;


struct Proc_req {
    enum {LOAD, STORE};
    Proc_req(int rid, int nid, unsigned long ad, int t) :
        req_id(rid), addr(ad), type(t)
	{}
    unsigned long get_addr() { return addr; }
    bool is_read() { return type == LOAD; }

    int req_id;
    unsigned long addr;
    int type;
};


//! This class emulates an L1 and a memory controller.
class MockProc : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};

    void send_req(Proc_req* req)
    {
	Send(OUT, req);
    }

    void handle_incoming(int, Proc_req* msg)
    {
    //cout << "MockProc handle_incoming @ " << Manifold::NowTicks() << endl;
	m_cache_resps.push_back(*msg);
	delete msg;
        m_ticks.push_back(Manifold :: NowTicks());
    }

    vector<Proc_req>& get_cache_resps() { return m_cache_resps; }
    vector<Ticks_t>& get_ticks() { return m_ticks; }
private:
    vector<Proc_req> m_cache_resps;
    vector<Ticks_t> m_ticks;
};



class MockMc : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};

    MockMc(int nid) : m_nid(nid) {}

    void handle_incoming(int, NetworkPacket* pkt)
    {
        if(pkt->type == L2_cache :: MEM_MSG) {
//cout << "MC got MEM req\n";
	    Mem_msg* mem = (Mem_msg*)(pkt->data);
	    m_reqs.push_back(*mem);
	}
        else if(pkt->type == L2_cache :: CREDIT_MSG) {
	    //ignore
	    delete pkt;
	    return;
	}
	else {
	    assert(0);
	}

	pkt->dst = pkt->src;
	pkt->dst_port = pkt->src_port;
	pkt->src = m_nid;

	Send(OUT, pkt);
    }

    vector<Mem_msg>& get_reqs() { return m_reqs; }
    vector<Ticks_t>& get_ticks() { return m_ticks; }

private:
    const int m_nid; //node id
    vector<Mem_msg> m_reqs;
    vector<Ticks_t> m_ticks;
};




//! This mapping maps an address to an L2's node ID based on page.
class MyL2Map : public DestMap {
public:
    MyL2Map(vector<int>& nodes, int page_offset_bits) : m_page_offset_bits(page_offset_bits)
    {
	assert(nodes.size() > 0);

	for(unsigned i=0; i<nodes.size(); i++)
	    m_nodes.push_back(nodes[i]);

	int bits_for_mask = myLog2(nodes.size());

        m_selector_mask = (0x1 << bits_for_mask) - 1;
    }

    int lookup(paddr_t addr)
    {
        return m_nodes[m_selector_mask & (addr >> m_page_offset_bits)];
    }

    int get_nodes() { return m_nodes.size(); }

private:

    static int myLog2(unsigned num)
    {
	assert(num > 0);

	int bits = 0;
	while(((unsigned)0x1 << bits) < num) {
	    bits++;
	}
	return bits;
    }

    vector<int> m_nodes; //L2 node ids
    const int m_page_offset_bits;
    paddr_t m_selector_mask;
};


//! This mapping maps any address to the same id, which is initialized
//! in construction.
class MyMcMap1 : public DestMap {
public:
    MyMcMap1(int x) : m_mcid(x) {}
    int lookup(paddr_t addr) { return m_mcid; }
private:
    const int m_mcid;
};




class MockNet : public manifold::kernel::Component {
public:
    void add_node_id_to_port_map(int node_id, int port)
    {
	//node_id hasn't been put in the map
        assert(m_map_node_id_to_port.find(node_id) == m_map_node_id_to_port.end());
	//port hasn't been assigned
	for(map<int, int>::iterator it = m_map_node_id_to_port.begin(); it != m_map_node_id_to_port.end(); ++it)
	    assert((*it).second != port);

	m_map_node_id_to_port[node_id] = port;
    }

    void handle_incoming(int port, NetworkPacket* pkt)
    {
        if(pkt->type == CREDIT_TYPE) {
	    //cout << "network got credit from port " << port << endl;
	    m_recv_credits[port]++;
	    return;
	}
//cout << "Network: " << pkt->src << " --> " << pkt->dst << endl;

	NetworkPacket* credit = new NetworkPacket;
	credit->type = CREDIT_TYPE;
	credit->dst = pkt->src;
	Send(port, credit);
	m_send_credits[port]++;

	Send(m_map_node_id_to_port[pkt->dst], pkt);
    }

    int map_node_id_to_port(int node) { return m_map_node_id_to_port[node]; }

    map<int,int>& get_recv_credit_counts() { return m_recv_credits; }
    map<int,int>& get_send_credit_counts() { return m_send_credits; }

private:
    map<int, int> m_map_node_id_to_port; //maps the node id to port number
    map<int, int> m_recv_credits; //count number of credits received.
    map<int, int> m_send_credits; //count number of credits received.
};




//####################################################################
//####################################################################
class MESI_L1_L2_cacheTest : public CppUnit::TestFixture {
private:
    static const unsigned L1_MSHR_SIZE = 8;
    static const unsigned L2_MSHR_SIZE = 8;

    MockProc* m_procp1;
    MockProc* m_procp2;
    MockProc* m_procp3;
    MESI_L1_cache* m_l1p1;
    MESI_L1_cache* m_l1p2;
    MESI_L1_cache* m_l1p3;
    MESI_L2_cache* m_l2p1;
    MESI_L2_cache* m_l2p2;
    MockMc* m_mcp1;
    MockMc* m_mcp2;
    MockNet* m_netp;

    CompId_t m_procId1;
    CompId_t m_procId2;
    CompId_t m_procId3;
    CompId_t m_l1Id1;
    CompId_t m_l1Id2;
    CompId_t m_l1Id3;
    CompId_t m_l2Id1;
    CompId_t m_l2Id2;
    CompId_t m_mcId1;
    CompId_t m_mcId2;
    CompId_t m_netId;

    MyL2Map* m_l2map;

    static const int PAGE_BITS = 12; //page offset bits

public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
	cache_settings l1_parameters;
	l1_parameters.name = "testL1";
	//m_settings.type = CACHE_DATA;
	l1_parameters.size = 0x1 << 10; //1K
	l1_parameters.assoc = 2;
	l1_parameters.block_size = 32;
	l1_parameters.hit_time = 2;
	l1_parameters.lookup_time = 3;
	l1_parameters.replacement_policy = RP_LRU;

	cache_settings l2_parameters;
	l2_parameters.name = "testL2";
	//m_settings.type = CACHE_DATA;
	l2_parameters.size = 0x1 << 12; //4K
	l2_parameters.assoc = 4;
	l2_parameters.block_size = 32;
	l2_parameters.hit_time = 2;
	l2_parameters.lookup_time = 3;
	l2_parameters.replacement_policy = RP_LRU;

	vector<int> l2_nodes;
	l2_nodes.push_back(NODEID_L2_1);
	l2_nodes.push_back(NODEID_L2_2);
	m_l2map = new MyL2Map(l2_nodes, PAGE_BITS); //maps an address to an L2 node ID.

	MyMcMap1* mcmap1 = new MyMcMap1(NODEID_MC1);
	MyMcMap1* mcmap2 = new MyMcMap1(NODEID_MC2);

        //create proc
	m_procId1 = Component :: Create<MockProc>(0);
	m_procp1 = Component::GetComponent<MockProc>(m_procId1);
	m_procId2 = Component :: Create<MockProc>(0);
	m_procp2 = Component::GetComponent<MockProc>(m_procId2);
	m_procId3 = Component :: Create<MockProc>(0);
	m_procp3 = Component::GetComponent<MockProc>(m_procId3);

        //create the MESI_L1_cache.
	L1_cache :: Set_msg_types(COH_TYPE, CREDIT_TYPE);
	L2_cache :: Set_msg_types(COH_TYPE, MEM_TYPE, CREDIT_TYPE);

	L1_cache_settings l1_settings;
	l1_settings.l2_map = m_l2map;
	l1_settings.mshr_sz = L1_MSHR_SIZE;
	l1_settings.downstream_credits = 10;

	m_l1Id1 = Component :: Create<MESI_L1_cache>(0, NODEID_L1_1, l1_parameters, l1_settings);
	m_l1p1 = Component::GetComponent<MESI_L1_cache>(m_l1Id1);
	m_l1Id2 = Component :: Create<MESI_L1_cache>(0, NODEID_L1_2, l1_parameters, l1_settings);
	m_l1p2 = Component::GetComponent<MESI_L1_cache>(m_l1Id2);
	m_l1Id3 = Component :: Create<MESI_L1_cache>(0, NODEID_L1_3, l1_parameters, l1_settings);
	m_l1p3 = Component::GetComponent<MESI_L1_cache>(m_l1Id3);

        //create the MESI_L2_cache.
	L2_cache_settings l2_settings_1, l2_settings_2;
	l2_settings_1.mc_map = mcmap1;
	l2_settings_1.mshr_sz = L2_MSHR_SIZE;
	l2_settings_1.downstream_credits = 10;

	l2_settings_2.mc_map = mcmap2;
	l2_settings_2.mshr_sz = L2_MSHR_SIZE;
	l2_settings_2.downstream_credits = 10;

	m_l2Id1 = Component :: Create<MESI_L2_cache>(0, NODEID_L2_1, l2_parameters, l2_settings_1); //first L2 always sends to MC1
	m_l2p1 = Component::GetComponent<MESI_L2_cache>(m_l2Id1);
	m_l2Id2 = Component :: Create<MESI_L2_cache>(0, NODEID_L2_2, l2_parameters, l2_settings_2); //2nd L2 always sends to MC2
	m_l2p2 = Component::GetComponent<MESI_L2_cache>(m_l2Id2);

	//network
	m_netId = Component :: Create<MockNet>(0);
	m_netp = Component::GetComponent<MockNet>(m_netId);

	//2 MCs
	m_mcId1 = Component :: Create<MockMc>(0, int(NODEID_MC1));
	m_mcp1= Component::GetComponent<MockMc>(m_mcId1);
	m_mcId2 = Component :: Create<MockMc>(0, int(NODEID_MC2));
	m_mcp2 = Component::GetComponent<MockMc>(m_mcId2);


        //connect the components (proc and L1)
	Manifold::Connect(m_procId1, MockProc::OUT, &MockProc::handle_incoming,
	                  m_l1Id1, MESI_L1_cache::PORT_PROC, &MESI_L1_cache::handle_processor_request<Proc_req>,
			  MasterClock, MasterClock, 1, 1);

	Manifold::Connect(m_procId2, MockProc::OUT, &MockProc::handle_incoming,
	                  m_l1Id2, MESI_L1_cache::PORT_PROC, &MESI_L1_cache::handle_processor_request<Proc_req>,
			  MasterClock, MasterClock, 1, 1);

	Manifold::Connect(m_procId3, MockProc::OUT, &MockProc::handle_incoming,
	                  m_l1Id3, MESI_L1_cache::PORT_PROC, &MESI_L1_cache::handle_processor_request<Proc_req>,
			  MasterClock, MasterClock, 1, 1);

        //connect the components to the network
	//first L1
	const int NET_L1_1_PORT = 0;
	Manifold::Connect(m_l1Id1, MESI_L1_cache::PORT_L2, &MESI_L1_cache::handle_peer_and_manager_request,
	                  m_netId, NET_L1_1_PORT, &MockNet::handle_incoming,
			  MasterClock, MasterClock, 1, 1);
	//second L1
	const int NET_L1_2_PORT = 1;
	Manifold::Connect(m_l1Id2, MESI_L1_cache::PORT_L2, &MESI_L1_cache::handle_peer_and_manager_request,
	                  m_netId, NET_L1_2_PORT, &MockNet::handle_incoming,
			  MasterClock, MasterClock, 1, 1);
	//third L1
	const int NET_L1_3_PORT = 2;
	Manifold::Connect(m_l1Id3, MESI_L1_cache::PORT_L2, &MESI_L1_cache::handle_peer_and_manager_request,
	                  m_netId, NET_L1_3_PORT, &MockNet::handle_incoming,
			  MasterClock, MasterClock, 1, 1);
	//first L2
	const int NET_L2_1_PORT = 3;
	Manifold::Connect(m_l2Id1, MESI_L2_cache::PORT_L1, &MESI_L2_cache::handle_incoming<Mem_msg>,
	                  m_netId, NET_L2_1_PORT, &MockNet::handle_incoming,
			  MasterClock, MasterClock, 1, 1);
	//second L2
	const int NET_L2_2_PORT = 4;
	Manifold::Connect(m_l2Id2, MESI_L2_cache::PORT_L1, &MESI_L2_cache::handle_incoming<Mem_msg>,
	                  m_netId, NET_L2_2_PORT, &MockNet::handle_incoming,
			  MasterClock, MasterClock, 1, 1);
	//MC
	const int NET_MC1_PORT = 5;
	Manifold::Connect(m_mcId1, MockMc::OUT, &MockMc::handle_incoming,
	                  m_netId, NET_MC1_PORT, &MockNet::handle_incoming,
			  MasterClock, MasterClock, 1, 1);
	//MC
	const int NET_MC2_PORT = 6;
	Manifold::Connect(m_mcId2, MockMc::OUT, &MockMc::handle_incoming,
	                  m_netId, NET_MC2_PORT, &MockNet::handle_incoming,
			  MasterClock, MasterClock, 1, 1);


	Clock::Register(MasterClock, (L1_cache*)m_l1p1, &L1_cache::tick, (void(L1_cache::*)(void))0);
	Clock::Register(MasterClock, (L1_cache*)m_l1p2, &L1_cache::tick, (void(L1_cache::*)(void))0);
	Clock::Register(MasterClock, (L1_cache*)m_l1p3, &L1_cache::tick, (void(L1_cache::*)(void))0);

        m_netp->add_node_id_to_port_map(NODEID_MC1, NET_MC1_PORT);
        m_netp->add_node_id_to_port_map(NODEID_MC2, NET_MC2_PORT);
        m_netp->add_node_id_to_port_map(NODEID_L1_1, NET_L1_1_PORT);
        m_netp->add_node_id_to_port_map(NODEID_L1_2, NET_L1_2_PORT);
        m_netp->add_node_id_to_port_map(NODEID_L1_3, NET_L1_3_PORT);
        m_netp->add_node_id_to_port_map(NODEID_L2_1, NET_L2_1_PORT);
        m_netp->add_node_id_to_port_map(NODEID_L2_2, NET_L2_2_PORT);
    }






    //======================================================================
    //======================================================================
    //! @brief Load miss in L1; manager in I.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager; client enters IE.
    //! If it's also miss in L2, L2 sends a request to memory controller first;
    //! after the response from memory, L2 manager sends MESI_MC_GRANT_E_DATA to
    //! client; manager enters IE. Client enters E, sends MESI_CM_UNBLOCK_E;
    //! manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->mem
    //! 3. M<---mem
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E; C--->proc, Proc_req
    //!
    //! Send a LOAD to first L1, since this is the very first LOAD, it will
    //! miss in both L1 and L2.
    void test_load_l1_I_l2_I_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);



        //verify proc receives a response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp.msg);


        //verify mc receives a request
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);
    }





    //======================================================================
    //======================================================================
    //! @brief Load miss in L1; manager in E.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager; client enters IE.
    //! If manager is in E, L2 sends MESI_MC_FWD_S to owner; it adds both owner and
    //! client to sharer list and clears owner; manager enters ES.
    //! Owner sends MESI_CC_S_DATA to client and enters S. Client sends MESI_CM_UNBLOCK_S
    //! to manager and enters S. Manager enters S state.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    //! 3. C<---owner, MESI_CC_S_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_S; C--->proc, Proc_req
    //!
    //! Send a LOAD to one of the L1s; both L1 and L2 enter E state. Send LOAD to same
    //! address from a different L1; both L1s and L2 enter S state.
    void test_load_l1_I_l2_E_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc2 to send cache request
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req2);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client1->state);


	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client2->state);



        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in S state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_S, manager->state);
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_1));
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_2));




        //verify proc2 receives a response
        vector<Proc_req>& cresps = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID2, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }




    //======================================================================
    //======================================================================
    //! @brief Load miss in L1; manager in E; owner in M.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager; client enters IE.
    //! If manager is in E state, it sends a MESI_MC_FWD_S to owner; it adds owner and
    //! client to sharers list and resets owner; manager enters ES.
    //! Owner sends MESI_CC_S_DATA to client, and MESI_CM_WRITEBACK to mangager;
    //! after the response from memory, L2 manager sends MESI_MC_GRANT_E_DATA to
    //! client; manager enters IE. Client enters E, eventually M, sends MESI_CM_UNBLOCK_E;
    //! manager enters E state (manager doesn't have M state).
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S; manager ES
    //! 3. owner-->M, MESI_CM_WRITEBACK
    //! 4. owner-->C, MESI_CC_S_DATA; owner S
    //! 4. C<---M, MESI_MC_GRANT_S_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_S; C--->proc, Proc_req
    //!
    //! Send a STORE to one of the L1s; client enters M state and manager enters E.
    //! Send a LOAD to the same address from another proc; it would miss in L1.
    void test_load_l1_I_owner_M_0()
    {
	//create a STORE request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc2 to send cache request
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req2);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client->state);

	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in S state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_S, manager->state);
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(2, manager->sharersList.count());
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_1));
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_2));



        //verify proc receives a response
        vector<Proc_req>& cresps = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID2, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp.msg);


        //verify mc receives a request
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);
    }






    //======================================================================
    //======================================================================
    //! @brief Load miss in L1; manager in S.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager; client enters IE.
    //! If manager is in S, it sends MESI_MC_GRANT_S_DATA to clinet; it adds
    //! client to sharer list; manager enters SS.
    //! Client sends MESI_CM_UNBLOCK_S to manager and enters S. Manager enters S state.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. C<---M, MESI_MC_GRANT_S_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_S; C--->proc, Proc_req
    //!
    //! Send a LOAD to 2 L1s with same address. This would bring the 2 clients and the manager
    //! to the S state. Sends LOAD with dame address from a 3rd proc; all 3 clients should
    //! be in S state.
    void test_load_l1_I_l2_S_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc2 to send cache request to put the line in S state.
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req2);
	When += 50;

	//schedule for the proc3 to send cache request
	const int REQ_ID3 = random();
	Proc_req* req3 = new Proc_req(REQ_ID3, NODEID_L1_3, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp3, req3);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client1->state);


	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client2->state);


	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p3->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p3->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p3->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client3 = dynamic_cast<MESI_client*>(m_l1p3->clients[m_l1p3->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client3->state);



        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in S state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_S, manager->state);
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_1));
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_2));
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_3));



        //verify proc3 receives a response
        vector<Proc_req>& cresps = m_procp3->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID3, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }




    //======================================================================
    //======================================================================
    //! @brief Load hit in L1; manager in E; client in E.
    //!
    //! In a LOAD hit, no message is sent; client and manager states are unchanged.
    //!
    //! Send a LOAD to one of the L1s; both L1 and L2 enter E state. Send LOAD to same
    //! address from same proc; both client and manager stay unchanged.
    void test_load_l1_E_l2_E_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc1 to send cache request
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client1->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(0, manager->sharersList.size());



        //verify proc1 receives 2 responses
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp.msg);

	Proc_req cresp1 = cresps[1];
	CPPUNIT_ASSERT_EQUAL(REQ_ID2, cresp1.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp1.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp1.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }







    //======================================================================
    //======================================================================
    //! @brief Load hit in L1; manager in S.
    //!
    //! In a LOAD hit, client sends no message; both client and manager stay unchanged.
    //!
    //! Send a LOAD to 2 L1s with same address. This would bring the 2 clients and the manager
    //! to the S state. Send LOAD with dame address from a 1st proc; load hits.
    void test_load_l1_S_l2_S_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc2 to send cache request to put the line in S state.
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req2);
	When += 50;

	//schedule for the proc1 to send cache request
	const int REQ_ID3 = random();
	Proc_req* req3 = new Proc_req(REQ_ID3, NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req3);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client1->state);


	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in S state
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_S, client2->state);



        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in S state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_S, manager->state);
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_1));
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(NODEID_L1_2));



        //verify proc1 receives a 2 responses
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp.msg);

	Proc_req cresp1 = cresps[1];
	CPPUNIT_ASSERT_EQUAL(REQ_ID3, cresp1.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp1.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp1.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }





    //======================================================================
    //======================================================================
    //! @brief Load hit in L1; manager in E; client in M.
    //!
    //! Send a STORE to one of the L1s; client enters M state and manager enters E.
    //! Send a LOAD to the same address from same proc; it would hit.
    void test_load_l1_M_l2_E_0()
    {
	//create a STORE request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc1 to send cache request
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in M state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(0, manager->sharersList.count());



        //verify proc receives a response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp.msg);
	Proc_req cresp1 = cresps[1];
	CPPUNIT_ASSERT_EQUAL(REQ_ID2, cresp1.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp1.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp1.msg);



        //verify mc receives a request
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);
    }
















    //======================================================================
    //======================================================================
    //! @brief Store miss in L1; manager in I.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager; client enters IE.
    //! If it's also miss in L2, L2 sends a request to memory controller first;
    //! after the response from memory, L2 manager sends MESI_MC_GRANT_E_DATA to
    //! client; manager enters IE. Client enters E, eventually M, sends MESI_CM_UNBLOCK_E;
    //! manager enters E state (manager doesn't have M state).
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->mem
    //! 3. M<---mem
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E; C--->proc, Proc_req
    //!
    //! Send a STORE to one of the L1s; since it's the very first request, it would
    //! miss in both L1 and L2.
    void test_store_l1_I_l2_I_0()
    {
     
	//create a STORE request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in M state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);



        //verify proc receives a response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp.msg);


        //verify mc receives a request
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);
    }






    //======================================================================
    //======================================================================
    //! @brief Store miss in L1; manager in E.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager; client enters IE.
    //! If manager is in E, L2 sends MESI_MC_FWD_E to owner; it resets owner to the
    //! client and enters EE.
    //! Owner sends MESI_CC_E_DATA to client and enters I. Client sends MESI_CM_UNBLOCK_E
    //! to manager and enters E, and eventually M. Manager enters E state (manager does
    //! not have M state).
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    //! 3. C<---owner, MESI_CC_E_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_E; C--->proc, Proc_req
    //!
    //! Send a LOAD to one of the L1s; both L1 and L2 enter E state. Send STORE to same
    //! address from a different proc; the first client is invalidated; the 2nd client
    //! enters M and manager enters E state.
    void test_store_l1_I_l2_E_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc2 to send cache request
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req2);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//since the hash entry is not valid, its client's state is meaningless
	//MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	//CPPUNIT_ASSERT_EQUAL(MESI_C_S, client1->state);


	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in M state
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client2->state);



        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager->owner);
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(NODEID_L1_1));
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(NODEID_L1_2));




        //verify proc2 receives a response
        vector<Proc_req>& cresps = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID2, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }





    //======================================================================
    //======================================================================
    //! @brief Store miss in L1; manager in S.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager; client enters IE.
    //! If manager is in S, it sends MESI_MC_DEMAND_I to all sharers; it clears the
    //! sharers list and set owner to client; manager enters SIE.
    //! Sharers reply with MESI_CM_UNBLOCK_I and enters I state. When manager receives
    //! from all sharers, it sends MESI_MC_GRANT_E_DATA to client; manager enters IE state.
    //! Client sends MESI_CM_UNBLOCK_E to manager and enters E and eventually M.
    //! Manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->sharers, MESI_MC_DEMAND_I
    //! 3. M<---sharers, MESI_CM_UNBLOCK_I
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E; C--->proc, Proc_req
    //!
    //! Send a LOAD to 2 L1s with same address. This would bring the 2 clients and the manager
    //! to the S state. Sends STORE with dame address from a 3rd proc; the 3rd client enters M
    //! and manager enters E state; the other 2 clients enter I state.
    void test_store_l1_I_l2_S_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc2 to send cache request to put the line in S state.
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req2);
	When += 50;

	//schedule for the proc3 to send cache request
	const int REQ_ID3 = random();
	Proc_req* req3 = new Proc_req(REQ_ID3, NODEID_L1_3, ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp3, req3);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(ADDR)); //invalidated
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//since the hash entry is not valid, its client's state is meaningless
	//MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	//CPPUNIT_ASSERT_EQUAL(MESI_C_S, client1->state);


	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->my_table->has_match(ADDR)); //invalidated
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//since the hash entry is not valid, its client's state is meaningless
	//MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR)->get_idx()]);
	//CPPUNIT_ASSERT_EQUAL(MESI_C_S, client2->state);


	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p3->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p3->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p3->stalled_client_req_buffer.size());
	//client in M state
	MESI_client* client3 = dynamic_cast<MESI_client*>(m_l1p3->clients[m_l1p3->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client3->state);



        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_3, manager->owner);
	CPPUNIT_ASSERT_EQUAL(0, manager->sharersList.count());



        //verify proc3 receives a response
        vector<Proc_req>& cresps = m_procp3->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID3, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }





    //======================================================================
    //======================================================================
    //! @brief Store miss in L1; manager in E; owner in M.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager; client enters IE.
    //! If manager is in E state, it sends a MESI_MC_FWD_E to owner; it resets owner
    //! to client and enters EE.
    //! Owner in M state; sends MESI_CC_M_DATA to client and enters I.
    //! Client sends MESI_MC_GRANT_E_DATA to manager and enters E, and eventually M.
    //! Manager enters E state (manager doesn't have M state).
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E; manager EE
    //! 3. owner-->C, MESI_CC_M_DATA; owner I
    //! 4. C--->M, MESI_CM_UNBLOCK_E; C--->proc, Proc_req
    //!
    //! Send a STORE to one of the L1s; client enters M state and manager enters E.
    //! Send a STORE to the same address from another proc; it would miss in L1.
    void test_store_l1_I_owner_M_0()
    {
	//create a STORE request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc2 to send cache request
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req2);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in I state
	//MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	//CPPUNIT_ASSERT_EQUAL(MESI_C_S, client->state);

	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in M state
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager->owner);
	CPPUNIT_ASSERT_EQUAL(0, manager->sharersList.count());



        //verify proc receives a response
        vector<Proc_req>& cresps = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID2, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp.msg);


        //verify mc receives a request
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);
    }





    //======================================================================
    //======================================================================
    //! @brief Store hit in L1; manager in E; client in E.
    //!
    //! In a STORE hit, if client is in E, it is changed to M. Manager stays in E.
    //!
    //! Send a LOAD to one of the L1s; both L1 and L2 enter E state. Send STORE to same
    //! address from same proc; it is a hit.
    void test_store_l1_E_l2_E_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc1 to send cache request
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in M state
	MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client1->state);



        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(NODEID_L1_1));




        //verify proc1 receives 2 responses
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp.msg);
	Proc_req cresp1 = cresps[1];
	CPPUNIT_ASSERT_EQUAL(REQ_ID2, cresp1.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp1.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp1.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }





    //======================================================================
    //======================================================================
    //! @brief Store hit in L1; manager in S.
    //!
    //! In a STORE hit, client sends MESI_CM_I_to_E to manager; client enters SIE.
    //! If manager is in S, it sends MESI_MC_DEMAND_I to all sharers; it clears the
    //! sharers list and set owner to client; manager enters SIE.
    //! Sharers reply with MESI_CM_UNBLOCK_I and enters I state. Client is also a sharer,
    //! it enters IE state.
    //! When manager receives from all sharers, it sends MESI_MC_GRANT_E_DATA to client;
    //! manager enters IE state.
    //! Client sends MESI_CM_UNBLOCK_E to manager and enters E and eventually M.
    //! Manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->sharers, MESI_MC_DEMAND_I
    //! 3. M<---sharers, MESI_CM_UNBLOCK_I; sharers except client enter E; client enters IE.
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E; C--->proc, Proc_req
    //!
    //! Send a LOAD to 2 L1s with same address. This would bring the 2 clients and the manager
    //! to the S state. Sends STORE with dame address from a 1st proc; this would hit.
    void test_store_l1_S_l2_S_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc2 to send cache request to put the line in S state.
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req2);
	When += 50;

	//schedule for the proc1 to send cache request
	const int REQ_ID3 = random();
	Proc_req* req3 = new Proc_req(REQ_ID3, NODEID_L1_1, ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req3);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in M state.
	MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client1->state);


	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->my_table->has_match(ADDR)); //invalidated
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//since the hash entry is not valid, its client's state is meaningless
	//MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR)->get_idx()]);
	//CPPUNIT_ASSERT_EQUAL(MESI_C_S, client2->state);



        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(0, manager->sharersList.count());



        //verify proc1 receives 2 responses
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(LD_RESPONSE, cresp.msg);
	Proc_req cresp1 = cresps[1];
	CPPUNIT_ASSERT_EQUAL(REQ_ID3, cresp1.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp1.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp1.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }







    //======================================================================
    //======================================================================
    //! @brief Store hit in L1; manager in E; client in M.
    //!
    //! In a STORE hit, if client is in M, it stays in M. Manager stays in E.
    //!
    //! Send a STORE to one of the L1s; client enters M and manager enters E state.
    //! Send STORE to same address from same proc; it is a hit.
    void test_store_l1_M_l2_E_0()
    {
	//create a LOAD request
	const int REQ_ID = random();
	const paddr_t ADDR = random();

	Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the proc1 to send cache request
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule for the proc1 to send cache request
	const int REQ_ID2 = random();
	Proc_req* req2 = new Proc_req(REQ_ID2, NODEID_L1_2, ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in M state
	MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client1->state);



        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(NODEID_L1_1));




        //verify proc1 receives 2 responses
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());

	Proc_req cresp = cresps[0];
	CPPUNIT_ASSERT_EQUAL(REQ_ID, cresp.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp.msg);
	Proc_req cresp1 = cresps[1];
	CPPUNIT_ASSERT_EQUAL(REQ_ID2, cresp1.req_id); //verify the request id
	CPPUNIT_ASSERT_EQUAL(ADDR, cresp1.addr);
	//CPPUNIT_ASSERT_EQUAL(ST_COMPLETE, cresp1.msg);


        //verify mc receives one request only
	MockMc* mc;
	if(l2 == m_l2p1)
	    mc = m_mcp1;
	else
	    mc = m_mcp2;
        vector<Mem_msg>& creqs = mc->get_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)creqs.size());

	Mem_msg creq = creqs[0];
	CPPUNIT_ASSERT_EQUAL(m_l1p1->my_table->get_line_addr(ADDR), creq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, creq.op_type);
	if(l2 == m_l2p1)
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, creq.src_id);
	else
	    CPPUNIT_ASSERT_EQUAL(NODEID_L2_2, creq.src_id);

    }




    //======================================================================
    //======================================================================
    //! @brief Evict L1 line in E state.
    //!
    //! Send a LOAD request to a full set; victim is in E state.
    //!
    //! 1. LOAD request
    //! 2. C--->M, MESI_CM_E_to_I.
    //! 3. C<---M, MESI_MC_GRANT_I
    //! 4. C--->M, MESI_CM_UNBLOCK_I
    //! 5. C--->M, MESI_CM_I_to_S.
    //!
    //! Send N LOAD to an L1; then send another.
    //!
    void test_l1_E_eviction_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int NUM_TAGS = m_l1p1->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	//set is full
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule 1 more
	Proc_req* req = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);

	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);

	//victim has been evicted
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);



        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify mc receives a request
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)(creqs1.size() + creqs2.size()));

	//the last request has a memory request
	found = false;
	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		found = true;
		break;
	    }
	}
	if(found == false) {
	    for(unsigned j=0; j<creqs2.size(); j++) {
		if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		    found = true;
		    break;
		}
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);
    }






    //======================================================================
    //======================================================================
    //! @brief Evict L1 line in M state.
    //!
    //! Send a LOAD request to a full set; victim is in M state.
    //!
    //! 1. LOAD request
    //! 2. C--->M, MESI_CM_M_to_I.
    //! 3. C<---M, MESI_MC_GRANT_I; line in L2 marked dirty
    //! 4. C--->M, MESI_CM_UNBLOCK_I
    //! 5. C--->M, MESI_CM_I_to_S.
    //!
    void test_l1_M_eviction_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int NUM_TAGS = m_l1p1->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::STORE);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	//set is full
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;


	//schedule 1 more
	Proc_req* req = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);

	//victim has been evicted
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);



        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify mc receives a request
	//Store to an I line needs a Load first
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+2, (int)(creqs1.size() + creqs2.size())); //NUM_TAGS loads + 1 store (victim writeback) + 1 load(last request)

	//the last request has a memory request
	found = false;
	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		found = true;
		break;
	    }
	}
	if(found == false) {
	    for(unsigned j=0; j<creqs2.size(); j++) {
		if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		    found = true;
		    break;
		}
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);

	//The victim has 2 requests to the memory: the 1st is a load when it first does store,
	//and the 2nd is a store, which is a writeback when the L2 line is evicted.
	Mem_msg* victim_reqs[2];
	int nfound = 0;
	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == VICTIM_ADDR) {
		victim_reqs[nfound] = &creqs1[j];
		nfound++;
		assert(nfound <= 2);
	    }
	}
	if(nfound == 0) {
	    for(unsigned j=0; j<creqs2.size(); j++) {
		if(creqs2[j].addr == VICTIM_ADDR) {
		    victim_reqs[nfound] = &creqs2[j];
		    nfound++;
		    assert(nfound <= 2);
		}
	    }
	}
    }





    //======================================================================
    //======================================================================
    //! @brief Evict L1 line in S state.
    //!
    //! Send a LOAD request to a full set; victim is in S state.
    //!
    //! Since the victim is in S, it transits to I without sending any messages.
    //!
    void test_l1_S_eviction_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int NUM_TAGS = m_l1p1->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];



	Manifold::unhalt();
	Ticks_t When = 1;
	//first schedule a request from the 2nd L1 for the VICTIM_ADDR.
	{
	    Proc_req* req = new Proc_req(random(), NODEID_L1_2, VICTIM_ADDR, Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);
	}
	When += 50;

	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	//set is full; victim is S; rest in E
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule 1 more; victim will be evicted.
	Proc_req* req = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);

	//victim has been evicted
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));

	//In 2nd L1, VICTIM_ADDR still exists and in S state.
	{
	    CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(VICTIM_ADDR));
	    MESI_client* client = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	    CPPUNIT_ASSERT_EQUAL(MESI_C_S, client->state);
	}


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);

	//manager still has the VICTIM_ADDR line in S state.
	{
	    //###### NOTE: VICTIM_ADDR's l2 may be different from ADDR's l2
	    if(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_1) {
		l2 = m_l2p1;
	    }
	    else {
		assert(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_2);
		l2 = m_l2p2;
	    }
	    MESI_manager* managerv = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	    CPPUNIT_ASSERT_EQUAL(MESI_MNG_S, managerv->state);
	    CPPUNIT_ASSERT_EQUAL(-1, managerv->owner);
	    CPPUNIT_ASSERT_EQUAL(true, managerv->sharersList.get(NODEID_L1_1));
	    CPPUNIT_ASSERT_EQUAL(true, managerv->sharersList.get(NODEID_L1_2));
	}


        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify mc receives a request
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)(creqs1.size() + creqs2.size())); //1 load for 2nd L1; NUM_TAGS -1 loads for the NUM_TAGS requests
	                                                                        //from 1st L1; 1 for the last request from 1st L1.

	//the last request has a memory request
	found = false;
	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		found = true;
		break;
	    }
	}
	if(found == false) {
	    for(unsigned j=0; j<creqs2.size(); j++) {
		if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		    found = true;
		    break;
		}
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);
    }





    //======================================================================
    //======================================================================
    //! @brief Evict L1 line in S state, then manager invalidates the line.
    //!
    //! Send a LOAD request to a full set; victim is in S state. Victim is set to I state,
    //! but manager still thinks the line is in S state. When the other L1 requests the
    //! line to be changed from S to E, manager sends invalidation to all sharers. The
    //! L1 where the line is already evicted thus gets a request for which there's no mshr
    //! or hash table entry.
    //!
    void test_l1_S_eviction_1()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int NUM_TAGS = m_l1p1->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];


	Manifold::unhalt();
	Ticks_t When = 1;
	//first schedule a request from the 2nd L1 for the VICTIM_ADDR.
	{
	    Proc_req* req = new Proc_req(random(), NODEID_L1_2, VICTIM_ADDR, Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);
	}
	When += 50;

	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	//set is full; victim is S; rest in E
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 50;

	//schedule 1 more
	Proc_req* req = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	When += 50;

	//Write to the line so that manager would send out invalidations.
	{
	    Proc_req* req = new Proc_req(random(), NODEID_L1_2, VICTIM_ADDR, Proc_req::STORE);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);
	}
	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);

	//victim has been evicted
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));

	//In 2nd L1, VICTIM_ADDR still exists and in M state.
	{
	    CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(VICTIM_ADDR));
	    MESI_client* client = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	    CPPUNIT_ASSERT_EQUAL(MESI_C_M, client->state);
	}


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);

	//manager has the VICTIM_ADDR line in E state.
	{
	    //###### NOTE: VICTIM_ADDR's l2 may be different from ADDR's l2
	    if(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_1) {
		l2 = m_l2p1;
	    }
	    else {
		assert(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_2);
		l2 = m_l2p2;
	    }
	    MESI_manager* managerv = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	    CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, managerv->state);
	    CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, managerv->owner);
	    CPPUNIT_ASSERT_EQUAL(0, (int)managerv->sharersList.size()); //no sharers
	}



        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify mc receives a request
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)(creqs1.size() + creqs2.size())); //1 load for 2nd L1; NUM_TAGS -1 loads for the NUM_TAGS requests
	                                                                        //from 1st L1; 1 for the last request from 1st L1.

	//the last request has a memory request
	found = false;
	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		found = true;
		break;
	    }
	}
	if(found == false) {
	    for(unsigned j=0; j<creqs2.size(); j++) {
		if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		    found = true;
		    break;
		}
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);
    }










    //======================================================================
    //======================================================================
    //! @brief MSHR_STALL in L1.
    //!
    //! If L1's MSHR size is N, send N requests; before any response comes back, send
    //! one more, which would have a MSHR_STALL. Verify the stalled request is eventually
    //! processed.
    //!
    //! 1. C--->M, N MESI_CM_I_to_S
    //! 2. M--->mem
    //! 3. M<---mem
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E; C--->proc, Proc_req
    //!
    //! Send N LOAD to an L1; then send another.
    //!
    void test_MSHR_STALL_in_l1_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->mshr->get_tag_mask();
	const int NUM_TAGS = m_l1p1->mshr->assoc;
	assert(L1_MSHR_SIZE == m_l1p1->mshr->assoc);

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}



	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += 1;

	//schedule 1 more
	Proc_req* req = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);

	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	//There should be an entry in the hash table ????????
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR)); //Not necessarily!!
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table ?????
	//CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR)); //not necessarily!!
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);



        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify mc receives a request
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)(creqs1.size() + creqs2.size()));

	//the last request has a memory request
	found = false;
	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		found = true;
		break;
	    }
	}
	if(found == false) {
	    for(unsigned j=0; j<creqs2.size(); j++) {
		if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		    found = true;
		    break;
		}
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);
    }















//############
//Request race
//############

    //======================================================================
    //======================================================================
    //! @brief Test race condition on L1: E_to_I and FWD_E.
    //!
    //! Fill up a set. Send a write request to LRU from another client, which
    //! causes manager to send FWD_E to owner. Before the FWD_E reaches owner,
    //! send a read request to the same set but misses, which causes the LRU
    //! to start eviction: client gets FWD_E in the EI state.
    //!
    //! 0. Fill up a set on one L1_1 (the client of the LRU is called client1),
    //!    Client2 is a client from L1_2.
    //! 1. Client2--->manager, I_to_E.
    //! 2. 1 tick later, Client1 gets load request, causing eviction.
    //! 3. Client1--->Manager, MESI_CM_E_to_I.
    //! 4. Client1 gets FWD_E, sends CC_E_DATA to client2 and wakes up the LOAD.
    //!    Client --> manager, I_to_S.
    //! 5. Client2 --> manager, UNBLOCK_E.
    //! 6. Manager enters E, processes the E_to_I and simply ignores it.
    //! 7. Possibly before this, manager processes I_to_S.
    //!
    //! Eventually, the race was handled properly. Client2 has the VICTIM_ADDR in M.
    //! On L1_1, the victim was evicted, and the read for the new ADDR is processed
    //! as expected.
    //!
    void test_request_race_l1_E_to_I_l2_FWD_E_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int NUM_TAGS = m_l1p1->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];

//cout << "ADDR = " << hex<< ADDR <<dec<< "\n";

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
//cout << "addrs = " << hex<< addrs[i] <<dec<< "\n";
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	//set is full
	When += 50;

	//schedule a write request for L1_2 for L1_1's victiom.
	Proc_req* req = new Proc_req(random(), NODEID_L1_2, VICTIM_ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);

	When += 1; //the following must be done before FWD_E reaches L1_1.
	//schedule a read request for L1_1 that causes eviction.
	Proc_req* req2 = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);


	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(VICTIM_ADDR));
	//the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);

        //do the same for VICTIM_ADDR
        //verify state of L2 cache
	if(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(VICTIM_ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager->owner);


        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify proc2
        vector<Proc_req>& cresps2 = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps2.size());

	CPPUNIT_ASSERT_EQUAL(VICTIM_ADDR, cresps2[0].addr);


        //verify mc
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)(creqs1.size() + creqs2.size()));

	//there's 1 request for ADDR, and 2 for VICTIM_ADDR (1 read + 1 write).
	int addr_cnt = 0; //requests for ADDR.
	int vaddr_read_cnt = 0; //read request for VICTIM_ADDR.
	int vaddr_write_cnt = 0; //write request for VICTIM_ADDR.

	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		addr_cnt++;
	    }
	    else if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(VICTIM_ADDR)) {
	        if(creqs1[j].op_type == OpMemLd)
		    vaddr_read_cnt++;
		else
		    vaddr_write_cnt++;
	    }
	}
	for(unsigned j=0; j<creqs2.size(); j++) {
	    if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		addr_cnt++;
	    }
	    else if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(VICTIM_ADDR)) {
	        if(creqs2[j].op_type == OpMemLd)
		    vaddr_read_cnt++;
		else
		    vaddr_write_cnt++;
	    }
	}

	CPPUNIT_ASSERT_EQUAL(1, addr_cnt);
	CPPUNIT_ASSERT_EQUAL(1, vaddr_read_cnt);
	CPPUNIT_ASSERT_EQUAL(0, vaddr_write_cnt); //the write hits in L2.
    }





    //======================================================================
    //======================================================================
    //! @brief Test race condition on L1: E_to_I and FWD_S.
    //!
    //! Fill up a set. Send a read request to LRU from another client, which
    //! causes manager to send FWD_S to owner. Before the FWD_S reaches owner,
    //! send a read request to the same set but misses, which causes the LRU
    //! to start eviction: client gets FWD_S in the EI state.
    //!
    //! 0. Fill up a set on one L1_1 (the client of the LRU is called client1),
    //!    Client2 is a client from L1_2.
    //! 1. Client2--->manager, I_to_S.
    //! 2. 1 tick later, Client1 gets load request, causing eviction.
    //! 3. Client1--->Manager, MESI_CM_E_to_I.
    //! 4. Client1 gets FWD_S, sends CC_E_DATA to client2 and wakes up the LOAD.
    //!    Client --> manager, I_to_S.
    //! 5. Client2 --> manager, UNBLOCK_E.
    //! 6. Manager enters E, processes the E_to_I and ignores it.
    //! 7. Possibly before this, manager processes I_to_S.
    //!
    //! Eventually, the race was handled properly.
    //! On L1_1, the victim was evicted, and the read for the new ADDR is processed
    //! as expected.
    //!
    void test_request_race_l1_E_to_I_l2_FWD_S_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int NUM_TAGS = m_l1p1->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	//set is full
	When += 50;

	//schedule a READ request for L1_2 for L1_1's victiom.
	Proc_req* req = new Proc_req(random(), NODEID_L1_2, VICTIM_ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);

	When += 1; //the following must be done before FWD_S reaches L1_1.
	//schedule a read request for L1_1 that causes eviction.
	Proc_req* req2 = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);


	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(VICTIM_ADDR));
	//the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);

        //do the same for VICTIM_ADDR
        //verify state of L2 cache
	if(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(VICTIM_ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager->owner);


        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify proc2
        vector<Proc_req>& cresps2 = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps2.size());

	CPPUNIT_ASSERT_EQUAL(VICTIM_ADDR, cresps2[0].addr);


        //verify mc
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)(creqs1.size() + creqs2.size()));

	//there's 1 request for ADDR, and 1 for VICTIM_ADDR 
	int addr_cnt = 0; //requests for ADDR.
	int vaddr_read_cnt = 0; //read request for VICTIM_ADDR.
	int vaddr_write_cnt = 0; //write request for VICTIM_ADDR.

	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		addr_cnt++;
	    }
	    else if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(VICTIM_ADDR)) {
	        if(creqs1[j].op_type == OpMemLd)
		    vaddr_read_cnt++;
		else
		    vaddr_write_cnt++;
	    }
	}
	for(unsigned j=0; j<creqs2.size(); j++) {
	    if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		addr_cnt++;
	    }
	    else if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(VICTIM_ADDR)) {
	        if(creqs2[j].op_type == OpMemLd)
		    vaddr_read_cnt++;
		else
		    vaddr_write_cnt++;
	    }
	}

	CPPUNIT_ASSERT_EQUAL(1, addr_cnt);
	CPPUNIT_ASSERT_EQUAL(1, vaddr_read_cnt);
	CPPUNIT_ASSERT_EQUAL(0, vaddr_write_cnt); //no write involved.
    }





    //======================================================================
    //======================================================================
    //! @brief Test race condition on L1: M_to_I and FWD_E.
    //!
    //! Fill up a set with write requests so all clients end up in M state.
    //! Send a write request to LRU from another client, which
    //! causes manager to send FWD_E to owner. Before the FWD_E reaches owner,
    //! send a read request to the same set but misses, which causes the LRU
    //! to start eviction: client gets FWD_E in the MI state.
    //!
    //! 0. Fill up a set on one L1_1 (the client of the LRU is called client1),
    //!    Client2 is a client from L1_2.
    //! 1. Client2--->manager, I_to_E.
    //! 2. 1 tick later, Client1 gets load request, causing eviction.
    //! 3. Client1--->Manager, MESI_CM_M_to_I.
    //! 4. Client1 gets FWD_E, sends CC_M_DATA to client2 and wakes up the LOAD.
    //!    Client --> manager, I_to_S.
    //! 5. Client2 --> manager, UNBLOCK_E.
    //! 6. Manager enters E, processes the M_to_I and simply ignores it.
    //! 7. Possibly before this, manager processes I_to_S.
    //!
    //! Eventually, the race was handled properly. Client2 has the VICTIM_ADDR in M.
    //! On L1_1, the victim was evicted, and the read for the new ADDR is processed
    //! as expected.
    //!
    void test_request_race_l1_M_to_I_l2_FWD_E_0()
    {
	//create a request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int NUM_TAGS = m_l1p1->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];

//cout << "ADDR = " << hex<< ADDR <<dec<< "\n";

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
//cout << "addrs = " << hex<< addrs[i] <<dec<< "\n";
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::STORE); //Store so client is in M state.
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	//set is full
	When += 50;

	//schedule a write request for L1_2 for L1_1's victiom.
	Proc_req* req = new Proc_req(random(), NODEID_L1_2, VICTIM_ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);

	When += 1; //the following must be done before FWD_E reaches L1_1.
	//schedule a read request for L1_1 that causes eviction.
	Proc_req* req2 = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);


	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(VICTIM_ADDR));
	//the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);

        //do the same for VICTIM_ADDR
        //verify state of L2 cache
	if(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(VICTIM_ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager->owner);


        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify proc2
        vector<Proc_req>& cresps2 = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps2.size());

	CPPUNIT_ASSERT_EQUAL(VICTIM_ADDR, cresps2[0].addr);


        //verify mc
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)(creqs1.size() + creqs2.size()));

	//there's 1 request for ADDR, and 2 for VICTIM_ADDR (1 read + 1 write).
	int addr_cnt = 0; //requests for ADDR.
	int vaddr_read_cnt = 0; //read request for VICTIM_ADDR.
	int vaddr_write_cnt = 0; //write request for VICTIM_ADDR.

	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		addr_cnt++;
	    }
	    else if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(VICTIM_ADDR)) {
	        if(creqs1[j].op_type == OpMemLd)
		    vaddr_read_cnt++;
		else
		    vaddr_write_cnt++;
	    }
	}
	for(unsigned j=0; j<creqs2.size(); j++) {
	    if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		addr_cnt++;
	    }
	    else if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(VICTIM_ADDR)) {
	        if(creqs2[j].op_type == OpMemLd)
		    vaddr_read_cnt++;
		else
		    vaddr_write_cnt++;
	    }
	}

	CPPUNIT_ASSERT_EQUAL(1, addr_cnt);
	CPPUNIT_ASSERT_EQUAL(1, vaddr_read_cnt);
	CPPUNIT_ASSERT_EQUAL(0, vaddr_write_cnt); //the write hits in L2.
    }




    //======================================================================
    //======================================================================
    //! @brief Test race condition on L1: M_to_I and FWD_S.
    //!
    //! Fill up a set. Send a read request to LRU from another client, which
    //! causes manager to send FWD_S to owner. Before the FWD_S reaches owner,
    //! send a read request to the same set but misses, which causes the LRU
    //! to start eviction: client gets FWD_S in the MI state.
    //!
    //! 0. Fill up a set on one L1_1 (the client of the LRU is called client1),
    //!    Client2 is a client from L1_2.
    //! 1. Client2--->manager, I_to_S.
    //! 2. 1 tick later, Client1 gets load request, causing eviction.
    //! 3. Client1--->Manager, MESI_CM_M_to_I.
    //! 4. Client1 gets FWD_S, sends CC_M_DATA to client2 and wakes up the LOAD.
    //!    Client --> manager, I_to_S.
    //! 5. Client2 --> manager, UNBLOCK_E. and enters M state.
    //! 6. Manager enters E, processes the M_to_I and ignores it.
    //! 7. Possibly before this, manager processes I_to_S.
    //!
    //! Eventually, the race was handled properly.
    //! On L1_1, the victim was evicted, and the read for the new ADDR is processed
    //! as expected.
    //!
    void test_request_race_l1_M_to_I_l2_FWD_S_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int NUM_TAGS = m_l1p1->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t addrs[NUM_TAGS];

	for(int i=0; i<NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR & (~TAG_MASK); //clear the tag portion
		ad |= (random() & TAG_MASK); // OR-in a random tag
		if(ad != ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR || ret.second == false);

	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule N requests
	for(int i=0; i<NUM_TAGS; i++) {
	    int REQ_ID = random();
	    Proc_req* req = new Proc_req(REQ_ID, NODEID_L1_1, addrs[i], Proc_req::STORE); //store so client enters M state.
	    Manifold::Schedule(When, &MockProc::send_req, m_procp1, req);
	}
	//set is full
	When += 50;

	//schedule a READ request for L1_2 for L1_1's victiom.
	Proc_req* req = new Proc_req(random(), NODEID_L1_2, VICTIM_ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);

	When += 1; //the following must be done before FWD_S reaches L1_1.
	//schedule a read request for L1_1 that causes eviction.
	Proc_req* req2 = new Proc_req(random(), NODEID_L1_1, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);


	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(VICTIM_ADDR));
	//the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2;
	if(m_l2map->lookup(ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);

        //do the same for VICTIM_ADDR
        //verify state of L2 cache
	if(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_1) {
	    l2 = m_l2p1;
	}
	else {
	    assert(m_l2map->lookup(VICTIM_ADDR) == NODEID_L2_2);
	    l2 = m_l2p2;
	}
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(VICTIM_ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager->owner);


        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)cresps.size());

	for(int i=0; i<NUM_TAGS; i++) {
	    bool found = false;
	    for(unsigned j=0; j<cresps.size(); j++) {
	        if(cresps[j].addr == addrs[i]) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
	//the last request also has a response
	bool found = false;
	for(unsigned j=0; j<cresps.size(); j++) {
	    if(cresps[j].addr == ADDR) {
		found = true;
		break;
	    }
	}
	CPPUNIT_ASSERT_EQUAL(true, found);


        //verify proc2
        vector<Proc_req>& cresps2 = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps2.size());

	CPPUNIT_ASSERT_EQUAL(VICTIM_ADDR, cresps2[0].addr);


        //verify mc
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1, (int)(creqs1.size() + creqs2.size()));

	//there's 1 request for ADDR, and 1 for VICTIM_ADDR 
	int addr_cnt = 0; //requests for ADDR.
	int vaddr_read_cnt = 0; //read request for VICTIM_ADDR.
	int vaddr_write_cnt = 0; //write request for VICTIM_ADDR.

	for(unsigned j=0; j<creqs1.size(); j++) {
	    if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		addr_cnt++;
	    }
	    else if(creqs1[j].addr == m_l1p1->my_table->get_line_addr(VICTIM_ADDR)) {
	        if(creqs1[j].op_type == OpMemLd)
		    vaddr_read_cnt++;
		else
		    vaddr_write_cnt++;
	    }
	}
	for(unsigned j=0; j<creqs2.size(); j++) {
	    if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(ADDR)) {
		addr_cnt++;
	    }
	    else if(creqs2[j].addr == m_l1p1->my_table->get_line_addr(VICTIM_ADDR)) {
	        if(creqs2[j].op_type == OpMemLd)
		    vaddr_read_cnt++;
		else
		    vaddr_write_cnt++;
	    }
	}

	CPPUNIT_ASSERT_EQUAL(1, addr_cnt);
	CPPUNIT_ASSERT_EQUAL(1, vaddr_read_cnt);
	CPPUNIT_ASSERT_EQUAL(0, vaddr_write_cnt); //no write involved.
    }




    //======================================================================
    //======================================================================
    //! @brief Test race condition on L1: E_to_I and DEMAND_I.
    //!
    //! Fill up a set on L2; ensure the owner of LRU is L1_1. Fill up a set
    //! on L1 too; ensure the LRU is the same as the L2 set.
    //! Send a new request from L1_2 to L2 for the same set, which causes eviction.
    //! L2 sends DEMAND_I to L1_1.
    //! Before the DEMAND_I reaches L1_1, send a request to L1_1 causing L1_1 to
    //! evict. Now L1_1 would receive DEMAND_I while in EI state.
    //!
    //! Eventually, the race was handled properly. On L1_1, the victim was evicted, and
    //! the request causing the eviction was processed as expected. On L1_2, the request
    //! causing L2 eviction was processed as expected.
    //!
    void test_request_race_l1_E_to_I_l2_DEMAND_I_0()
    {
	//Assuming L2's assoc >= L1's assoc
	CPPUNIT_ASSERT(m_l2p1->my_table->assoc >= m_l1p1->my_table->assoc);

	//ADDR1 sent to L1_1 causing L1_1 eviction; ADDR2 sent to L1_2 causing L2_1 eviction.
	//Therefore, they must both map to L2_1.
	const paddr_t PAGE_MASK = ~(0x1 << PAGE_BITS);

	//Assuming there are 2 L2 nodes.
	CPPUNIT_ASSERT_EQUAL(2, (int)(dynamic_cast<MyL2Map*>(m_l1p1->l2_map))->get_nodes());


	//first create a random address.
	paddr_t ADDR2 = random(); //this is the one we use to cause L2 eviction.
	ADDR2 &= PAGE_MASK;
	CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, m_l1p1->l2_map->lookup(ADDR2));


	//Fill up L2 set
	const paddr_t L2_TAG_MASK = m_l2p1->my_table->get_tag_mask();
	const int L2_NUM_TAGS = m_l2p1->my_table->assoc;

	//create L2_NUM_TAGS  unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t l2_addrs[L2_NUM_TAGS];

	for(int i=0; i<L2_NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR2 & (~L2_TAG_MASK); //clear  mthe tag portion
		ad |= (random() & L2_TAG_MASK); // OR-in a random tag
	        ad &= PAGE_MASK;
		if(ad != ADDR2)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR2 || ret.second == false);

	    l2_addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = l2_addrs[0];
	CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, m_l1p1->l2_map->lookup(VICTIM_ADDR));


	//put the tags into L2 hash table.
	for(int i=0; i<L2_NUM_TAGS; i++) {
//cout << "l2 addrs = " << hex<< l2_addrs[i] <<dec<< "\n";
	    m_l2p1->my_table->reserve_block_for(l2_addrs[i]);
	    hash_entry* entry = m_l2p1->my_table->get_entry(l2_addrs[i]);
	    entry->set_have_data(true);
	    //put manager in E state.
	    MESI_manager* manager = dynamic_cast<MESI_manager*>(m_l2p1->managers[entry->get_idx()]);
	    manager->state = MESI_MNG_E;
	    manager->owner = NODEID_L1_1;
	}


        //do same for L1_1.
	const paddr_t L1_TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int L1_NUM_TAGS = m_l1p1->my_table->assoc;

	//create L1_NUM_TAGS unique tags in the same set as VICTIM_ADDR.
	addrset.clear();

	paddr_t l1_addrs[L1_NUM_TAGS + 1];
	l1_addrs[0] = VICTIM_ADDR;

	for(int i=0; i<L1_NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = VICTIM_ADDR & (~L1_TAG_MASK); //clear the tag portion
		ad |= (random() & L1_TAG_MASK); // OR-in a random tag
		ad &= PAGE_MASK;
		if(ad != VICTIM_ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == VICTIM_ADDR || ret.second == false);

	    l1_addrs[i+1] = ad;
	}


	const paddr_t ADDR1 = l1_addrs[L1_NUM_TAGS]; //this is the one we use to cause L1 eviction.
	CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, m_l1p1->l2_map->lookup(ADDR1));

	//put the tags into L1_1 hash table.
	for(int i=0; i<L1_NUM_TAGS; i++) {
//cout << "l1 addrs = " << hex<< l1_addrs[i] <<dec<< "\n";
	    m_l1p1->my_table->reserve_block_for(l1_addrs[i]);
	    hash_entry* entry = m_l1p1->my_table->get_entry(l1_addrs[i]);
	    //put client in E state.
	    MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[entry->get_idx()]);
	    client->state = MESI_C_E;
	}




	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule request for L1_2.
	Proc_req* req = new Proc_req(random(), NODEID_L1_2, ADDR2, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);

	When += 1; //the following must be done before DEMAND_I reaches L1_1.
	//schedule a read request for L1_1 that causes eviction.
	Proc_req* req2 = new Proc_req(random(), NODEID_L1_1, ADDR1, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);

	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR1));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR2));
	//the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR1));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR2));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR1)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR2)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2 = m_l2p1;
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR1));
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR2));
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(VICTIM_ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR1));
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR2));
	CPPUNIT_ASSERT_EQUAL(false, l2->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR1)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);
	MESI_manager* manager2 = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR2)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager2->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager2->owner);

        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	CPPUNIT_ASSERT_EQUAL(ADDR1, cresps[0].addr);


        //verify proc2
        vector<Proc_req>& cresps2 = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps2.size());
	CPPUNIT_ASSERT_EQUAL(ADDR2, cresps2[0].addr);


        //verify mc
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	//one request for ADDR1 and one for ADDR2.
	CPPUNIT_ASSERT_EQUAL(2, (int)(creqs1.size() + creqs2.size()));
    }




    //======================================================================
    //======================================================================
    //! @brief Test race condition on L1: M_to_I and DEMAND_I.
    //!
    //! Fill up a set on L2; ensure the owner of LRU is L1_1. Fill up a set
    //! on L1 too; ensure the LRU is the same as the L2 set.
    //! Send a new request from L1_2 to L2 for the same set, which causes eviction.
    //! L2 sends DEMAND_I to L1_1.
    //! Before the DEMAND_I reaches L1_1, send a request to L1_1 causing L1_1 to
    //! evict. Now L1_1 would receive DEMAND_I while in MI state.
    //!
    //! Eventually, the race was handled properly. On L1_1, the victim was evicted, and
    //! the request causing the eviction was processed as expected. On L1_2, the request
    //! causing L2 eviction was processed as expected.
    //!
    void test_request_race_l1_M_to_I_l2_DEMAND_I_0()
    {
	//Assuming L2's assoc >= L1's assoc
	CPPUNIT_ASSERT(m_l2p1->my_table->assoc >= m_l1p1->my_table->assoc);

	//ADDR1 sent to L1_1 causing L1_1 eviction; ADDR2 sent to L1_2 causing L2_1 eviction.
	//Therefore, they must both map to L2_1.
	const paddr_t PAGE_MASK = ~(0x1 << PAGE_BITS);

	//Assuming there are 2 L2 nodes.
	CPPUNIT_ASSERT_EQUAL(2, (int)(dynamic_cast<MyL2Map*>(m_l1p1->l2_map))->get_nodes());


	//first create a random address.
	paddr_t ADDR2 = random(); //this is the one we use to cause L2 eviction.
	ADDR2 &= PAGE_MASK;
	CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, m_l1p1->l2_map->lookup(ADDR2));


	//Fill up L2 set
	const paddr_t L2_TAG_MASK = m_l2p1->my_table->get_tag_mask();
	const int L2_NUM_TAGS = m_l2p1->my_table->assoc;

	//create L2_NUM_TAGS  unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t l2_addrs[L2_NUM_TAGS];

	for(int i=0; i<L2_NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR2 & (~L2_TAG_MASK); //clear  mthe tag portion
		ad |= (random() & L2_TAG_MASK); // OR-in a random tag
	        ad &= PAGE_MASK;
		if(ad != ADDR2)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR2 || ret.second == false);

	    l2_addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = l2_addrs[0];
	CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, m_l1p1->l2_map->lookup(VICTIM_ADDR));


	//put the tags into L2 hash table.
	for(int i=0; i<L2_NUM_TAGS; i++) {
//cout << "l2 addrs = " << hex<< l2_addrs[i] <<dec<< "\n";
	    m_l2p1->my_table->reserve_block_for(l2_addrs[i]);
	    hash_entry* entry = m_l2p1->my_table->get_entry(l2_addrs[i]);
	    entry->set_have_data(true);
	    //put manager in E state.
	    MESI_manager* manager = dynamic_cast<MESI_manager*>(m_l2p1->managers[entry->get_idx()]);
	    manager->state = MESI_MNG_E;
	    manager->owner = NODEID_L1_1;
	}


        //do same for L1_1.
	const paddr_t L1_TAG_MASK = m_l1p1->my_table->get_tag_mask();
	const int L1_NUM_TAGS = m_l1p1->my_table->assoc;

	//create L1_NUM_TAGS unique tags in the same set as VICTIM_ADDR.
	addrset.clear();

	paddr_t l1_addrs[L1_NUM_TAGS + 1];
	l1_addrs[0] = VICTIM_ADDR;

	for(int i=0; i<L1_NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = VICTIM_ADDR & (~L1_TAG_MASK); //clear the tag portion
		ad |= (random() & L1_TAG_MASK); // OR-in a random tag
		ad &= PAGE_MASK;
		if(ad != VICTIM_ADDR)
		    ret = addrset.insert(ad);
	    } while(ad == VICTIM_ADDR || ret.second == false);

	    l1_addrs[i+1] = ad;
	}


	const paddr_t ADDR1 = l1_addrs[L1_NUM_TAGS]; //this is the one we use to cause L1 eviction.
	CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, m_l1p1->l2_map->lookup(ADDR1));

	//put the tags into L1_1 hash table.
	for(int i=0; i<L1_NUM_TAGS; i++) {
//cout << "l1 addrs = " << hex<< l1_addrs[i] <<dec<< "\n";
	    m_l1p1->my_table->reserve_block_for(l1_addrs[i]);
	    hash_entry* entry = m_l1p1->my_table->get_entry(l1_addrs[i]);
	    //put client in M state.
	    MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[entry->get_idx()]);
	    client->state = MESI_C_M;
	}




	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule request for L1_2.
	Proc_req* req = new Proc_req(random(), NODEID_L1_2, ADDR2, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);

	When += 1; //the following must be done before DEMAND_I reaches L1_1.
	//schedule a read request for L1_1 that causes eviction.
	Proc_req* req2 = new Proc_req(random(), NODEID_L1_1, ADDR1, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);

	When += 50;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(ADDR1));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR2));
	//the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(ADDR1));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR2));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in E state
	MESI_client* client = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(ADDR1)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client->state);
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR2)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2 = m_l2p1;
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR1));
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR2));
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(VICTIM_ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR1));
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR2));
	CPPUNIT_ASSERT_EQUAL(false, l2->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	MESI_manager* manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR1)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);
	MESI_manager* manager2 = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR2)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager2->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager2->owner);

        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	CPPUNIT_ASSERT_EQUAL(ADDR1, cresps[0].addr);


        //verify proc2
        vector<Proc_req>& cresps2 = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps2.size());
	CPPUNIT_ASSERT_EQUAL(ADDR2, cresps2[0].addr);


        //verify mc
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	//one request for ADDR1, one for ADDR2, and one to write VICTIM_ADDR back to memory.
	CPPUNIT_ASSERT_EQUAL(3, (int)(creqs1.size() + creqs2.size()));
    }




    //======================================================================
    //======================================================================
    //! @brief Test race condition on L1: I_to_E (meaning S_to_E) and DEMAND_I.
    //!
    //! Fill up a set on L2; state of LRU is S, and VICTIM_ADDR is in both L1_1 and
    //! L1_2.
    //! Send a new request from L1_2 to L2 for the same set, which causes eviction.
    //! L2 sends DEMAND_I to L1_1.
    //! Before the DEMAND_I reaches L1_1, send a request to L1_1 causing L1_1 to
    //! request changing state from S to E for VICTIM_ADDR . Now L1_1 would receive
    //! DEMAND_I while in SE state. (Note, for S to E, the message sent is CM_I_to_E).
    //!
    //! Eventually, the race was handled properly. On L1_1, the victim eventually becomes
    //! E. On L1_2, the request causing L2 eviction was processed as expected.
    //!
    void test_request_race_l1_I_to_E_l2_DEMAND_I_0()
    {
	//ADDR2 sent to L1_2 causing L2_1 eviction. Therefore, it must map to L2_1.
	const paddr_t PAGE_MASK = ~(0x1 << PAGE_BITS);

	//Assuming there are 2 L2 nodes.
	CPPUNIT_ASSERT_EQUAL(2, (int)(dynamic_cast<MyL2Map*>(m_l1p1->l2_map))->get_nodes());


	//first create a random address.
	paddr_t ADDR2 = random(); //this is the one we use to cause L2 eviction.
	ADDR2 &= PAGE_MASK;
	CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, m_l1p2->l2_map->lookup(ADDR2));


	//Fill up L2 set
	const paddr_t L2_TAG_MASK = m_l2p1->my_table->get_tag_mask();
	const int L2_NUM_TAGS = m_l2p1->my_table->assoc;

	//create L2_NUM_TAGS  unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t l2_addrs[L2_NUM_TAGS];

	for(int i=0; i<L2_NUM_TAGS; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
	    //make sure ad is new and different from ADDR
	    do {
		ad = ADDR2 & (~L2_TAG_MASK); //clear  mthe tag portion
		ad |= (random() & L2_TAG_MASK); // OR-in a random tag
	        ad &= PAGE_MASK;
		if(ad != ADDR2)
		    ret = addrset.insert(ad);
	    } while(ad == ADDR2 || ret.second == false);

	    l2_addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = l2_addrs[0];
	CPPUNIT_ASSERT_EQUAL(NODEID_L2_1, m_l1p1->l2_map->lookup(VICTIM_ADDR));


	//put the tags into L2 hash table.
	for(int i=0; i<L2_NUM_TAGS; i++) {
//cout << "l2 addrs = " << hex<< l2_addrs[i] <<dec<< "\n";
	    m_l2p1->my_table->reserve_block_for(l2_addrs[i]);
	    hash_entry* entry = m_l2p1->my_table->get_entry(l2_addrs[i]);
	    entry->set_have_data(true);
	}

	//put manager of VICTIM_ADDR in S state.
	hash_entry* victim_entry = m_l2p1->my_table->get_entry(VICTIM_ADDR);
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_l2p1->managers[victim_entry->get_idx()]);
	manager->state = MESI_MNG_S;
	manager->sharersList.set(NODEID_L1_1);
	manager->sharersList.set(NODEID_L1_2);

	//put addrs[1] in E state, because it will also be evicted.
	victim_entry = m_l2p1->my_table->get_entry(l2_addrs[1]);
	manager = dynamic_cast<MESI_manager*>(m_l2p1->managers[victim_entry->get_idx()]);
	manager->state = MESI_MNG_E;
	manager->owner = NODEID_L1_1;


        //put VICTIM_ADDR in L1_1.
	m_l1p1->my_table->reserve_block_for(VICTIM_ADDR);
	victim_entry = m_l1p1->my_table->get_entry(VICTIM_ADDR);
	//put client in S state.
	MESI_client* client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[victim_entry->get_idx()]);
	client1->state = MESI_C_S;
	//put l2_addrs[1] in L1_1 as well.
	m_l1p1->my_table->reserve_block_for(l2_addrs[1]);
	victim_entry = m_l1p1->my_table->get_entry(l2_addrs[1]);
	//put client in S state.
	client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[victim_entry->get_idx()]);
	client1->state = MESI_C_E;


        //do the same for L1_2.
        //put VICTIM_ADDR in L1_2.
	m_l1p2->my_table->reserve_block_for(VICTIM_ADDR);
	victim_entry = m_l1p2->my_table->get_entry(VICTIM_ADDR);
	//put client in S state.
	MESI_client* client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[victim_entry->get_idx()]);
	client2->state = MESI_C_S;



	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule request for L1_2.
	Proc_req* req = new Proc_req(random(), NODEID_L1_2, ADDR2, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_req, m_procp2, req);

	When += 1; //the following must be done before DEMAND_I reaches L1_1.
	//schedule a WRITE request for L1_1 that causes S to E transition.
	Proc_req* req2 = new Proc_req(random(), NODEID_L1_1, VICTIM_ADDR, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_req, m_procp1, req2);

	When += 100;

	Manifold::StopAt(When);  //############## make sure stop time > time when everything is complete.
	Manifold::Run();


        //verify state of L1 cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_l1p1->mshr->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(false, m_l1p2->mshr->has_match(ADDR2));
	//the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_l1p1->my_table->has_match(VICTIM_ADDR));
	CPPUNIT_ASSERT_EQUAL(true, m_l1p2->my_table->has_match(ADDR2));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p1->stalled_client_req_buffer.size());
	CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p2->stalled_client_req_buffer.size());
	//client in M state
	client1 = dynamic_cast<MESI_client*>(m_l1p1->clients[m_l1p1->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_M, client1->state);
	client2 = dynamic_cast<MESI_client*>(m_l1p2->clients[m_l1p2->my_table->get_entry(ADDR2)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_C_E, client2->state);


        //verify state of L2 cache
	MESI_L2_cache* l2 = m_l2p1;
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(ADDR2));
	CPPUNIT_ASSERT_EQUAL(false, l2->mshr->has_match(VICTIM_ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(ADDR2));
	CPPUNIT_ASSERT_EQUAL(true, l2->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)l2->stalled_client_req_buffer.size());
	//manager in E state
	manager = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(VICTIM_ADDR)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_1, manager->owner);
	MESI_manager* manager2 = dynamic_cast<MESI_manager*>(l2->managers[l2->my_table->get_entry(ADDR2)->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager2->state);
	CPPUNIT_ASSERT_EQUAL(NODEID_L1_2, manager2->owner);

        //verify proc receives response
        vector<Proc_req>& cresps = m_procp1->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	CPPUNIT_ASSERT_EQUAL(VICTIM_ADDR, cresps[0].addr);


        //verify proc2
        vector<Proc_req>& cresps2 = m_procp2->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps2.size());
	CPPUNIT_ASSERT_EQUAL(ADDR2, cresps2[0].addr);


        //verify mc
        vector<Mem_msg>& creqs1 = m_mcp1->get_reqs();
        vector<Mem_msg>& creqs2 = m_mcp2->get_reqs();
	//one request for VICTIM_ADDR and one for ADDR2.
	CPPUNIT_ASSERT_EQUAL(2, (int)(creqs1.size() + creqs2.size()));
    }
/*
*/







    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MESI_L1_L2_cacheTest");
	/*
	*/
	//load miss
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_load_l1_I_l2_I_0", &MESI_L1_L2_cacheTest::test_load_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_load_l1_I_l2_E_0", &MESI_L1_L2_cacheTest::test_load_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_load_l1_I_owner_M_0", &MESI_L1_L2_cacheTest::test_load_l1_I_owner_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_load_l1_I_l2_S_0", &MESI_L1_L2_cacheTest::test_load_l1_I_l2_S_0));
	//load hit
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_load_l1_E_l2_E_0", &MESI_L1_L2_cacheTest::test_load_l1_E_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_load_l1_S_l2_S_0", &MESI_L1_L2_cacheTest::test_load_l1_S_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_load_l1_M_l2_E_0", &MESI_L1_L2_cacheTest::test_load_l1_M_l2_E_0));
	//store miss
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_store_l1_I_l2_I_0", &MESI_L1_L2_cacheTest::test_store_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_store_l1_I_l2_E_0", &MESI_L1_L2_cacheTest::test_store_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_store_l1_I_l2_S_0", &MESI_L1_L2_cacheTest::test_store_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_store_l1_I_owner_M_0", &MESI_L1_L2_cacheTest::test_store_l1_I_owner_M_0));
	//store hit
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_store_l1_E_l2_E_0", &MESI_L1_L2_cacheTest::test_store_l1_E_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_store_l1_S_l2_S_0", &MESI_L1_L2_cacheTest::test_store_l1_S_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_store_l1_M_l2_E_0", &MESI_L1_L2_cacheTest::test_store_l1_M_l2_E_0));
	//L1 eviction
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_l1_E_eviction_0", &MESI_L1_L2_cacheTest::test_l1_E_eviction_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_l1_M_eviction_0", &MESI_L1_L2_cacheTest::test_l1_M_eviction_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_l1_S_eviction_0", &MESI_L1_L2_cacheTest::test_l1_S_eviction_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_l1_S_eviction_1", &MESI_L1_L2_cacheTest::test_l1_S_eviction_1));
	//MSHR_STALL in L1
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_MSHR_STALL_in_l1_0", &MESI_L1_L2_cacheTest::test_MSHR_STALL_in_l1_0));
	//Request race
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_request_race_l1_E_to_I_l2_FWD_E_0", &MESI_L1_L2_cacheTest::test_request_race_l1_E_to_I_l2_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_request_race_l1_E_to_I_l2_FWD_S_0", &MESI_L1_L2_cacheTest::test_request_race_l1_E_to_I_l2_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_request_race_l1_M_to_I_l2_FWD_E_0", &MESI_L1_L2_cacheTest::test_request_race_l1_M_to_I_l2_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_request_race_l1_M_to_I_l2_FWD_S_0", &MESI_L1_L2_cacheTest::test_request_race_l1_M_to_I_l2_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_request_race_l1_E_to_I_l2_DEMAND_I_0", &MESI_L1_L2_cacheTest::test_request_race_l1_E_to_I_l2_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_request_race_l1_M_to_I_l2_DEMAND_I_0", &MESI_L1_L2_cacheTest::test_request_race_l1_M_to_I_l2_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_L2_cacheTest>("test_request_race_l1_I_to_E_l2_DEMAND_I_0", &MESI_L1_L2_cacheTest::test_request_race_l1_I_to_E_l2_DEMAND_I_0));
	//????????????????????? NEED test case for I_to_E and DEMAND_I conflict while client sends I_to_E in IE state ??????????????
	/*
	*/



	return mySuite;
    }
};


Clock MESI_L1_L2_cacheTest :: MasterClock(MESI_L1_L2_cacheTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MESI_L1_L2_cacheTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


