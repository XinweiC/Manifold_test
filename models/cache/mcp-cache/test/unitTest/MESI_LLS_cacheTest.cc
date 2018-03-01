//! This program tests MESI_LLS_cache as a component. We use 2 Mock
//! components and connect the MESI_LLS_cache to the component.
//!
//!                    --------
//!                   | MockL1 |
//!                    --------
//!                         | Coh_msg
//!                  ---------------
//!                 | MESI_L2_cache |
//!                  ---------------
//!                         | NetworkPacket; this link is not Manifold link
//!                      ---------
//!                     | MockMux |
//!                      ---------
//!                          |
//!                      ---------
//!                     | MockNet |
//!                      ---------

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
#include "mux_demux.h"
#include "MESI_LLS_cache.h"
#include "LLP_cache.h"
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
const int COH = 123;
const int MEM = 456;
const int CREDIT = 789;


//! This class emulates an L1 and a memory controller.
class MockL1 : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};

    MockL1() {}

    void send_req(Coh_msg* req)
    {
	Send(OUT, req);
    }
    void send_pkt(NetworkPacket* pkt)
    {
//cout << "send_pkt, type= " << pkt->type << " pkt= " << pkt << "\n";
	Send(OUT, pkt);
    }
    void handle_incoming(int, Coh_msg* msg)
    {
    //cout << "MockProc handle_incoming @ " << Manifold::NowTicks() << endl;
	m_cache_resps.push_back(*msg);
	delete msg;
        //m_ticks.push_back(Manifold :: NowTicks());
    }

/*
    void handle_incoming_packet(int, NetworkPacket* pkt)
    {
//cout << "handle_incoming_packet, pkt= " << pkt << " type= " << pkt->type << " tick= " << manifold::kernel::Manifold::NowTicks() << "\n";
	if(pkt->type == L2_cache :: COH_MSG) {
	    m_pkts.push_back(*pkt);
	    //do nothing
	    delete pkt;
	}
	else if(pkt->type == L2_cache :: MEM_MSG) {
	    m_pkts.push_back(*pkt);
	    Mem_msg* mem = (Mem_msg*)(pkt->data);
	    if(mem->op_type == OpMemLd)
		Send(OUT, pkt);
	    else
	        delete pkt;
	}
	else if(pkt->type == L2_cache :: CREDIT_MSG) {
	    //do nothing
	    delete pkt;
	}
	else {
	    assert(0);
	}
    }
*/
    vector<Coh_msg>& get_cache_resps() { return m_cache_resps; }
    vector<NetworkPacket>& get_pkts() { return m_pkts; }
    //vector<Ticks_t>& get_ticks() { return m_ticks; }
private:
    vector<Coh_msg> m_cache_resps;
    vector<NetworkPacket> m_pkts;
    //vector<Ticks_t> m_ticks;
};


//! This class subclasses MuxDemux.
class MockMux : public MuxDemux {
public:
    MockMux(MESI_LLS_cache* c) : m_lls_cache(c), MuxDemux(Clock::Master(), CREDIT)  {}

    void tick()
    {
        NetworkPacket* pkt = m_lls_cache->pop_from_output_buffer(); 
	if(pkt) {
	    if(pkt->type == L2_cache :: COH_MSG) {
		m_pkts.push_back(*pkt);
		delete pkt;
	    }
	    else if(pkt->type == L2_cache :: MEM_MSG) {
		m_pkts.push_back(*pkt);
		Mem_msg* mem = (Mem_msg*)(pkt->data);
		if(mem->op_type == OpMemLd)
		    m_lls_cache->handle_incoming<Mem_msg>(0, pkt);
		else
		    delete pkt;
	    }
	    else if(pkt->type == L2_cache :: CREDIT_MSG) {
		m_credit_pkts.push_back(*pkt);
		delete pkt;
	    }
	    else {
		assert(0);
	    }
	}
    }


    void send_pkt(NetworkPacket* pkt)
    {
	m_lls_cache->handle_incoming<Mem_msg>(0, pkt);
    }

    void dummy(int, int) {} //dummy handler

    vector<NetworkPacket>& get_pkts() { return m_pkts; }
    //vector<NetworkPacket>& get_credits() { return m_credit_pkts; }

private:
    MESI_LLS_cache* m_lls_cache;

    //Save the requests and timestamps for verification.
    vector<NetworkPacket> m_pkts;
    vector<NetworkPacket> m_credit_pkts;
};


//! This class emulates the network below the mux
class MockNet : public manifold::kernel::Component {
public:
    enum {PORT=0};

    void handle_incoming_packet(int, NetworkPacket* pkt)
    {
	if(pkt->type == CREDIT)
	    m_credit_pkts.push_back(*pkt);
	else {
	    assert(0);
	}
    }

    vector<NetworkPacket>& get_credits() { return m_credit_pkts; }

private:
    vector<NetworkPacket> m_credit_pkts;
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


#define NODE_ID  7
#define MC_ID  8


//####################################################################
//! Class MESI_LLS_cacheTest is the test class for class MESI_LLS_cache. 
//####################################################################
class MESI_LLS_cacheTest : public CppUnit::TestFixture {
private:
    static const int HT_SIZE = 0x1 << 14; //2^14 = 16k;
    static const int HT_ASSOC = 4;
    static const int HT_BLOCK_SIZE = 32;
    static const int HT_HIT = 2;
    static const int HT_LOOKUP = 11;

    static const unsigned MSHR_SIZE = 8;

    //latencies
    static const Ticks_t L1_L2 = 1;

    MESI_LLS_cache* m_cachep;
    MockL1* m_l1p;
    MockMux* m_muxp;
    MockNet* m_netp;

public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
	cache_settings parameters;
	parameters.name = "testCache";
	//parameters.type = CACHE_DATA;
	parameters.size = HT_SIZE;
	parameters.assoc = HT_ASSOC;
	parameters.block_size = HT_BLOCK_SIZE;
	parameters.hit_time = HT_HIT;
	parameters.lookup_time = HT_LOOKUP;
	parameters.replacement_policy = RP_LRU;


        //create a MockL1, a MESI_L2_cache.
	CompId_t l1Id = Component :: Create<MockL1>(0);
	m_l1p = Component::GetComponent<MockL1>(l1Id);

	L2_cache :: Set_msg_types(COH, MEM, CREDIT);

	L2_cache_settings settings;
	settings.mc_map =  new MyMcMap1(MC_ID); //all addresses map to mc MC_ID
	settings.mshr_sz = MSHR_SIZE;
	settings.downstream_credits = 30;

	int nodeId = NODE_ID;
	CompId_t cacheId = Component :: Create<MESI_LLS_cache>(0, nodeId, parameters, settings);
	m_cachep = Component::GetComponent<MESI_LLS_cache>(cacheId);

	CompId_t muxId = Component :: Create<MockMux>(0, m_cachep);
	m_muxp = Component::GetComponent<MockMux>(muxId);
	CompId_t netId = Component :: Create<MockNet>(0);
	m_netp = Component::GetComponent<MockNet>(netId);

	m_cachep->set_mux(m_muxp);

        //connect the components
	//L1 to LLS
	Manifold::Connect(l1Id, MockL1::OUT, &MockL1::handle_incoming,
	                  cacheId, LLS_cache::PORT_LOCAL_L1, &LLS_cache::handle_llp_incoming,
			  MasterClock, MasterClock, L1_L2, L1_L2);

	//mux and mockNet
	Manifold::Connect(muxId, MuxDemux::PORT_NET, &MockMux::dummy,
	                  netId, MockNet::PORT, &MockNet::handle_incoming_packet,
			  MasterClock, MasterClock, 1, 1);

	Clock::Register(MasterClock, (LLS_cache*)m_cachep, &LLS_cache::tick, (void(LLS_cache::*)(void))0);
	Clock::Register(MasterClock, m_muxp, &MockMux::tick, (void(MockMux::*)(void))0);

	//cout << "COH= " << L2_cache ::COH_MSG << "  meM= " << L2_cache :: MEM_MSG << "\n";
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss; also miss in L2.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. If it's also miss
    //! in L2, L2 sends a request to memory controller first; after the response
    //! from memory, L2 manager sends MESI_MC_GRANT_E_DATA to client; manager
    //! enters IE.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->mem
    //! 3. M<---mem
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //!
    void test_process_client_request_load_l1_I_l2_I_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg;
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt + L1_L2 + 2*HT_LOOKUP + 10);
	Manifold::Run();

        //verify the mock object receives a memory request
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	{
	    if(SOURCE_ID == NODE_ID)
		CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());
	    else
		CPPUNIT_ASSERT_EQUAL(2, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);

	    Mem_msg mreq = *((Mem_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);
	}

        //verify the mock object receives a MESI_MC_GRANT_E_DATA
	Coh_msg cresp;
	if(SOURCE_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    NetworkPacket& pkt = mux_pkts[1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_IE, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss in L1; manager in E.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_S to the owner; then it clears owner and adds client
    //! to sharer list. Manager enters ES.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    void test_process_client_request_load_l1_I_l2_E_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify the mock object receives a MESI_MC_FWD_S request.
	Coh_msg cresp;
	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	//hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	//CPPUNIT_ASSERT(entry != 0);
	//MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->mshr_managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_ES, manager->state);
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss in L1; manager in S.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in S, it sends MESI_MC_GRANT_S_DATA to the client; it adds client
    //! to sharer list and enters SS.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->C, MESI_MC_GRANT_S_DATA
    void test_process_client_request_load_l1_I_l2_S_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_S;
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify the mock object receives a MESI_MC_GRAND_S_DATA request.
	Coh_msg cresp;
	if(SOURCE_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_S_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	//hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	//CPPUNIT_ASSERT(entry != 0);
	//MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->mshr_managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_SS, manager->state);
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss; also miss in L2.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. If it's also miss
    //! in L2, L2 sends a request to memory controller first to read the line; after
    //! the response from memory, L2 manager sends MESI_MC_GRANT_E_DATA to client;
    //! manager enters IE.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->mem read
    //! 3. M<---mem
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //!
    void test_process_client_request_store_l1_I_l2_I_0()
    {
	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID = random();
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();

        //verify the mock object receives a memory request
	vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	{
	    if(SOURCE_ID == NODE_ID)
		CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());
	    else
		CPPUNIT_ASSERT_EQUAL(2, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);

	    Mem_msg mreq = *((Mem_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);
	}

        //verify the mock object receives a MESI_MC_GRANT_E_DATA
	Coh_msg cresp;
	if(SOURCE_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();

	    NetworkPacket& pkt = mux_pkts[1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_IE, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss in L1; manager in E.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_E to the owner; then it clears owner and sets the
    //! client as the new owner. Manager enters EE state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    void test_process_client_request_store_l1_I_l2_E_0()
    {
	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify the mock object receives a MESI_MC_FWD_E request.
	Coh_msg cresp;
	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	//hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	//CPPUNIT_ASSERT(entry != 0);
	//MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->mshr_managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_EE, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss in L1; manager in S.
    //!
    //! In a STOREmiss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in S, it sends MESI_MC_DEMAND_I to all sharers; the client is set to be owner
    //! and manager enters SIE.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->all sharers, MESI_MC_DEMAND_I
    void test_process_client_request_store_l1_I_l2_S_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_S;
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;
	//add a random number of sharers
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    do {
		SHARERS_ID[i] = random() % 1024;
	    } while(SHARERS_ID[i] == SOURCE_ID || SHARERS_ID[i] == NODE_ID);
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify the mock object receives a MESI_MC_DEMAND_I request.
        vector<NetworkPacket>& cresps = m_muxp->get_pkts();
        vector<Coh_msg>& l1_cresps = m_l1p->get_cache_resps();

	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)cresps.size() + (int)l1_cresps.size());
	list<int> dest_ids(NUM_SHARERS);
	for(unsigned i=0; i<cresps.size(); i++) {
	    NetworkPacket& pkt = cresps[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}
	for(unsigned i=0; i<l1_cresps.size(); i++) {
	    Coh_msg cresp = l1_cresps[i];

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}


	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}


	//verify state of the MESI_L2_cache
	//There should be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	//hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	//CPPUNIT_ASSERT(entry != 0);
	//MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->mshr_managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_SIE, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in I.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. If it's also miss
    //! in L2, L2 sends a request to memory controller first; after the response
    //! from memory, L2 manager sends MESI_MC_GRANT_E_DATA to client; manager
    //! enters IE. Upresponse from client (MESI_CM_UNBLOCK_E), manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->mem
    //! 3. M<---mem
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E
    //!
    void test_process_client_request_and_reply_load_l1_I_l2_I_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random();
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_E;
	req2->src_id = SOURCE_ID;
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the mock object receives a memory request
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	{
	    if(SOURCE_ID == NODE_ID)
		CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());
	    else
		CPPUNIT_ASSERT_EQUAL(2, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);

	    Mem_msg mreq = *((Mem_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);
	}

        //verify the mock object receives a MESI_MC_GRANT_E_DATA
	Coh_msg cresp;
	if(SOURCE_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();

	    NetworkPacket& pkt = mux_pkts[1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in E; owner in E.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_S to the owner; it adds client
    //! to sharer list and enters ES. Owner sends MESI_CC_S_DATA to client and MESI_CM_CLEAN
    //! to manager; client sends MESI_CM_UNBLOCK_S to manager; manager enters S state.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    //! 3. owner--->M, MESI_CM_CLEAN; and owner--->C, MESI_CC_S_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_S
    void test_process_client_request_and_reply_load_l1_I_l2_E_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	int OWNER_ID;
	while((OWNER_ID = random() % 1024) == SOURCE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_CLEAN;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_S;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_FWD_S which was from manager to owner
	Coh_msg cresp;
	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_S, manager->state);
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(OWNER_ID));
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in E; owner in M.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_S to the owner; it adds client
    //! to sharer list and enters ES. Owner sends MESI_CC_S_DATA to client and MESI_CM_WRITEBACK
    //! to manager; client sends MESI_CM_UNBLOCK_S to manager; manager enters S state;
    //! manager's hash entry in dirty state.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    //! 3. owner--->M, MESI_CM_WRITEBACK; and owner--->C, MESI_CC_S_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_S
    void test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	int OWNER_ID;
	while((OWNER_ID = random() % 1024) == SOURCE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_WRITEBACK;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_S;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_FWD_S which was from manager to owner
	Coh_msg cresp;
	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->get_entry(ADDR)->is_dirty());
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_S, manager->state);
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(OWNER_ID));
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in S.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in S, it sends MESI_MC_GRANT_S_DATA to the client; it adds client
    //! to sharer list and enters SS. Client replys with MESI_CM_UNBLOCK_S; manager
    //! enters S.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->C, MESI_MC_GRANT_S_DATA
    //! 3. C--->M, MESI_CM_UNBLOCK_S
    void test_process_client_request_and_reply_load_l1_I_l2_S_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_S;
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_S;
	req2->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives a MESI_MC_GRANT_S_DATA request.
	Coh_msg cresp;
	if(SOURCE_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_S_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	//hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	//CPPUNIT_ASSERT(entry != 0);
	//MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->mshr_managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_S, manager->state);
	CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
    }






    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store
    //! miss in L1; also miss in L2.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. If it's also miss
    //! in L2, L2 sends a request to memory controller first to read the line; after
    //! the response from memory, L2 manager sends MESI_MC_GRANT_E_DATA to client;
    //! manager enters IE. Client sends back MESI_CM_UNBLOCK_E; manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->mem read
    //! 3. M<---mem
    //! 4. C<---M, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E.
    //!
    void test_process_client_request_and_reply_store_l1_I_l2_I_0()
    {
	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID = random();
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_E;
	req2->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the mock object receives a memory request
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	{
	    if(SOURCE_ID == NODE_ID)
		CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());
	    else
		CPPUNIT_ASSERT_EQUAL(2, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);

	    Mem_msg mreq = *((Mem_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);
	}



        //verify the mock object receives a MESI_MC_GRANT_E_DATA
	Coh_msg cresp;
	if(SOURCE_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(2, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store miss in L1;
    //! manager in E.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_E to the owner; then it clears owner and sets the
    //! client as the new owner. Manager enters EE state. Owner sends MESI_CC_E_DATA to
    //! client; client sends MESI_CM_UNBLOCK_E to manager; manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    //! 3. owner--->C, MESI_CC_E_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_E
    void test_process_client_request_and_reply_store_l1_I_l2_E_0()
    {
	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	int OWNER_ID;
	while((OWNER_ID = random() % 1024) == SOURCE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_E;
	req2->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives a MESI_MC_FWD_E request.
	Coh_msg cresp;
	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store miss in L1;
    //! manager in S.
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in S, it sends MESI_MC_DEMAND_I to all sharers; the client is set to be owner
    //! and manager enters SIE; manager enters SIE. Sharers send MESI_CM_UNBLOCK_I to
    //! manager; after receiving from all sharers, manager sends MESI_MC_GRANT_E_DATA
    //! to client and enters IE. Client sends MESI_CM_UNBLOCK_E; manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->all sharers, MESI_MC_DEMAND_I
    //! 3. sharers--->M, MESI_CM_UNBLOCK_I
    //! 4. M--->C, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E
    void test_process_client_request_and_reply_store_l1_I_l2_S_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_S;
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;
	//add a random number of sharers
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    SHARERS_ID[i] = random() % 1024;
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=0; i<NUM_SHARERS; i++) {
	    Coh_msg* req2 = new Coh_msg();
	    req2->type = Coh_msg :: COH_RPLY;
	    req2->addr = ADDR;
	    req2->msg = MESI_CM_UNBLOCK_I;
	    req2->src_id = SHARERS_ID[i];
	    if(SHARERS_ID[i] == NODE_ID) {
		Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	    }
	    else {
		NetworkPacket* pkt = new NetworkPacket;
		pkt->type = L2_cache :: COH_MSG;
		pkt->src = SOURCE_ID;
		pkt->src_port = LLP_cache :: LLP_ID;
		pkt->dst = NODE_ID;
		pkt->dst_port = LLP_cache :: LLS_ID;
		*((Coh_msg*)(pkt->data)) = *req2;
		pkt->data_size = sizeof(Coh_msg);
		delete req2;

		Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	    }
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_E;
	req2->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives  MESI_MC_DEMAND_I requests.
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
        vector<Coh_msg>& l1_cresps = m_l1p->get_cache_resps();

	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+1, (int)mux_pkts.size() + (int)l1_cresps.size());
	list<int> dest_ids(NUM_SHARERS);
	int mux_cnt = mux_pkts.size();
	int l1_cnt = l1_cresps.size();

	//excluding the last MC_GRANT_E_DATA.
	if(SOURCE_ID == NODE_ID)
	    l1_cnt--;
	else
	    mux_cnt--;

	for(unsigned i=0; i<mux_cnt; i++) {
	    NetworkPacket& pkt = mux_pkts[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    //CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}
	for(unsigned i=0; i<l1_cnt; i++) {
	    Coh_msg cresp = l1_cresps[i];

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}

	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

        //verify the mock object receives  MESI_MC_GRANT_E_DATA request.
	{
	    Coh_msg cresp;

	    if(SOURCE_ID == NODE_ID) {
		cresp = l1_cresps[l1_cresps.size() - 1];
	    }
	    else {
		NetworkPacket& pkt = mux_pkts[mux_pkts.size()-1];
		CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
		CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
		CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
		CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
		CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

		cresp = *((Coh_msg*)(pkt.data));
	    }

	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	}

	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store hit in L1;
    //! client in S; manager in S.
    //!
    //! In a STORE hit when client is in S state, client sends MESI_CM_I_to_E to manager.
    //! As the manager is in S, it sends MESI_MC_DEMAND_I to all sharers except the requestor;
    //! the client is set to be owner and manager enters SIE; manager enters SIE. Sharers
    //! send MESI_CM_UNBLOCK_I to
    //! manager; after receiving from all sharers, manager sends MESI_MC_GRANT_E_DATA
    //! to client and enters IE. Client sends MESI_CM_UNBLOCK_E; manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->all sharers except requestor, MESI_MC_DEMAND_I
    //! 3. sharers--->M, MESI_CM_UNBLOCK_I
    //! 4. M--->C, MESI_MC_GRANT_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E
    void test_process_client_request_and_reply_store_hit_l1_S_l2_S_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_S;
	MESI_manager_state_t old_state = manager->state;

	//add a random number of sharers
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10
	int SHARERS_ID[NUM_SHARERS];
	for(int i=1; i<NUM_SHARERS; i++) {
	    SHARERS_ID[i] = random() % 1024;
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}
	//add requestor to sharersList.
	manager->sharersList.set(SOURCE_ID);
	SHARERS_ID[0] = SOURCE_ID; //requestor is a sharer.


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=1; i<NUM_SHARERS; i++) { //the first is requestor; it should not reply.
	    Coh_msg* req2 = new Coh_msg();
	    req2->type = Coh_msg :: COH_RPLY;
	    req2->addr = ADDR;
	    req2->msg = MESI_CM_UNBLOCK_I;
	    req2->src_id = SHARERS_ID[i];
	    if(SHARERS_ID[i] == NODE_ID) {
		Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	    }
	    else {
		NetworkPacket* pkt = new NetworkPacket;
		pkt->type = L2_cache :: COH_MSG;
		pkt->src = SOURCE_ID;
		pkt->src_port = LLP_cache :: LLP_ID;
		pkt->dst = NODE_ID;
		pkt->dst_port = LLP_cache :: LLS_ID;
		*((Coh_msg*)(pkt->data)) = *req2;
		pkt->data_size = sizeof(Coh_msg);
		delete req2;

		Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	    }
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_E;
	req2->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives  MESI_MC_DEMAND_I requests.
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
        vector<Coh_msg>& l1_cresps = m_l1p->get_cache_resps();

	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS-1 + 1, (int)mux_pkts.size() + (int)l1_cresps.size());
	list<int> dest_ids(NUM_SHARERS);
	for(unsigned i=0; i<mux_pkts.size()-1; i++) {
	    NetworkPacket& pkt = mux_pkts[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    //CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}
	for(unsigned i=0; i<l1_cresps.size(); i++) {
	    Coh_msg cresp = l1_cresps[i];

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}


	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=1; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

        //verify the mock object receives  MESI_MC_GRANT_E_DATA request.
	{
	    Coh_msg cresp;
	    if(SOURCE_ID == NODE_ID) {
		cresp = l1_cresps[l1_cresps.size()-1];
	    }
	    else {
		NetworkPacket& pkt = mux_pkts[mux_pkts.size()-1];
		CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
		CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
		CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
		CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
		CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

		cresp = *((Coh_msg*)(pkt.data));
	    }

	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	}


	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! victim in E; manager in E
    //!
    //! 1. C--->M, MESI_CM_E_to_I
    //! 2. M--->C, MESI_MC_GRANT_I, manager enters EI_PUT
    //! 4. C--->M, MESI_CM_UNBLOCK_I
    void test_process_client_request_and_reply_client_eviction_victim_E_l2_E_0()
    {
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_E_to_I;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = SOURCE_ID;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_I;
	req2->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_GRANT_I which was from manager to owner
	Coh_msg cresp;
	if(SOURCE_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_I, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);

	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table - invalidated
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_I, manager->state);
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(OWNER_ID));
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! victim in M; manager in E
    //!
    //! 1. C--->M, MESI_CM_M_to_I
    //! 2. M--->C, MESI_MC_GRANT_I, manager enters EI_PUT
    //! 4. C--->M, MESI_CM_UNBLOCK_I
    void test_process_client_request_and_reply_client_eviction_victim_M_l2_E_0()
    {
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_M_to_I;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = SOURCE_ID;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_I;
	req2->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_GRANT_I which was from manager to owner
	Coh_msg cresp;
	if(SOURCE_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(2, (int)mux_pkts.size()); //MC_GRANT_I, and one to MC

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_I, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table - invalidated
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_I, manager->state);
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(OWNER_ID));
	CPPUNIT_ASSERT_EQUAL(-1, manager->owner);
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! line in l2 evicted; victim in E; owner in E.
    //!
    //! Manually fill up a set; then send a request for the same set but miss;
    //! L2 would have to evict. If manager in E state, it sends MESI_MC_DEMAND_I
    //! to owner and reset owner; it enters EI_EVICT state.
    //! Owner sends MESI_CM_UNBLOCK_I; manager enters I state.
    //!
    //! 1. C--->M, MESI_CM_I_to_S;
    //! 2. M--->owner, MESI_MC_DEMAND_I, manager enters EI_EVICT
    //! 4. owner--->M, MESI_CM_UNBLOCK_I
    void test_process_client_request_and_reply_l2_eviction_victim_E_l1_E_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t set_entries[NUM_TAGS];

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

	    m_cachep->my_table->reserve_block_for(ad);
	    set_entries[i] = ad;

	}

	//The set is full.

	const paddr_t VICTIM_ADDR = set_entries[0];

	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put victim's state to E.
	hash_entry* victim_hash_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	victim_hash_entry->have_data = true;
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[victim_hash_entry->get_idx()]);
	manager->state = MESI_MNG_E;
	int OWNER_ID;
	while((OWNER_ID = random() % 1024) == SOURCE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = m_cachep->my_table->get_line_addr(VICTIM_ADDR);
	req2->msg = MESI_CM_UNBLOCK_I;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + 2*HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_DEMAND_I which was from manager to owner
	Coh_msg cresp;
	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(3, (int)mux_pkts.size()); //DEMAND_I, msg to MC, GRANT_E

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


        //verify the mock object receives a memory request
	{
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();

	    NetworkPacket& pkt = mux_pkts[1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);

	    Mem_msg mem_req = *((Mem_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mem_req.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mem_req.op_type);
	}


	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the hash table - victim evicted
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_IE, manager->state);
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(OWNER_ID));
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }






    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! line in l2 evicted; victim in E; owner in M.
    //!
    //! Manually fill up a set; then send a request for the same set but miss;
    //! L2 would have to evict. If manager in E state, it sends MESI_MC_DEMAND_I
    //! to owner and reset owner; it enters EI_EVICT state.
    //! Owner sends MESI_CM_UNBLOCK_I_DIRTY; manager writes back to memory and
    //! enters I state.
    //!
    //! 1. C--->M, MESI_CM_I_to_S;
    //! 2. M--->owner, MESI_MC_DEMAND_I, manager enters EI_EVICT
    //! 4. owner--->M, MESI_CM_UNBLOCK_I_DIRTY
    void test_process_client_request_and_reply_l2_eviction_victim_E_l1_M_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t set_entries[NUM_TAGS];

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

	    m_cachep->my_table->reserve_block_for(ad);
	    set_entries[i] = ad;

	}

	//The set is full.

	const paddr_t VICTIM_ADDR = set_entries[0];

	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put victim's state to E.
	hash_entry* victim_hash_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	victim_hash_entry->have_data = true;
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[victim_hash_entry->get_idx()]);
	manager->state = MESI_MNG_E;
	int OWNER_ID;
	while((OWNER_ID = random() % 1024) == SOURCE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = m_cachep->my_table->get_line_addr(VICTIM_ADDR);
	req2->msg = MESI_CM_UNBLOCK_I_DIRTY;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_DEMAND_I which was from manager to owner
	Coh_msg cresp;
	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();

	    if(SOURCE_ID != NODE_ID)
		CPPUNIT_ASSERT_EQUAL(4, (int)mux_pkts.size()); // 1 to owner, and 2 to MC: write back of evicted L2 line,
	                                                   // read for new LOAD request
							   // and one to source (MC_GRANT_E_DATA)
	    else
		CPPUNIT_ASSERT_EQUAL(3, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


        //verify the mock object receives a memory request
	{
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();

	    NetworkPacket& pkt = mux_pkts[1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);

	    Mem_msg mem_req = *((Mem_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), mem_req.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemSt, mem_req.op_type);


	    pkt = mux_pkts[2];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);

	    Mem_msg mem_req1 = *((Mem_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mem_req1.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mem_req1.op_type);
	}



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the hash table - victim evicted
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_IE, manager->state);
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(OWNER_ID));
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! line in l2 evicted; victim in S
    //!
    //! Manually fill up a set; then send a request for the same set but miss;
    //! L2 would have to evict. If manager in S state, it sends MESI_MC_DEMAND_I
    //! to all sharers; it enters SI_EVICT state.
    //! Each sharer sends MESI_CM_UNBLOCK_I to manager; after receiving all, it
    //! enters I state.
    //!
    //! 1. C--->M, MESI_CM_I_to_S;
    //! 2. M--->sharers, MESI_MC_DEMAND_I, manager enters SI_EVICT
    //! 3. sharers--->M, MESI_CM_UNBLOCK_I
    //! 4. M--->mem, read the new line
    //! 5. M<---mem, new line read
    //! 6. C<---M, MESI_MC_GRANT_E_DATA.
    void test_process_client_request_and_reply_l2_eviction_victim_E_l1_S_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

	//create NUM_TAGS unique tags
	set<paddr_t> addrset; //use a set to generate unique values

	paddr_t set_entries[NUM_TAGS];

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

	    m_cachep->my_table->reserve_block_for(ad);
	    set_entries[i] = ad;

	}

	//The set is full.

	const paddr_t VICTIM_ADDR = set_entries[0];

	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//manually put victim's state to E.
	hash_entry* victim_hash_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	victim_hash_entry->have_data = true;
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[victim_hash_entry->get_idx()]);
	manager->state = MESI_MNG_S;
	const int OWNER_ID = random() % 1024;
	manager->owner = -1;
	//add a random number of sharers
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    while((SHARERS_ID[i] = random() % 1024) == SOURCE_ID);
	    manager->sharersList.set(SHARERS_ID[i]);
	}



	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=0; i<NUM_SHARERS; i++) {
	    Coh_msg* req2 = new Coh_msg();
	    req2->type = Coh_msg :: COH_RPLY;
	    req2->addr = m_cachep->my_table->get_line_addr(VICTIM_ADDR);
	    req2->msg = MESI_CM_UNBLOCK_I;
	    req2->src_id = SHARERS_ID[i];
	    if(SHARERS_ID[i] == NODE_ID) {
		Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	    }
	    else {
		NetworkPacket* pkt = new NetworkPacket;
		pkt->type = L2_cache :: COH_MSG;
		pkt->src = SOURCE_ID;
		pkt->src_port = LLP_cache :: LLP_ID;
		pkt->dst = NODE_ID;
		pkt->dst_port = LLP_cache :: LLS_ID;
		*((Coh_msg*)(pkt->data)) = *req2;
		pkt->data_size = sizeof(Coh_msg);
		delete req2;

		Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	    }
	}
	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_DEMAND_I
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
        vector<Coh_msg>& l1_cresps = m_l1p->get_cache_resps();

	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2, (int)mux_pkts.size() + (int)l1_cresps.size()); // N  MC_DEMAND_I, MC_GRANT_E_DATA, and a mem request

	list<int> dest_ids(NUM_SHARERS);
	unsigned m_cnt = mux_pkts.size();
	unsigned l1_cnt = l1_cresps.size();
	//exclude MC_GRANT_E_DATA
	if(SOURCE_ID == NODE_ID)
	    l1_cnt--;
        else
	    m_cnt--;

	m_cnt--; //mem req
	for(unsigned i=0; i<m_cnt; i++) {
	    NetworkPacket& pkt = mux_pkts[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    //CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}
	for(unsigned i=0; i<l1_cnt; i++) {
	    Coh_msg cresp = l1_cresps[i];

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}


	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}


	if(SOURCE_ID == NODE_ID) {
	    Coh_msg cresp = l1_cresps[l1_cresps.size() - 1];

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	}
	else {
	    NetworkPacket& pkt = mux_pkts[mux_pkts.size() - 1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	}


        //verify the mock object receives a memory request
	{
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();

	    NetworkPacket pkt;

	    if(SOURCE_ID == NODE_ID) //GRANT_E goes to local l1
		pkt = mux_pkts[mux_pkts.size()-1];
	    else //GRANT_E goes to mux
		pkt = mux_pkts[mux_pkts.size()-2];

	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);

	    Mem_msg mreq = *((Mem_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);
	}


	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the hash table - victim evicted
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(VICTIM_ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_IE, manager->state);
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(SOURCE_ID));
	//CPPUNIT_ASSERT_EQUAL(true, manager->sharersList.get(OWNER_ID));
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request():
    //! no mshr entry available (i.e., C_MSHR_STALL).
    //!
    //! Create N requests to fill up the MSHR; then create one more request; it should
    //! have a C_MSHR_STALL.
    //!
    //! 1. C--->M, N MESI_CM_I_to_S.
    //! 2. Another request, which is stalled.
    //!
    //! Verify C_MSHR_STALL is created and put in the stall buffer.
    void test_process_client_request_MSHR_STALL_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->mshr->get_tag_mask();
	const int NUM_TAGS = m_cachep->mshr->assoc;
	assert(MSHR_SIZE == m_cachep->mshr->assoc);

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
	for(int i=0; i<NUM_TAGS; i++) { //send N requests to fill up MSHR
	    Coh_msg* req = new Coh_msg();
	    req->type = Coh_msg :: COH_REQ;
	    req->addr = addrs[i];
	    req->msg = MESI_CM_I_to_S;

	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule for the MockProc to send the cache_req
	//This request would MSHR_STALL
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->src_id = SOURCE_ID;
	req->rw = 0;

	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;



	Manifold::StopAt(When);
	Manifold::Run();


	//verify no mshr entry for the request.
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));

	//There should be a MSHR_STALL in the stall buffer.
	std::list<L2_cache::Stall_buffer_entry>::reverse_iterator it = m_cachep->stalled_client_req_buffer.rbegin();
	CPPUNIT_ASSERT_EQUAL(C_MSHR_STALL, (*it).type);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply():
    //! no mshr entry available (i.e., C_MSHR_STALL).
    //!
    //! Create N requests to fill up the MSHR; then create one more request; it should
    //! have a C_MSHR_STALL. Send a response to one of the N requests; a mshr entry is
    //! freed and the stalled request is woken up and reprocessed.
    //!
    //! 1. C--->M, N MESI_CM_I_to_S.
    //! 2. Another request, which is stalled.
    //!
    //! Verify C_MSHR_STALL is removed from stall buffer.
    void test_process_client_request_and_reply_MSHR_STALL_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->mshr->get_tag_mask();
	const int NUM_TAGS = m_cachep->mshr->assoc;
	assert(MSHR_SIZE == m_cachep->mshr->assoc);

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
	for(int i=0; i<NUM_TAGS; i++) { //send N requests to fill up MSHR
	    Coh_msg* req = new Coh_msg();
	    req->type = Coh_msg :: COH_REQ;
	    req->addr = addrs[i];
	    req->msg = MESI_CM_I_to_S;

	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule for the MockProc to send the cache_req
	//This request would MSHR_STALL
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->src_id = SOURCE_ID;
	req->rw = 0;

	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a reply to one of the requests to free an mshr
	Coh_msg* reply = new Coh_msg();
	reply->type = Coh_msg :: COH_RPLY;
	const paddr_t REPLY_ADDR = addrs[random() % NUM_TAGS];
	reply->addr = REPLY_ADDR;
	reply->msg = MESI_CM_UNBLOCK_E;

	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, reply);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *reply;
	    pkt->data_size = sizeof(Coh_msg);
	    delete reply;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;




	Manifold::StopAt(When);
	Manifold::Run();


	//verify mshr entry allocated for the request.
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR));

	//There should not be a MSHR_STALL in the stall buffer.
	for(std::list<L2_cache::Stall_buffer_entry>::iterator it = m_cachep->stalled_client_req_buffer.begin();
	       it != m_cachep->stalled_client_req_buffer.end(); ++it) {
	    CPPUNIT_ASSERT(C_MSHR_STALL != (*it).type);
        }

    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request():
    //! an mshr entry for the same line already exists (i.e., PREV_PEND_STALL).
    //!
    //! Send a request; then send another with address of the same line. The 2nd
    //! request should have a C_PREV_PEND_STALL.
    //!
    //! Verify C_PREV_PEND_STALL is created and put in the stall buffer.
    void test_process_client_request_PREV_PEND_STALL_0()
    {
	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule a request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->src_id = SOURCE_ID;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->rw = 0;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request for the same line
	//This request would PREV_PEND_STALL
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == ADDR);

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), m_cachep->my_table->get_line_addr(ADDR2));

	Coh_msg* req2= new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR2;
	int SOURCE_ID2;
	while((SOURCE_ID2 = random() % 1024) == SOURCE_ID);
	req2->src_id = SOURCE_ID2;
	req2->msg = MESI_CM_I_to_S; //a load miss
	req2->rw = 0;

	if(SOURCE_ID2 == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID2;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();


	//There should be a PREV_PEND_STALL in the stall buffer.
	std::list<L2_cache::Stall_buffer_entry>::reverse_iterator it = m_cachep->stalled_client_req_buffer.rbegin();
	CPPUNIT_ASSERT_EQUAL(C_PREV_PEND_STALL, (*it).type);
	CPPUNIT_ASSERT_EQUAL(ADDR2, (*it).req->addr);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request():
    //! an mshr entry for the same line already exists (i.e., PREV_PEND_STALL).
    //!
    //! Send a request; then send another with address of the same line. The 2nd
    //! request should have a C_PREV_PEND_STALL.
    //! Send a reply to allow the 1st request to finish; the 2nd is processed.
    //!
    //! Verify C_PREV_PEND_STALL is removed from the stall buffer.
    void test_process_client_request_and_reply_PREV_PEND_STALL_0()
    {
	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule a request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->src_id = SOURCE_ID;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->rw = 0;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request for the same line
	//This request would PREV_PEND_STALL
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == ADDR);

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), m_cachep->my_table->get_line_addr(ADDR2));

	Coh_msg* req2= new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR2;
	int SOURCE_ID2;
	while((SOURCE_ID2 = random() % 1024) == SOURCE_ID);
	req2->src_id = SOURCE_ID2;
	req2->msg = MESI_CM_I_to_S; //a load miss
	req2->rw = 0;

	if(SOURCE_ID2 == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID2;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a reply to allow the 1st request to finish.
	Coh_msg* reply = new Coh_msg();
	reply->type = Coh_msg :: COH_RPLY;
	reply->addr = ADDR;
	reply->msg = MESI_CM_UNBLOCK_E; //a load miss

	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, reply);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID2;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *reply;
	    pkt->data_size = sizeof(Coh_msg);
	    delete reply;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();


	//There should NOT be a PREV_PEND_STALL in the stall buffer.
	for(std::list<L2_cache::Stall_buffer_entry>::iterator it = m_cachep->stalled_client_req_buffer.begin();
	       it != m_cachep->stalled_client_req_buffer.end(); ++it) {
	    CPPUNIT_ASSERT(C_PREV_PEND_STALL != (*it).type);
        }

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): LRU_BUSY_STALL.
    //!
    //! Fill up a cache set; then send a request with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the evicted entry is in a transient state, the new request should have
    //! a LRU_BUSY_STALL.
    //! Verify a LRU_BUSY_STALL is created and put in the stall buffer.
    void test_process_client_request_LRU_BUSY_STALL_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

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
	//schedule NUM_TAGS requests to make the set full
	for(int i=0; i<NUM_TAGS; i++) {
	    Coh_msg* req = new Coh_msg();
	    req->type = Coh_msg :: COH_REQ;
	    req->addr = addrs[i];
	    req->msg = MESI_CM_I_to_S; //a load miss
	    req->rw = 0;
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->src_id = SOURCE_ID;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->rw = 0;

	//schedule the req, which would LRU_BUSY_STALL
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

	//verify state of the MESI_L2_cache
	//for the new request
	//There should NOT be an entry in the MSHR.
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR)); //LRU_BUSY_STALLed request has given up its mshr
	//There should NOT be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(ADDR));

	//There should be a LRU_BUSY_STALL in the stall buffer.
	std::list<L2_cache::Stall_buffer_entry>::reverse_iterator it = m_cachep->stalled_client_req_buffer.rbegin();
	CPPUNIT_ASSERT_EQUAL(C_LRU_BUSY_STALL, (*it).type);
	CPPUNIT_ASSERT_EQUAL(ADDR, (*it).req->addr);
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): LRU_BUSY_STALL.
    //!
    //! Fill up a cache set; then send a request with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the evicted entry is in a transient state, the new request should have
    //! a LRU_BUSY_STALL.
    //! Verify LRU_BUSY_STALL is removed from the stall buffer and processed.
    void test_process_client_request_and_reply_LRU_BUSY_STALL_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

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
	//schedule NUM_TAGS requests to make the set full
	for(int i=0; i<NUM_TAGS; i++) {
	    Coh_msg* req = new Coh_msg();
	    req->type = Coh_msg :: COH_REQ;
	    req->addr = addrs[i];
	    req->msg = MESI_CM_I_to_S; //a load miss
	    req->rw = 0;
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request for the same set
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0) //50% percent chance from the local LLP.
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->src_id = SOURCE_ID;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->rw = 0;

	//schedule the req, which would LRU_BUSY_STALL
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
        }
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a response to allow the victim to complete its request and then be evicted.
	Coh_msg* reply = new Coh_msg();
	reply->type = Coh_msg :: COH_RPLY;
	reply->addr = VICTIM_ADDR;
	reply->msg = MESI_CM_UNBLOCK_E;
	reply->rw = 0;

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, reply);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

	//verify state of the MESI_L2_cache
	//for the new request
	//There should be an entry in the MSHR.
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR));
	//There should NOT be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(ADDR));

	//There should NOT be a LRU_BUSY_STALL in the stall buffer.
	for(std::list<L2_cache::Stall_buffer_entry>::iterator it = m_cachep->stalled_client_req_buffer.begin();
	       it != m_cachep->stalled_client_req_buffer.end(); ++it) {
	    CPPUNIT_ASSERT(C_LRU_BUSY_STALL != (*it).type);
        }

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): TRANS_STALL: hit entry beying evicted.
    //!
    //! Fill up a cache set and put them in the E state, then send a request with a new
    //! tag for the same set. This causes the LRU entry to be evicted.
    //! While the victim is being evicted, send another request that hits on the victim.
    //! This request would have a C_TRANS_STALL.
    //! Verify a C_TRANS_STALL is created and put in the stall buffer.
    void test_process_client_request_TRANS_STALL_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

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
	    m_cachep->my_table->reserve_block_for(ad);
	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];

	//put the state of all the lines in the set to E
	for(int i=0; i<NUM_TAGS; i++) {
	    hash_entry* entry = m_cachep->my_table->get_entry(addrs[i]);
	    assert(entry);
	    MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	    manager->state = MESI_MNG_E;
	}



	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a request which will miss and cause an eviction
	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->rw = 0;

	if(random() % 2 == 0) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request which will hit on the victim
	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(VICTIM_ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == VICTIM_ADDR);

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR2;
	req2->msg = MESI_CM_I_to_S; //a load miss
	req2->rw = 0;

	if(random() % 2 == 0) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



	//verify state of the MESI_L1_cache
	//for the new request
	//There should NOT be an entry in the MSHR.
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR2)); //TRANS_STALLed request has given up its mshr

	//There should be a TRANS_STALL in the stall buffer.
	std::list<L2_cache::Stall_buffer_entry>::reverse_iterator it = m_cachep->stalled_client_req_buffer.rbegin();
	CPPUNIT_ASSERT_EQUAL(C_TRANS_STALL, (*it).type);
	CPPUNIT_ASSERT_EQUAL(ADDR2, (*it).req->addr);
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): TRANS_STALL:
    //! hit entry beying evicted.
    //!
    //! Fill up a cache set and put them in the E state, then send a request with a new
    //! tag for the same set. This causes the LRU entry to be evicted.
    //! While the victim is being evicted, send another request that hits on the victim.
    //! This request would have a C_TRANS_STALL.
    //! Send a reply to the victim, which wakes up the 1st request.
    //! Send a reply to the 1st request, which wakes up the 2nd request (TRANS_STALL).
    //!
    //! Verify C_TRANS_STALL is removed from the stall buffer and processed.
    void test_process_client_request_and_reply_TRANS_STALL_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

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
	    m_cachep->my_table->reserve_block_for(ad);
	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];

	//put the state of all the lines in the set to E
	for(int i=0; i<NUM_TAGS; i++) {
	    hash_entry* entry = m_cachep->my_table->get_entry(addrs[i]);
	    assert(entry);
	    MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	    manager->state = MESI_MNG_E;
	}



	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a request which will miss and cause an eviction
	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->rw = 0;

	if(random() % 2 == 0) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request which will hit on the victim
	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(VICTIM_ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == VICTIM_ADDR);

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR2;
	req2->msg = MESI_CM_I_to_S; //a load miss
	req2->rw = 0;

	if(random() % 2 == 0) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a reply to the victim to allow the eviction to clomplete and wake up the 1st request.
	Coh_msg* reply = new Coh_msg();
	reply->type = Coh_msg :: COH_RPLY;
	reply->addr = VICTIM_ADDR;
	reply->msg = MESI_CM_UNBLOCK_I;
	reply->rw = 0;

	if(random() % 2 == 0) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, reply);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *reply;
	    pkt->data_size = sizeof(Coh_msg);
	    delete reply;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a reply to the 1st request to allow it to complete and wake up the 2nd request.
	Coh_msg* reply2 = new Coh_msg();
	reply2->type = Coh_msg :: COH_RPLY;
	reply2->addr = ADDR;
	reply2->msg = MESI_CM_UNBLOCK_E;
	reply2->rw = 0;

	if(random() % 2 == 0) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, reply2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    //pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *reply2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete reply2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



	//verify state of the MESI_L1_cache
	//for the new request
	//There should be an entry in the MSHR.
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->mshr->has_match(ADDR2)); //TRANS_STALLed request has given up its mshr

	//There should NOT be a TRANS_STALL in the stall buffer.
	for(std::list<L2_cache::Stall_buffer_entry>::iterator it = m_cachep->stalled_client_req_buffer.begin();
	       it != m_cachep->stalled_client_req_buffer.end(); ++it) {
	    CPPUNIT_ASSERT(C_TRANS_STALL != (*it).type);
        }
    }
















//################
//Request race
//################

    //======================================================================
    //======================================================================
    //! @brief Test request race: I_to_E and E_to_I.
    //!
    //! When manager receives I_to_E, it sends FWD_E to owner, and enters EE state.
    //! If at this point owner starts eviction and sends to manager E_to I, the
    //! request would PREV_PEND_STALL. Owner has to complete the FWD_E first (it
    //! has to handle FWD_E in IE state). Then manager would process E_to_I in E
    //! state with owner set to the new owner. It simply ignores the E_to_I.
    //!
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    //! 3. Owner --> manager, CM_E_to_I; this PREV_PEND_STALL
    //! 4. C--->M, MESI_CM_UNBLOCK_E.
    //! 5. M gets E_to_I in E state with different owner.
    //!
    void test_request_race_l1_E_to_I_l2_FWD_E_0()
    {
	const paddr_t ADDR = random();
	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	manager->owner = OWNER_ID;



	Manifold::unhalt();
	Ticks_t When = 1;

	//create a request
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == OWNER_ID); //SOURCE_ID different from owner
	if(random() % 2 == 0 && OWNER_ID != NODE_ID)
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR;
	req2->msg = MESI_CM_E_to_I;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//1st requestor sends UNBLOCK_E to allow the first request to finish.
	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_E;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives FWD_E, only
        Coh_msg cresp;

	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: I_to_S and E_to_I.
    //!
    //! When manager receives I_to_S, it sends FWD_S to owner, and enters ES state.
    //! If at this point owner starts eviction and sends to manager E_to I, the
    //! request would PREV_PEND_STALL. Owner has to complete the FWD_S first (it
    //! has to handle FWD_S in IE state). Then manager would process E_to_I in E
    //! state. It simply ignores the E_to_I, and transitions to E.
    //!
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    //! 3. Owner --> manager, CM_E_to_I; this PREV_PEND_STALL
    //! 4. Owner(got FWD_S in EI) --> peer, CC_E_DATA
    //! 5. C--->M, MESI_CM_UNBLOCK_E.
    //! 6. M gets E_to_I in E state.
    //!
    void test_request_race_l1_E_to_I_l2_FWD_S_0()
    {
	const paddr_t ADDR = random();
	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	manager->owner = OWNER_ID;



	Manifold::unhalt();
	Ticks_t When = 1;

	//create a LOAD request
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == OWNER_ID); //SOURCE_ID different from owner
	if(random() % 2 == 0 && OWNER_ID != NODE_ID)
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR;
	req2->msg = MESI_CM_E_to_I;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//owner sends CC_E_DATA to allow the first request to finish.
	//This causes the requetor to send CM_UNBLOCK_E to manager.
	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_E;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives FWD_S, only
        Coh_msg cresp;

	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}


	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);


	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: I_to_E and M_to_I.
    //!
    //! When manager receives I_to_E, it sends FWD_E to owner, and enters EE state.
    //! If at this point owner starts eviction and sends to manager M_to I, the
    //! request would PREV_PEND_STALL. Owner has to complete the FWD_E first (it
    //! has to handle FWD_E in IE state). Then manager would process M_to_I in E
    //! state with owner set to the new owner. It simply ignores the M_to_I.
    //!
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    //! 3. Owner --> manager, CM_M_to_I; this PREV_PEND_STALL
    //! 4. C--->M, MESI_CM_UNBLOCK_E.
    //! 5. M gets M_to_I in E state with different owner.
    //!
    void test_request_race_l1_M_to_I_l2_FWD_E_0()
    {
	const paddr_t ADDR = random();
	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	manager->owner = OWNER_ID;



	Manifold::unhalt();
	Ticks_t When = 1;

	//create a request
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == OWNER_ID); //SOURCE_ID different from owner
	if(random() % 2 == 0 && OWNER_ID != NODE_ID)
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR;
	req2->msg = MESI_CM_M_to_I;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//1st requestor sends UNBLOCK_E to allow the first request to finish.
	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_E;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives FWD_E, only
        Coh_msg cresp;

	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }



    //======================================================================
    //======================================================================
    //! @brief Test request race: I_to_S and M_to_I.
    //!
    //! When manager receives I_to_S, it sends FWD_S to owner, and enters ES state.
    //! If at this point owner starts eviction and sends to manager M_to I, the
    //! request would PREV_PEND_STALL. Owner has to complete the FWD_S first (it
    //! has to handle FWD_S in IE state). Then manager would process M_to_I in E
    //! state. It simply ignores the M_to_I, and transitions to E.
    //!
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    //! 3. Owner --> manager, CM_M_to_I; this PREV_PEND_STALL
    //! 4. Owner--->peer, MESI_CC_M_DATA.
    //! 5. C--->M, MESI_CM_UNBLOCK_E.
    //! 6. M gets M_to_I in E state.
    //!
    void test_request_race_l1_M_to_I_l2_FWD_S_0()
    {
	const paddr_t ADDR = random();
	//manually put the ADDR in the hash table, and set the manager state to E.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	manager->owner = OWNER_ID;



	Manifold::unhalt();
	Ticks_t When = 1;

	//create a LOAD request
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == OWNER_ID); //SOURCE_ID different from owner
	if(random() % 2 == 0 && OWNER_ID != NODE_ID)
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR;
	req2->msg = MESI_CM_M_to_I;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//owner sends CC_M_DATA to requestor to allow the first request to finish.
	//This causes requestor to send CM_UNBLOCK_E to manager.
	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_E;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}


	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives FWD_S, only
        Coh_msg cresp;

	if(OWNER_ID == NODE_ID) {
	    vector<Coh_msg>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    cresp = cresps[0];
	}
	else {
	    vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mux_pkts.size());

	    NetworkPacket& pkt = mux_pkts[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    cresp = *((Coh_msg*)(pkt.data));
	}

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, manager->owner);
    }





    //======================================================================
    //======================================================================
    //! @brief Test request race: DEMAND_I and E_to_I.
    //!
    //! When manager starts evicting a line, it sends DEMAND_I to owner. 
    //! If at this point owner starts eviction and sends to manager E_to I, the
    //! request would hit and TRANS_STALL. Owner has to complete the DEMAND_I first (it
    //! has to handle DEMAND_I in EI state). Then manager would process E_to_I and realize
    //! it's an invalidation request but misses in the cache. So it's simply dropped.
    //!
    //! Fill up a set, set LRU state to E. Send a load request, manager would evict
    //! LRU. At the same time send E_to_I from owner to manager.
    //!
    //! 1. Fill up a set and set LRU state to E.
    //! 2. C-->M, I_to_S.
    //! 3. M-->Owner, DEMAND_I.
    //! 4. Owner --> manager, CM_E_to_I; this TRANS_STALL
    //! 5. Owner--->M, MESI_CM_UNBLOCK_I.
    //! 6. Eviction completes, manager-->C, MC_GRANT_E_DATA.
    //! 7. C-->M, UNBLOCK_E. Manager enters E.
    //! 8. E_to_I is ignored by cache.
    //!
    void test_request_race_l1_E_to_I_l2_DEMAND_I_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

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

	//put the tags in the hash table.
	for(int i=0; i<NUM_TAGS; i++) {
	    m_cachep->my_table->reserve_block_for(addrs[i]);
	    assert(m_cachep->my_table->has_match(addrs[i]));
        }


	//set victim's manager state to E.
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	MESI_manager* victim_manager = dynamic_cast<MESI_manager*>(m_cachep->managers[victim_entry->get_idx()]);
	victim_manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	victim_manager->owner = OWNER_ID;



	Manifold::unhalt();
	Ticks_t When = 1;

	//create a request
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == OWNER_ID); //SOURCE_ID different from owner
	if(random() % 2 == 0 && OWNER_ID != NODE_ID)
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->src_id = SOURCE_ID;
	req->msg = MESI_CM_I_to_S;
	req->rw = 0;

	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = VICTIM_ADDR;
	req2->msg = MESI_CM_E_to_I;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//owner sends UNBLOCK_I to allow eviction to finish.
	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = VICTIM_ADDR;
	req3->msg = MESI_CM_UNBLOCK_I;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//Now first request is processed.
	//1st requestor sends UNBLOCK_E to allow 1st request to finish.
	Coh_msg* req4 = new Coh_msg();
	req4->type = Coh_msg :: COH_RPLY;
	req4->addr = ADDR;
	req4->msg = MESI_CM_UNBLOCK_E;
	req4->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req4);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req4;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req4;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives DEMAND_I for owner, and GRANT_E for 1st requestor.
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
        vector<Coh_msg>& l1_cresps = m_l1p->get_cache_resps();

	CPPUNIT_ASSERT_EQUAL(3, (int)mux_pkts.size() + (int)l1_cresps.size());

	int mux_idx = 0;
	int l1_idx = 0;

	if(OWNER_ID == NODE_ID) {
	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), l1_cresps[l1_idx].addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, l1_cresps[l1_idx].msg);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, l1_cresps[l1_idx].dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, l1_cresps[l1_idx].dst_port);
	    l1_idx++;
	}
	else {
	    NetworkPacket& pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    mux_idx++;
	}

	if(SOURCE_ID == NODE_ID) {
	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), l1_cresps[l1_idx].addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, l1_cresps[l1_idx].msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, l1_cresps[l1_idx].dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, l1_cresps[l1_idx].dst_port);
	}
	else {
	    //the next pkt is to MC for the read request
	    NetworkPacket& pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	    mux_idx++;

	    pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	}


	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());
	//victim evicted.
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(VICTIM_ADDR));

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, victim_manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, victim_manager->owner);
    }





    //======================================================================
    //======================================================================
    //! @brief Test request race: DEMAND_I and M_to_I.
    //!
    //! When manager starts evicting a line, it sends DEMAND_I to owner. 
    //! If at this point owner starts eviction and sends to manager M_to I, the
    //! request would hit and TRANS_STALL. Owner has to complete the DEMAND_I first (it
    //! has to handle DEMAND_I in MI state). Then manager would process M_to_I and realize
    //! it's an invalidation request but misses in the cache. So it's simply dropped.
    //!
    //! Fill up a set, set LRU state to E. Send a load request, manager would evict
    //! LRU. At the same time send M_to_I from owner to manager.
    //!
    //! 1. Fill up a set and set LRU state to E.
    //! 2. C-->M, I_to_S.
    //! 3. M-->Owner, DEMAND_I.
    //! 4. Owner --> manager, CM_M_to_I; this TRANS_STALL
    //! 5. Owner--->M, MESI_CM_UNBLOCK_I_DIRTY.
    //! 6. Eviction completes, manager-->C, MC_GRANT_E_DATA.
    //! 7. C-->M, UNBLOCK_E. Manager enters E.
    //! 8. M_to_I is ignored by cache.
    //!
    void test_request_race_l1_M_to_I_l2_DEMAND_I_0()
    {
	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

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

	//put the tags in the hash table.
	for(int i=0; i<NUM_TAGS; i++) {
	    m_cachep->my_table->reserve_block_for(addrs[i]);
	    assert(m_cachep->my_table->has_match(addrs[i]));
        }


	//set victim's manager state to E.
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	MESI_manager* victim_manager = dynamic_cast<MESI_manager*>(m_cachep->managers[victim_entry->get_idx()]);
	victim_manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	victim_manager->owner = OWNER_ID;



	Manifold::unhalt();
	Ticks_t When = 1;

	//create a request
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == OWNER_ID); //SOURCE_ID different from owner
	if(random() % 2 == 0 && OWNER_ID != NODE_ID)
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = VICTIM_ADDR;
	req2->msg = MESI_CM_M_to_I;
	req2->src_id = OWNER_ID;
	if(OWNER_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//owner sends UNBLOCK_I_DIRTY to allow eviction to finish.
	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = VICTIM_ADDR;
	req3->msg = MESI_CM_UNBLOCK_I_DIRTY;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//Now first request is processed.
	//1st requestor sends UNBLOCK_E to allow 1st request to finish.
	Coh_msg* req4 = new Coh_msg();
	req4->type = Coh_msg :: COH_RPLY;
	req4->addr = ADDR;
	req4->msg = MESI_CM_UNBLOCK_E;
	req4->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req4);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req4;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req4;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives DEMAND_I for owner, and GRANT_E for 1st requestor.
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
        vector<Coh_msg>& l1_cresps = m_l1p->get_cache_resps();

	CPPUNIT_ASSERT_EQUAL(4, (int)mux_pkts.size() + (int)l1_cresps.size()); //DEMAND_I, write back for M_to_I,
	                                                                       //read from mem for 1st requestor,
									       //GRANT_E

	int mux_idx = 0;
	int l1_idx = 0;

	if(OWNER_ID == NODE_ID) {
	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), l1_cresps[l1_idx].addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, l1_cresps[l1_idx].msg);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, l1_cresps[l1_idx].dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, l1_cresps[l1_idx].dst_port);
	    l1_idx++;
	}
	else {
	    NetworkPacket& pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    mux_idx++;
	}

	if(SOURCE_ID == NODE_ID) {
	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), l1_cresps[l1_idx].addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, l1_cresps[l1_idx].msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, l1_cresps[l1_idx].dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, l1_cresps[l1_idx].dst_port);
	}
	else {
	    //the next pkt is to MC for write back
	    NetworkPacket& pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	    mux_idx++;

	    //the next pkt is to MC for the read request
	    pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	    mux_idx++;

	    pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	}



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());
	//victim evicted.
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->my_table->has_match(VICTIM_ADDR));

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, victim_manager->state);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, victim_manager->owner);
    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: DEMAND_I and I_to_E (I_to_E actually means S_to_E).
    //!
    //! When manager in S starts evicting a line, it sends DEMAND_I to sharers. 
    //! If at this point a sharer has a write request and sends to manager I_to E, the
    //! request would PREV_PEND_STALL. Requestor has to complete the DEMAND_I first (it
    //! has to handle DEMAND_I in SE state). Then manager would process I_to_E.
    //!
    //! This race condition is not as interesting for the manager as it is for the client.
    //!
    //! Manually put an addr in hash table. Put manager in S state and add a few sharers.
    //! Send an I_to_E from a non-sharer. Manager would send DEMAND_I to all sharers. At this
    //! point send I_to_E from a sharer.
    //!
    //! 2. Client -->M, I_to_E.
    //! 3. M-->sharers, DEMAND_I.
    //! 4. Sharer 1 --> M, I_to_E.
    //! 5. Sharers --->M, MESI_CM_UNBLOCK_I.
    //! 6. Manager-->Client, MC_GRANT_E_DATA.
    //! 7. Client -->M, UNBLOCK_E. Manager enters E.
    //! 8. Now manager processes I_to_E from sharer 1. Manager --> client, FWD_E.
    //! 9. Sharer1 --> Manager, UNBLOCK_E; manager enters E; owner set to sharer 1.
    //!
    void test_request_race_l1_I_to_E_l2_DEMAND_I_0()
    {
	//create a request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0)
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_S;
	MESI_manager_state_t old_state = manager->state;

	//add a random number of sharers
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		while((SHARERS_ID[i] = random() % 1024) == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) //make sure ID's are unique.
		    if(SHARERS_ID[j] == SHARERS_ID[i]) {
		        ok = false;
		        break;
		    }
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//sharer 1 sends I_to_E
	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR;
	req2->msg = MESI_CM_I_to_E;
	req2->src_id = SHARERS_ID[0];
	if(SHARERS_ID[0] == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}



	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=0; i<NUM_SHARERS; i++) { //all sharers reply
	    Coh_msg* req2 = new Coh_msg();
	    req2->type = Coh_msg :: COH_RPLY;
	    req2->addr = ADDR;
	    req2->msg = MESI_CM_UNBLOCK_I;
	    req2->src_id = SHARERS_ID[i];
	    if(SHARERS_ID[i] == NODE_ID) {
		Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	    }
	    else {
		NetworkPacket* pkt = new NetworkPacket;
		pkt->type = L2_cache :: COH_MSG;
		pkt->src = SOURCE_ID;
		pkt->src_port = LLP_cache :: LLP_ID;
		pkt->dst = NODE_ID;
		pkt->dst_port = LLP_cache :: LLS_ID;
		*((Coh_msg*)(pkt->data)) = *req2;
		pkt->data_size = sizeof(Coh_msg);
		delete req2;

		Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	    }
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//requestor reply with UNBLOCK_E
	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_E;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//sharer 1 reply with UNBLOCK_E
	Coh_msg* req5 = new Coh_msg();
	req5->type = Coh_msg :: COH_RPLY;
	req5->addr = ADDR;
	req5->msg = MESI_CM_UNBLOCK_E;
	req5->src_id = SOURCE_ID;
	if(SHARERS_ID[0] == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req5);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req5;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req5;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives  MESI_MC_DEMAND_I requests.
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
        vector<Coh_msg>& l1_cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2, (int)mux_pkts.size() + (int)l1_cresps.size());

	list<int> dest_ids(NUM_SHARERS);
	int mux_cnt = mux_pkts.size();
	int l1_cnt = l1_cresps.size();
	if(SOURCE_ID == NODE_ID) //SOURCE_ID also becomes the new owner
	    l1_cnt -= 2; //some DEMAND_I followed by GRANT_E_DATA and FWD_E
	else
	    mux_cnt -= 2; //some DEMAND_I followed by get from mem, GRANT_E_DATA, FWD_E,


	for(unsigned i=0; i<mux_cnt; i++) {
	    NetworkPacket& pkt = mux_pkts[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    //CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}
	for(unsigned i=0; i<l1_cnt; i++) {
	    Coh_msg cresp = l1_cresps[i];

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}

	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

        //verify the mock object receives GRANT_E_DATA and FWD_E
	int l1_idx = l1_cnt;
	int mux_idx = mux_cnt;
	if(SOURCE_ID == NODE_ID) {
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, l1_cresps[l1_idx].msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, l1_cresps[l1_idx].dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, l1_cresps[l1_idx].dst_port);
	    l1_idx++;
	}
	else {
	    NetworkPacket& pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    mux_idx++;
	}

	if(SOURCE_ID == NODE_ID) {
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, l1_cresps[l1_idx].msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, l1_cresps[l1_idx].dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, l1_cresps[l1_idx].dst_port);
	}
	else {
	    NetworkPacket& pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	}



	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SHARERS_ID[0], manager->owner);
	CPPUNIT_ASSERT_EQUAL(0, (int)manager->sharersList.count());

    }





    //======================================================================
    //======================================================================
    //! @brief Test request race: DEMAND_I and I_to_E
    //!
    //! When manager in S starts evicting a line, it sends DEMAND_I to sharers. 
    //! If at this point a sharer that has evicted in L1 before has a write request
    //! and sends to manager I_to E, the
    //! request would PREV_PEND_STALL. Requestor has to complete the DEMAND_I first (it
    //! has to handle DEMAND_I in IE state). Then manager would process I_to_E.
    //!
    //! From manager's point view, this test case has no difference from the case where
    //! the I_to_E was sent by client in SE state.
    //!
    //! Manually put an addr in hash table. Put manager in S state and add a few sharers.
    //! Send an I_to_E from a non-sharer. Manager would send DEMAND_I to all sharers. At this
    //! point send I_to_E from a sharer.
    //!
    //! 2. Client -->manager, I_to_E.
    //! 3. Manager -->sharers, DEMAND_I.
    //! 4. Sharer 1 --> manager, I_to_E.
    //! 5. Sharers --->manager, MESI_CM_UNBLOCK_I.
    //! 6. Manager-->Client, MC_GRANT_E_DATA.
    //! 7. Client -->manager, UNBLOCK_E. Manager enters E.
    //! 8. Now manager processes I_to_E from sharer 1. Manager --> client, FWD_E.
    //! 9. Sharer1 --> Manager, UNBLOCK_E; manager enters E; owner set to sharer 1.
    //!
    void test_request_race_l1_I_to_E_in_IE_l2_DEMAND_I_0()
    {
	//create a request
	const paddr_t ADDR = random();
	int SOURCE_ID = random() % 1024;
	if(random() % 2 == 0)
	    SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	//manually put the ADDR in the hash table, and set the manager state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);
	entry->have_data = true;

	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	manager->state = MESI_MNG_S;
	MESI_manager_state_t old_state = manager->state;

	//add a random number of sharers
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		while((SHARERS_ID[i] = random() % 1024) == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) //make sure ID's are unique.
		    if(SHARERS_ID[j] == SHARERS_ID[i]) {
		        ok = false;
		        break;
		    }
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//sharer 1 sends I_to_E
	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_REQ;
	req2->addr = ADDR;
	req2->msg = MESI_CM_I_to_E;
	req2->src_id = SHARERS_ID[0];
	if(SHARERS_ID[0] == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req2;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req2;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}


	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=0; i<NUM_SHARERS; i++) { //all sharers reply
	    Coh_msg* req2 = new Coh_msg();
	    req2->type = Coh_msg :: COH_RPLY;
	    req2->addr = ADDR;
	    req2->msg = MESI_CM_UNBLOCK_I;
	    req2->src_id = SHARERS_ID[i];
	    if(SHARERS_ID[i] == NODE_ID) {
		Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	    }
	    else {
		NetworkPacket* pkt = new NetworkPacket;
		pkt->type = L2_cache :: COH_MSG;
		pkt->src = SOURCE_ID;
		pkt->src_port = LLP_cache :: LLP_ID;
		pkt->dst = NODE_ID;
		pkt->dst_port = LLP_cache :: LLS_ID;
		*((Coh_msg*)(pkt->data)) = *req2;
		pkt->data_size = sizeof(Coh_msg);
		delete req2;

		Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	    }
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//requestor reply with UNBLOCK_E
	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_E;
	req3->src_id = SOURCE_ID;
	if(SOURCE_ID == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req3;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req3;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;

	//sharer 1 reply with UNBLOCK_E
	Coh_msg* req5 = new Coh_msg();
	req5->type = Coh_msg :: COH_RPLY;
	req5->addr = ADDR;
	req5->msg = MESI_CM_UNBLOCK_E;
	req5->src_id = SOURCE_ID;
	if(SHARERS_ID[0] == NODE_ID) {
	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, req5);
	}
	else {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = L2_cache :: COH_MSG;
	    pkt->src = SOURCE_ID;
	    pkt->src_port = LLP_cache :: LLP_ID;
	    pkt->dst = NODE_ID;
	    pkt->dst_port = LLP_cache :: LLS_ID;
	    *((Coh_msg*)(pkt->data)) = *req5;
	    pkt->data_size = sizeof(Coh_msg);
	    delete req5;

	    Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	}

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives  MESI_MC_DEMAND_I requests.
        vector<NetworkPacket>& mux_pkts = m_muxp->get_pkts();
        vector<Coh_msg>& l1_cresps = m_l1p->get_cache_resps();

	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2, (int)mux_pkts.size() + (int)l1_cresps.size());
	list<int> dest_ids(NUM_SHARERS);

	int mux_cnt = mux_pkts.size();
	int l1_cnt = l1_cresps.size();
	if(SOURCE_ID == NODE_ID) //SOURCE_ID also becomes the new owner
	    l1_cnt -= 2;
	else
	    mux_cnt -= 2;

	for(int i=0; i<mux_cnt; i++) {
	    NetworkPacket& pkt = mux_pkts[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    //CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}
	for(int i=0; i<l1_cnt; i++) {
	    Coh_msg cresp = l1_cresps[i];

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    dest_ids.push_back(cresp.dst_id);
	}




	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

        //verify the mock object receives GRANT_E_DATA and FWD_E
	int l1_idx = l1_cnt;
	int mux_idx = mux_cnt;
	if(SOURCE_ID == NODE_ID) {
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, l1_cresps[l1_idx].msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, l1_cresps[l1_idx].dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, l1_cresps[l1_idx].dst_port);
	    l1_idx++;
	}
	else {
	    NetworkPacket& pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	    mux_idx++;
	}

	if(SOURCE_ID == NODE_ID) {
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, l1_cresps[l1_idx].msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, l1_cresps[l1_idx].dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, l1_cresps[l1_idx].dst_port);
	}
	else {
	    NetworkPacket& pkt = mux_pkts[mux_idx];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLS_ID, pkt.src_port);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, pkt.dst_port);

	    Coh_msg cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	    CPPUNIT_ASSERT_EQUAL((int)LLP_cache :: LLP_ID, cresp.dst_port);
	}




	//verify state of the MESI_L2_cache
	//There should NOT be an entry in the MSHR
	CPPUNIT_ASSERT_EQUAL(false, m_cachep->mshr->has_match(ADDR));
	//There should be an entry in the hash table
	CPPUNIT_ASSERT_EQUAL(true, m_cachep->my_table->has_match(ADDR));
	//There should NOT be an entry in the stall buffer.
	CPPUNIT_ASSERT_EQUAL(0, (int)m_cachep->stalled_client_req_buffer.size());

	//verify state of the manager
	CPPUNIT_ASSERT_EQUAL(MESI_MNG_E, manager->state);
	CPPUNIT_ASSERT_EQUAL(SHARERS_ID[0], manager->owner);
	CPPUNIT_ASSERT_EQUAL(0, (int)manager->sharersList.count());
    }








    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MESI_LLS_cacheTest");
	/*
	*/
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_I_0", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_I_0", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_S_0));
	//mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_S_1", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_S_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_I_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_I_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_hit_l1_S_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_hit_l1_S_l2_S_0));
	//eviction of client line
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_client_eviction_victim_E_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_client_eviction_victim_E_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_client_eviction_victim_M_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_client_eviction_victim_M_l2_E_0));
	//eviction of LLS line
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_l2_eviction_victim_E_l1_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_l2_eviction_victim_E_l1_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_l2_eviction_victim_E_l1_M_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_l2_eviction_victim_E_l1_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_l2_eviction_victim_E_l1_S_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_l2_eviction_victim_E_l1_S_0));
	//MSHR_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_MSHR_STALL_0", &MESI_LLS_cacheTest::test_process_client_request_MSHR_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_MSHR_STALL_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_MSHR_STALL_0));
	//PREV_PEND_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_PREV_PEND_STALL_0", &MESI_LLS_cacheTest::test_process_client_request_PREV_PEND_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_PREV_PEND_STALL_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_PREV_PEND_STALL_0));
	//LRU_BUSY_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_LRU_BUSY_STALL_0", &MESI_LLS_cacheTest::test_process_client_request_LRU_BUSY_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_LRU_BUSY_STALL_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_LRU_BUSY_STALL_0));
	//TRANS_STALL, e.g., the hit entry is being evicted
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_TRANS_STALL_0", &MESI_LLS_cacheTest::test_process_client_request_TRANS_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_TRANS_STALL_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_TRANS_STALL_0));
	//Request race
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_request_race_l1_E_to_I_l2_FWD_E_0", &MESI_LLS_cacheTest::test_request_race_l1_E_to_I_l2_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_request_race_l1_E_to_I_l2_FWD_S_0", &MESI_LLS_cacheTest::test_request_race_l1_E_to_I_l2_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_request_race_l1_M_to_I_l2_FWD_E_0", &MESI_LLS_cacheTest::test_request_race_l1_M_to_I_l2_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_request_race_l1_M_to_I_l2_FWD_S_0", &MESI_LLS_cacheTest::test_request_race_l1_M_to_I_l2_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_request_race_l1_E_to_I_l2_DEMAND_I_0", &MESI_LLS_cacheTest::test_request_race_l1_E_to_I_l2_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_request_race_l1_M_to_I_l2_DEMAND_I_0", &MESI_LLS_cacheTest::test_request_race_l1_M_to_I_l2_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_request_race_l1_I_to_E_l2_DEMAND_I_0", &MESI_LLS_cacheTest::test_request_race_l1_I_to_E_l2_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_request_race_l1_I_to_E_in_IE_l2_DEMAND_I_0", &MESI_LLS_cacheTest::test_request_race_l1_I_to_E_in_IE_l2_DEMAND_I_0));
	/*
	*/



	return mySuite;
    }
};


Clock MESI_LLS_cacheTest :: MasterClock(MESI_LLS_cacheTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MESI_LLS_cacheTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


