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
#include "MESI_LLS_cache.h"
#include "mux_demux.h"
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
	    m_credits.push_back(*pkt);
	    delete pkt;
	}
	else {
	    assert(0);
	}
    }
*/

    vector<Coh_msg>& get_cache_resps() { return m_cache_resps; }
    vector<NetworkPacket>& get_pkts() { return m_pkts; }
    vector<NetworkPacket>& get_credits() { return m_credits; }
private:
    vector<Coh_msg> m_cache_resps;
    vector<NetworkPacket> m_pkts;
    vector<NetworkPacket> m_credits;
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
    void mySetUp(int credits)
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
	settings.downstream_credits = credits;

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
    //! @brief Test process_client_request(): load miss; also miss in L2; remote L1
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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

	Coh_msg* req = new Coh_msg;
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule the req
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

	Manifold::StopAt(When + L1_L2 + 2*HT_LOOKUP + 10);
	Manifold::Run();

        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size());

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss; also miss in L2; local L1
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
    void test_process_client_request_load_l1_I_l2_I_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg;
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();

        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //one msg goes to MC

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size());

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss in L1; manager in E;
    //! source is remote; owner is remote.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_S to the owner; then it clears owner and adds client
    //! to sharer list. Manager enters ES.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    void test_process_client_request_load_l1_I_l2_E_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	int OWNER_ID;
	do {
	    OWNER_ID = random() % 1024;
	} while(OWNER_ID == SOURCE_ID || OWNER_ID == NODE_ID);

	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending MC_FWD_S remotely

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //MC_FWD_S
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_S from remote

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss in L1; manager in E;
    //! source is remote; owner is local.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_S to the owner; then it clears owner and adds client
    //! to sharer list. Manager enters ES.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    void test_process_client_request_load_l1_I_l2_E_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	int OWNER_ID = NODE_ID;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits); //sending MC_FWD_S local

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(0, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_S from remote

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //MC_FWD_S
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss in L1; manager in E;
    //! source is local; owner is remote.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_S to the owner; then it clears owner and adds client
    //! to sharer list. Manager enters ES.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    void test_process_client_request_load_l1_I_l2_E_2()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

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
	int OWNER_ID;
	do {
	    OWNER_ID = random() % 1024;
	} while(OWNER_ID == SOURCE_ID || OWNER_ID == NODE_ID);

	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending MC_FWD_S remotely

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //MC_FWD_S
        CPPUNIT_ASSERT_EQUAL(0, (int)m_netp->get_credits().size()); //CM_I_to_S from local

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss in L1; manager in S.
    //! source is remote.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in S, it sends MESI_MC_GRANT_S_DATA to the client; it adds client
    //! to sharer list and enters SS.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->C, MESI_MC_GRANT_S_DATA
    void test_process_client_request_load_l1_I_l2_S_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();

        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size());

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): load miss in L1; manager in S.
    //! source is local.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in S, it sends MESI_MC_GRANT_S_DATA to the client; it adds client
    //! to sharer list and enters SS.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->C, MESI_MC_GRANT_S_DATA
    void test_process_client_request_load_l1_I_l2_S_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

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
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();

        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits); //msg to L1 is local

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(0, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_netp->get_credits().size()); //msg from L1 is local

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss; also miss in L2.
    //! source is remote.
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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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

	Manifold::StopAt(When + L1_L2 + 2*HT_LOOKUP + 10);
	Manifold::Run();

        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits); //msg to MC; and to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size()); //msg from source, and from MC

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss; also miss in L2.
    //! source is local.
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
    void test_process_client_request_store_l1_I_l2_I_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();

        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //msg to MC

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to MC
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //msg from MC

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to source
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss in L1; manager in E.
    //! source is remote; owner is remote
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_E to the owner; then it clears owner and sets the
    //! client as the new owner. Manager enters EE state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    void test_process_client_request_store_l1_I_l2_E_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	do {
	    OWNER_ID = random() % 1024;
        } while(OWNER_ID == NODE_ID || OWNER_ID == SOURCE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending MC_FWD_E remotely

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //MC_FWD_E
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_E from remote

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss in L1; manager in E.
    //! source is remote; owner is local
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_E to the owner; then it clears owner and sets the
    //! client as the new owner. Manager enters EE state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    void test_process_client_request_store_l1_I_l2_E_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	int OWNER_ID = NODE_ID;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits); //sending MC_FWD_E local

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(0, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_E from remote

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //MC_FWD_E
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss in L1; manager in E.
    //! source is local; owner is remote
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_E to the owner; then it clears owner and sets the
    //! client as the new owner. Manager enters EE state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    void test_process_client_request_store_l1_I_l2_E_2()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

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
	do {
	    OWNER_ID = random() % 1024;
        } while(OWNER_ID == NODE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending MC_FWD_E remotely

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //MC_FWD_E
        CPPUNIT_ASSERT_EQUAL(0, (int)m_netp->get_credits().size()); //CM_I_to_E from local

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss in L1; manager in S.
    //! source is remote; sharers remote;
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in S, it sends MESI_MC_DEMAND_I to all sharers; the client is set to be owner
    //! and manager enters SIE.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->all sharers, MESI_MC_DEMAND_I
    void test_process_client_request_store_l1_I_l2_S_0()
    {
	int CREDITS = random() % 100 + 10; //downstream credits; at least 10
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

//cout << "CREDITS= " << CREDITS << " SOURCE= " << SOURCE_ID << endl;
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
//cout << "num sharers= " << NUM_SHARERS << endl;
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS, m_cachep->m_downstream_credits); //sending DEMAND_I remotely

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)m_muxp->get_pkts().size()); //DEMAND_I
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_E from remote

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss in L1; manager in S.
    //! source is remote; one sharer is local
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in S, it sends MESI_MC_DEMAND_I to all sharers; the client is set to be owner
    //! and manager enters SIE.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->all sharers, MESI_MC_DEMAND_I
    void test_process_client_request_store_l1_I_l2_S_1()
    {
	int CREDITS = random() % 100 + 10; //downstream credits; at least 10
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

//cout << "CREDITS= " << CREDITS << " SOURCE= " << SOURCE_ID << endl;
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
//cout << "num sharers= " << NUM_SHARERS << endl;
	int SHARERS_ID[NUM_SHARERS];
	//set one sharer to local node
	SHARERS_ID[0] = NODE_ID;
	for(int i=1; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS + 1, m_cachep->m_downstream_credits); //sending 1 DEMAND_I locally; rest remotely

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS-1, (int)m_muxp->get_pkts().size()); //DEMAND_I
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_E from remote

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request(): store miss in L1; manager in S.
    //! source is local; sharers remote;
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in S, it sends MESI_MC_DEMAND_I to all sharers; the client is set to be owner
    //! and manager enters SIE.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->all sharers, MESI_MC_DEMAND_I
    void test_process_client_request_store_l1_I_l2_S_2()
    {
	int CREDITS = random() % 100 + 10; //downstream credits; at least 10
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

//cout << "CREDITS= " << CREDITS << " SOURCE= " << SOURCE_ID << endl;
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
//cout << "num sharers= " << NUM_SHARERS << endl;
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS, m_cachep->m_downstream_credits); //sending DEMAND_I remotely

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)m_muxp->get_pkts().size()); //DEMAND_I
        CPPUNIT_ASSERT_EQUAL(0, (int)m_netp->get_credits().size()); //CM_I_to_E from local

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in I.
    //! source is remote.
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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_E;
	req2->src_id = SOURCE_ID;

	NetworkPacket* pkt2 = new NetworkPacket;
	pkt2->type = L2_cache :: COH_MSG;
	pkt2->src = SOURCE_ID;
	pkt2->src_port = LLP_cache :: LLP_ID;
	pkt2->dst = NODE_ID;
	pkt2->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt2->data)) = *req;
	pkt2->data_size = sizeof(Coh_msg);
	delete req;
	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits); //sending to MC and to Source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2, (int)m_muxp->get_pkts().size()); //msg to MC and MC_GRANT_E
        CPPUNIT_ASSERT_EQUAL(3, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_UNBLOCK from remote and msg from MC

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in I.
    //! source is local.
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
    void test_process_client_request_and_reply_load_l1_I_l2_I_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S;
	req->src_id = SOURCE_ID;
	req->rw = 0;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
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

        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending to MC and to Source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to MC
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_UNBLOCK from remote and msg from MC

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //MC_GRANT_E
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in E; owner in E.
    //! source is remote; owner is remote.
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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	do {
	    OWNER_ID = random() % 1024;
	} while(OWNER_ID == SOURCE_ID || OWNER_ID == NODE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_CLEAN;
	req2->src_id = OWNER_ID;

	NetworkPacket* pkt2 = new NetworkPacket;
	pkt2->type = L2_cache :: COH_MSG;
	pkt2->src = SOURCE_ID;
	pkt2->src_port = LLP_cache :: LLP_ID;
	pkt2->dst = NODE_ID;
	pkt2->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt2->data)) = *req2;
	pkt2->data_size = sizeof(Coh_msg);
	delete req2;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_S;
	req3->src_id = SOURCE_ID;

	NetworkPacket* pkt3 = new NetworkPacket;
	pkt3->type = L2_cache :: COH_MSG;
	pkt3->src = SOURCE_ID;
	pkt3->src_port = LLP_cache :: LLP_ID;
	pkt3->dst = NODE_ID;
	pkt3->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt3->data)) = *req3;
	pkt3->data_size = sizeof(Coh_msg);
	delete req3;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt3);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(3, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_CLEAN, CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in E; owner in E.
    //! source is remote; owner is local.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_S to the owner; it adds client
    //! to sharer list and enters ES. Owner sends MESI_CC_S_DATA to client and MESI_CM_CLEAN
    //! to manager; client sends MESI_CM_UNBLOCK_S to manager; manager enters S state.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    //! 3. owner--->M, MESI_CM_CLEAN; and owner--->C, MESI_CC_S_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_S
    void test_process_client_request_and_reply_load_l1_I_l2_E_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	int OWNER_ID = NODE_ID;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_CLEAN;
	req2->src_id = OWNER_ID;

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_S;
	req3->src_id = SOURCE_ID;

	NetworkPacket* pkt3 = new NetworkPacket;
	pkt3->type = L2_cache :: COH_MSG;
	pkt3->src = SOURCE_ID;
	pkt3->src_port = LLP_cache :: LLP_ID;
	pkt3->dst = NODE_ID;
	pkt3->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt3->data)) = *req3;
	pkt3->data_size = sizeof(Coh_msg);
	delete req3;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt3);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits); //sending to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(0, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_CLEAN (owner), CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in E; owner in E.
    //! source is local; owner is remote.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_S to the owner; it adds client
    //! to sharer list and enters ES. Owner sends MESI_CC_S_DATA to client and MESI_CM_CLEAN
    //! to manager; client sends MESI_CM_UNBLOCK_S to manager; manager enters S state.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->owner, MESI_MC_FWD_S
    //! 3. owner--->M, MESI_CM_CLEAN; and owner--->C, MESI_CC_S_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_S
    void test_process_client_request_and_reply_load_l1_I_l2_E_2()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

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
	do {
	    OWNER_ID = random() % 1024;
	} while(OWNER_ID == SOURCE_ID || OWNER_ID == NODE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_CLEAN;
	req2->src_id = OWNER_ID;

	NetworkPacket* pkt2 = new NetworkPacket;
	pkt2->type = L2_cache :: COH_MSG;
	pkt2->src = SOURCE_ID;
	pkt2->src_port = LLP_cache :: LLP_ID;
	pkt2->dst = NODE_ID;
	pkt2->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt2->data)) = *req2;
	pkt2->data_size = sizeof(Coh_msg);
	delete req2;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_S;
	req3->src_id = SOURCE_ID;

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_CLEAN(owner), CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }






    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in E; owner in M.
    //! source is remote; owner is remote.
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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	do {
	    OWNER_ID = random() % 1024;
	} while(OWNER_ID == SOURCE_ID || OWNER_ID == NODE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_WRITEBACK;
	req2->src_id = OWNER_ID;

	NetworkPacket* pkt2 = new NetworkPacket;
	pkt2->type = L2_cache :: COH_MSG;
	pkt2->src = SOURCE_ID;
	pkt2->src_port = LLP_cache :: LLP_ID;
	pkt2->dst = NODE_ID;
	pkt2->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt2->data)) = *req2;
	pkt2->data_size = sizeof(Coh_msg);
	delete req2;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_S;
	req3->src_id = SOURCE_ID;

	NetworkPacket* pkt3 = new NetworkPacket;
	pkt3->type = L2_cache :: COH_MSG;
	pkt3->src = SOURCE_ID;
	pkt3->src_port = LLP_cache :: LLP_ID;
	pkt3->dst = NODE_ID;
	pkt3->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt3->data)) = *req3;
	pkt3->data_size = sizeof(Coh_msg);
	delete req3;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt3);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(3, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_WBACK, CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in E; owner in M.
    //! source is remote; owner is local.
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
    void test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	int OWNER_ID = NODE_ID;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
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
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_WRITEBACK;
	req2->src_id = OWNER_ID;

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_S;
	req3->src_id = SOURCE_ID;

	NetworkPacket* pkt3 = new NetworkPacket;
	pkt3->type = L2_cache :: COH_MSG;
	pkt3->src = SOURCE_ID;
	pkt3->src_port = LLP_cache :: LLP_ID;
	pkt3->dst = NODE_ID;
	pkt3->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt3->data)) = *req3;
	pkt3->data_size = sizeof(Coh_msg);
	delete req3;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt3);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits); //sending to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(0, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_WBACK (owner), CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in E; owner in M.
    //! source is local; owner is remote.
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
    void test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_2()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

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
	do {
	    OWNER_ID = random() % 1024;
	} while(OWNER_ID == SOURCE_ID || OWNER_ID == NODE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_WRITEBACK;
	req2->src_id = OWNER_ID;

	NetworkPacket* pkt2 = new NetworkPacket;
	pkt2->type = L2_cache :: COH_MSG;
	pkt2->src = SOURCE_ID;
	pkt2->src_port = LLP_cache :: LLP_ID;
	pkt2->dst = NODE_ID;
	pkt2->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt2->data)) = *req2;
	pkt2->data_size = sizeof(Coh_msg);
	delete req2;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req3 = new Coh_msg();
	req3->type = Coh_msg :: COH_RPLY;
	req3->addr = ADDR;
	req3->msg = MESI_CM_UNBLOCK_S;
	req3->src_id = SOURCE_ID;

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req3);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_WBACK(owner), CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in S.
    //! source is remote.
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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_S;
	req2->src_id = SOURCE_ID;

	NetworkPacket* pkt2 = new NetworkPacket;
	pkt2->type = L2_cache :: COH_MSG;
	pkt2->src = SOURCE_ID;
	pkt2->src_port = LLP_cache :: LLP_ID;
	pkt2->dst = NODE_ID;
	pkt2->dst_port = LLP_cache :: LLS_ID;
	*((Coh_msg*)(pkt2->data)) = *req2;
	pkt2->data_size = sizeof(Coh_msg);
	delete req2;

	Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending MC_GRANT_S to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to source
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): load
    //! miss in L1; manager in S.
    //! source is local.
    //!
    //! In a LOAD miss, client sends MESI_CM_I_to_S to manager. As the manager is
    //! in S, it sends MESI_MC_GRANT_S_DATA to the client; it adds client
    //! to sharer list and enters SS. Client replys with MESI_CM_UNBLOCK_S; manager
    //! enters S.
    //! 1. C--->M, MESI_CM_I_to_S
    //! 2. M--->C, MESI_MC_GRANT_S_DATA
    //! 3. C--->M, MESI_CM_UNBLOCK_S
    void test_process_client_request_and_reply_load_l1_I_l2_S_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

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
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_S;
	req2->src_id = SOURCE_ID;

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits); //sending MC_GRANT_S to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(0, (int)m_muxp->get_pkts().size()); //msg to source
        CPPUNIT_ASSERT_EQUAL(0, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to source
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }






    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store
    //! miss in L1; also miss in L2.
    //! source is remote.
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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);


	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	{
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
	{
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


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits); //sending to MC, and MC_GRANT_E to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2, (int)m_muxp->get_pkts().size()); //msg to MC and source
        CPPUNIT_ASSERT_EQUAL(3, (int)m_netp->get_credits().size()); //CM_I_to_E, CM_UNBLOCK, and reply from MC

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store
    //! miss in L1; also miss in L2.
    //! source is local.
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
    void test_process_client_request_and_reply_store_l1_I_l2_I_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;


	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_E;
	req->src_id = SOURCE_ID;
	req->rw = 1;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
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


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //sending to MC, and MC_GRANT_E to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to MC
        CPPUNIT_ASSERT_EQUAL(1, (int)m_netp->get_credits().size()); //reply from MC

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to source
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store miss in L1;
    //! manager in E.
    //! source is remote; owner is remote
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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);


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
	do {
	    OWNER_ID = random() % 1024;
	} while(OWNER_ID == SOURCE_ID || OWNER_ID == NODE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	{
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
	{
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



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //MC_FWD_E to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size()); //CM_I_to_E, CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }





    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store miss in L1;
    //! manager in E.
    //! source is remote; owner is local
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_E to the owner; then it clears owner and sets the
    //! client as the new owner. Manager enters EE state. Owner sends MESI_CC_E_DATA to
    //! client; client sends MESI_CM_UNBLOCK_E to manager; manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    //! 3. owner--->C, MESI_CC_E_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_E
    void test_process_client_request_and_reply_store_l1_I_l2_E_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);


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
	int OWNER_ID = NODE_ID;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	{
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
	{
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



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits); //MC_FWD_E to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(0, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size()); //CM_I_to_E, CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store miss in L1;
    //! manager in E.
    //! source is local; owner is remote
    //!
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in E, it sends MESI_MC_FWD_E to the owner; then it clears owner and sets the
    //! client as the new owner. Manager enters EE state. Owner sends MESI_CC_E_DATA to
    //! client; client sends MESI_CM_UNBLOCK_E to manager; manager enters E state.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->owner, MESI_MC_FWD_E
    //! 3. owner--->C, MESI_CC_E_DATA
    //! 4. C--->M, MESI_CM_UNBLOCK_E
    void test_process_client_request_and_reply_store_l1_I_l2_E_2()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;


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
	do {
	    OWNER_ID = random() % 1024;
	} while(OWNER_ID == SOURCE_ID || OWNER_ID == NODE_ID);
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
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



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //MC_FWD_E to owner

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to owner
        CPPUNIT_ASSERT_EQUAL(0, (int)m_netp->get_credits().size()); //CM_I_to_E, CM_UNBLOCK

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }






    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store miss in L1;
    //! manager in S.
    //! source is remote; sharers are remote.
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
        const int MAX_SHARERS = 10;

	int CREDITS = random() % 100 + MAX_SHARERS + 1; //downstream credits; at least MAX_SHARERS+1
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);


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
	const int NUM_SHARERS = random() % (MAX_SHARERS-1) + 2; // 2 to MAX_SHARERS
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	{
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
	    {
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
	{
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



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS - 1, m_cachep->m_downstream_credits); //msg to sharers; MC_GRANT_E to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS + 1, (int)m_muxp->get_pkts().size()); //msg to sharers, source
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS + 2, (int)m_netp->get_credits().size()); //CM_I_to_E, CM_UNBLOCK_I (sharers), CM_UNBLOCK_E

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store miss in L1;
    //! manager in S.
    //! source is remote; one sharer is local.
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
    void test_process_client_request_and_reply_store_l1_I_l2_S_1()
    {
        const int MAX_SHARERS = 10;

	int CREDITS = random() % 100 + MAX_SHARERS + 1; //downstream credits; at least MAX_SHARERS+1
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);


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
	const int NUM_SHARERS = random() % (MAX_SHARERS-1) + 2; // 2 to MAX_SHARERS
	int SHARERS_ID[NUM_SHARERS];
	SHARERS_ID[0] = NODE_ID;
	for(int i=1; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	{
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
	    if(SHARERS_ID[i] != NODE_ID) {
		NetworkPacket* pkt = new NetworkPacket;
		pkt->type = L2_cache :: COH_MSG;
		pkt->src = SOURCE_ID;
		pkt->src_port = LLP_cache :: LLP_ID;
		pkt->dst = NODE_ID;
		pkt->dst_port = LLP_cache :: LLS_ID;
		*((Coh_msg*)(pkt->data)) = *req2;
		pkt->data_size = sizeof(Coh_msg);

		Manifold::Schedule(When, &MockMux::send_pkt, m_muxp, pkt);
	    }
	    delete req2;
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_E;
	req2->src_id = SOURCE_ID;
	{
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



        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS, m_cachep->m_downstream_credits); //msg to sharers; MC_GRANT_E to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)m_muxp->get_pkts().size()); //msg to sharers, source
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS + 1, (int)m_netp->get_credits().size()); //CM_I_to_E, CM_UNBLOCK_I (sharers), CM_UNBLOCK_E

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store miss in L1;
    //! manager in S.
    //! source is local; sharers are remote.
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
    void test_process_client_request_and_reply_store_l1_I_l2_S_2()
    {
        const int MAX_SHARERS = 10;

	int CREDITS = random() % 100 + MAX_SHARERS + 1; //downstream credits; at least MAX_SHARERS+1
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;


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
	const int NUM_SHARERS = random() % (MAX_SHARERS-1) + 2; // 2 to MAX_SHARERS
	int SHARERS_ID[NUM_SHARERS];
	for(int i=0; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=0; i<NUM_SHARERS; i++) {
	    Coh_msg* req2 = new Coh_msg();
	    req2->type = Coh_msg :: COH_RPLY;
	    req2->addr = ADDR;
	    req2->msg = MESI_CM_UNBLOCK_I;
	    req2->src_id = SHARERS_ID[i];
	    {
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

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS, m_cachep->m_downstream_credits); //msg to sharers

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)m_muxp->get_pkts().size()); //msg to sharers
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)m_netp->get_credits().size()); //CM_UNBLOCK_I (sharers)

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to source
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store hit in L1;
    //! client in S; manager in S.
    //! source is remote and it's also a sharer; other sharers are remote.
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
        const int MAX_SHARERS = 10;

	int CREDITS = random() % 100 + MAX_SHARERS + 1; //downstream credits; at least MAX_SHARERS+1
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);


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
	const int NUM_SHARERS = random() % (MAX_SHARERS-1) + 2; // 2 to MAX_SHARERS
	int SHARERS_ID[NUM_SHARERS];
	SHARERS_ID[0] = SOURCE_ID; //requestor is a sharer.
	//add requestor to sharersList.
	manager->sharersList.set(SOURCE_ID);

	for(int i=1; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	{
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
	    {
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
	{
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




        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS, m_cachep->m_downstream_credits); //msg to sharers; MC_GRANT to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)m_muxp->get_pkts().size()); //msg to sharers; MC_GRANT to source
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS + 1, (int)m_netp->get_credits().size()); //CM_I_to_E, CM_UNBLOCK_I (sharers), CM_UNBLOCK_E

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }



    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store hit in L1;
    //! client in S; manager in S.
    //! source is remote and it's also a sharer; one sharer is local; rest of sharers are remote.
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
    void test_process_client_request_and_reply_store_hit_l1_S_l2_S_1()
    {
        const int MAX_SHARERS = 10;

	int CREDITS = random() % 100 + MAX_SHARERS + 1; //downstream credits; at least MAX_SHARERS+1
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);


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
	const int NUM_SHARERS = random() % (MAX_SHARERS-1) + 2; // 2 to MAX_SHARERS
	int SHARERS_ID[NUM_SHARERS];
	SHARERS_ID[0] = SOURCE_ID; //requestor is a sharer.
	//add requestor to sharersList.
	manager->sharersList.set(SOURCE_ID);

	SHARERS_ID[1] = NODE_ID;
	manager->sharersList.set(NODE_ID);

	for(int i=2; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	{
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
		Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
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
	{
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




        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS + 1, m_cachep->m_downstream_credits); //msg to sharers; MC_GRANT to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS - 1, (int)m_muxp->get_pkts().size()); //msg to sharers; MC_GRANT to source
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)m_netp->get_credits().size()); //CM_I_to_E, CM_UNBLOCK_I (sharers), CM_UNBLOCK_E

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to local sharer
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): store hit in L1;
    //! client in S; manager in S.
    //! source is local and it's also a sharer; other sharers are remote.
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
    void test_process_client_request_and_reply_store_hit_l1_S_l2_S_2()
    {
        const int MAX_SHARERS = 10;

	int CREDITS = random() % 100 + MAX_SHARERS + 1; //downstream credits; at least MAX_SHARERS+1
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;


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
	const int NUM_SHARERS = random() % (MAX_SHARERS-1) + 2; // 2 to MAX_SHARERS
	int SHARERS_ID[NUM_SHARERS];
	SHARERS_ID[0] = SOURCE_ID; //requestor is a sharer.
	//add requestor to sharersList.
	manager->sharersList.set(SOURCE_ID);

	for(int i=1; i<NUM_SHARERS; i++) {
	    bool ok=false;
	    while(!ok) {
		do {
		    SHARERS_ID[i] = random() % 1024;
		} while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
		ok = true;
		for(int j=0; j<i; j++) {
		    if(SHARERS_ID[i] == SHARERS_ID[j]) {
		        ok = false;
			break;
		    }
		}
	    }
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=1; i<NUM_SHARERS; i++) { //the first is requestor; it should not reply.
	    Coh_msg* req2 = new Coh_msg();
	    req2->type = Coh_msg :: COH_RPLY;
	    req2->addr = ADDR;
	    req2->msg = MESI_CM_UNBLOCK_I;
	    req2->src_id = SHARERS_ID[i];
	    {
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

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_SHARERS + 1, m_cachep->m_downstream_credits); //msg to sharers

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS - 1, (int)m_muxp->get_pkts().size()); //msg to sharers
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS - 1, (int)m_netp->get_credits().size()); //CM_UNBLOCK_I (sharers) 

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //MC_GRANT to source
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! victim in E; manager in E
    //! source is remote;
    //!
    //! 1. C--->M, MESI_CM_E_to_I
    //! 2. M--->C, MESI_MC_GRANT_I, manager enters EI_PUT
    //! 4. C--->M, MESI_CM_UNBLOCK_I
    void test_process_client_request_and_reply_client_eviction_victim_E_l2_E_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	{
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
	{
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


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //MC_GRANT_I to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to source
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size()); //CM_E_to_I, CM_UNBLOCK_I

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! victim in E; manager in E
    //! source is local;
    //!
    //! 1. C--->M, MESI_CM_E_to_I
    //! 2. M--->C, MESI_MC_GRANT_I, manager enters EI_PUT
    //! 4. C--->M, MESI_CM_UNBLOCK_I
    void test_process_client_request_and_reply_client_eviction_victim_E_l2_E_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

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

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_I;
	req2->src_id = SOURCE_ID;

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits); //MC_GRANT_I to source

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(0, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_netp->get_credits().size()); //CM_E_to_I, CM_UNBLOCK_I

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //MC_GRANT_I
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! victim in M; manager in E
    //! source is remote;
    //!
    //! 1. C--->M, MESI_CM_M_to_I
    //! 2. M--->C, MESI_MC_GRANT_I, manager enters EI_PUT
    //! 4. C--->M, MESI_CM_UNBLOCK_I
    void test_process_client_request_and_reply_client_eviction_victim_M_l2_E_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID;
	while((SOURCE_ID = random() % 1024) == NODE_ID);

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
	{
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
	{
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


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits); //MC_GRANT_I to source; dirty_to_mem

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2, (int)m_muxp->get_pkts().size()); //msg to source, and MC
        CPPUNIT_ASSERT_EQUAL(2, (int)m_netp->get_credits().size()); //CM_M_to_I, CM_UNBLOCK_I

        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
    }




    //======================================================================
    //======================================================================
    //! @brief Test process_client_request() and process_client_reply(): 
    //! victim in M; manager in E
    //! source is local;
    //!
    //! 1. C--->M, MESI_CM_M_to_I
    //! 2. M--->C, MESI_MC_GRANT_I, manager enters EI_PUT
    //! 4. C--->M, MESI_CM_UNBLOCK_I
    void test_process_client_request_and_reply_client_eviction_victim_M_l2_E_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	const paddr_t ADDR = random();
	int SOURCE_ID = NODE_ID;

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

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg* req2 = new Coh_msg();
	req2->type = Coh_msg :: COH_RPLY;
	req2->addr = ADDR;
	req2->msg = MESI_CM_UNBLOCK_I;
	req2->src_id = SOURCE_ID;

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, req2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify credits
	CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //MC_GRANT_I to source; dirty_to_mem

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1, (int)m_muxp->get_pkts().size()); //msg to MC
        CPPUNIT_ASSERT_EQUAL(0, (int)m_netp->get_credits().size()); //CM_M_to_I, CM_UNBLOCK_I

        CPPUNIT_ASSERT_EQUAL(1, (int)m_l1p->get_cache_resps().size()); //msg to source
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());
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
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

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



        //verify credits
	//3 msgs sent: MC_DEMAND_I to owner, mem req to MC, and GRANT_E to source
	int n_local = 0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;

	CPPUNIT_ASSERT_EQUAL(CREDITS - (3-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_muxp->get_pkts().size()); // MC_DEMAND_I, msg to MC
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size()); //msg to owner

	n_local = 0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_UNBLOCK_I, mem response
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());



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
	int CREDITS = random() % 100 + 4; //downstream credits; at least 4
	mySetUp(CREDITS);

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
	When += L1_L2 + 2*HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify credits
	//4 msgs sent: MC_DEMAND_I to owner, 2 mem req to MC: write back of evicted L2 line and read for new LOAD req;  and GRANT_E_DATA to source
	int n_local = 0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;

	CPPUNIT_ASSERT_EQUAL(CREDITS - (4-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(4-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	n_local = 0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_netp->get_credits().size()); //CM_I_to_S, CM_UNBLOCK_I, mem response
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


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
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	if(CREDITS < NUM_SHARERS+2)
	    CREDITS = m_cachep->m_downstream_credits = NUM_SHARERS + 2;

	mySetUp(CREDITS);

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
	//const int NUM_SHARERS = random() % 9 + 2; // 2 to 10
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



        //verify credits
	//NUM_SHARERS+2 msgs sent: NUM_SHARERS MC_DEMAND_I to sharers, 1 mem req to MC,  and GRANT_E_DATA to source

	int n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
	for(int i=0; i<NUM_SHARERS; i++) {
	    if(SOURCE_ID == SHARERS_ID[i]) {
	        n_local++;
		break;
	    }
	}

	CPPUNIT_ASSERT_EQUAL(CREDITS - (NUM_SHARERS+2-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv NUM_TAGS+2 msgs: CM_I_to_S, NUM_TAGS CM_UNBLOCK_I, mem response
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
	for(int i=0; i<NUM_SHARERS; i++) {
	    if(SOURCE_ID == SHARERS_ID[i]) {
	        n_local++;
		break;
	    }
	}
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->mshr->get_tag_mask();
	const int NUM_TAGS = m_cachep->mshr->assoc;
	assert(MSHR_SIZE == m_cachep->mshr->assoc);

        if(CREDITS < 2*NUM_TAGS+1)
	    CREDITS = m_cachep->m_downstream_credits = 2*NUM_TAGS+1;


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

	int IDs[NUM_TAGS];
	for(int i=0; i<NUM_TAGS; i++) {
	    bool ok=false;
	    while(!ok) {
		while((IDs[i] = random() % 1024) == NODE_ID);
		ok = true;
		for(int j=0; j<i; j++) //make sure ID's are unique.
		    if(IDs[j] == IDs[i]) {
		        ok = false;
		        break;
		    }
	    }
	}

	if(random() % 2 == 0)
	    IDs[0] = NODE_ID;


	for(int i=0; i<NUM_TAGS; i++) { //send N requests to fill up MSHR
	    Coh_msg* req = new Coh_msg();
	    req->type = Coh_msg :: COH_REQ;
	    req->addr = addrs[i];
	    req->msg = MESI_CM_I_to_S;
	    req->src_id = IDs[i];

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


        //verify credits
	//2*NUM_TAGS msgs sent: NUM_TAGS mem req to MC, and NUM_TAGS MC_GRANT_E_DATA.

	int n_local = 0;
	if(IDs[0] == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (2*NUM_TAGS-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2*NUM_TAGS-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv 2*NUM_TAGS + 1 msgs: NUM_TAGS+1 CM_I_to_S, NUM_TAGS mem response
	n_local = 0;
	if(IDs[0] == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(2*NUM_TAGS+1-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->mshr->get_tag_mask();
	const int NUM_TAGS = m_cachep->mshr->assoc;
	assert(MSHR_SIZE == m_cachep->mshr->assoc);

        if(CREDITS < 2*NUM_TAGS+2)
	    CREDITS = m_cachep->m_downstream_credits = 2*NUM_TAGS+2;

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

	int IDs[NUM_TAGS];
	for(int i=0; i<NUM_TAGS; i++) {
	    bool ok=false;
	    while(!ok) {
		while((IDs[i] = random() % 1024) == NODE_ID);
		ok = true;
		for(int j=0; j<i; j++) //make sure ID's are unique.
		    if(IDs[j] == IDs[i]) {
		        ok = false;
		        break;
		    }
	    }
	}

	if(random() % 2 == 0)
	    IDs[0] = NODE_ID;

	for(int i=0; i<NUM_TAGS; i++) { //send N requests to fill up MSHR
	    Coh_msg* req = new Coh_msg();
	    req->type = Coh_msg :: COH_REQ;
	    req->addr = addrs[i];
	    req->msg = MESI_CM_I_to_S;
	    req->src_id = IDs[i];

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



        //verify credits
	//2*NUM_TAGS+2 msgs sent: NUM_TAGS+1 mem req to MC, and NUM_TAGS+1 MC_GRANT_E_DATA.

	int n_local = 0;
	if(IDs[0] == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (2*NUM_TAGS+2-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2*NUM_TAGS+2-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv 2*NUM_TAGS + 2 msgs: NUM_TAGS+1 CM_I_to_S, NUM_TAGS+1 mem response
	n_local = 0;
	if(IDs[0] == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(2*NUM_TAGS+2-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

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


        //verify credits
	//2 msgs sent:  mem req to MC, and MC_GRANT_E_DATA.
	int n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (2-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv 3 msgs: 2 CM_I_to_S, 1 mem response
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
	if(SOURCE_ID2 == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

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
	int CREDITS = random() % 100 + 4; //downstream credits; at least 4
	mySetUp(CREDITS);

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


        //verify credits
	//4 msgs sent:  1 mem req to MC, 1 MC_GRANT_E_DATA, 1 MC_FWD_S
	int n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (3-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv 4 msgs: 2 CM_I_to_S, 1 UNBLOCK_E, 1 mem response
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	if(SOURCE_ID2 == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(4-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

	if(CREDITS < 2*NUM_TAGS) //one mem req, one GRANT_E_DATA
	    CREDITS = m_cachep->m_downstream_credits = NUM_TAGS;

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

	int IDs[NUM_TAGS];
	for(int i=0; i<NUM_TAGS; i++) {
	    bool ok=false;
	    while(!ok) {
		while((IDs[i] = random() % 1024) == NODE_ID);
		ok = true;
		for(int j=0; j<i; j++) //make sure ID's are unique.
		    if(IDs[j] == IDs[i]) {
		        ok = false;
		        break;
		    }
	    }
	}


	//schedule NUM_TAGS requests to make the set full
	for(int i=0; i<NUM_TAGS; i++) {
	    Coh_msg* req = new Coh_msg();
	    req->type = Coh_msg :: COH_REQ;
	    req->addr = addrs[i];
	    req->msg = MESI_CM_I_to_S; //a load miss
	    req->rw = 0;
	    req->src_id = IDs[i];
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


        //verify credits
	//2*NUM_TAGS msgs sent:  NUM_TAGS mem req to MC, NUM_TAGS MC_GRANT_E_DATA
	CPPUNIT_ASSERT_EQUAL(CREDITS - (2*NUM_TAGS), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2*NUM_TAGS, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());

	//recv 2*NUM_TAGS+1 msgs: NUM_TAGS+1 CM_I_to_S, NUM_TAGS mem response
	//however, the 1st NUM_TAGS CM_I_to_S msgs are sent over the local L1 port, so they
	//don't affect credits.
	int n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

	if(CREDITS < 2*NUM_TAGS+2) //one mem req, one GRANT_E_DATA
	    CREDITS = m_cachep->m_downstream_credits = NUM_TAGS+2;

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


        //verify credits
	//2*NUM_TAGS+2 msgs sent:  NUM_TAGS mem req to MC, NUM_TAGS+1 MC_GRANT_E_DATA
	CPPUNIT_ASSERT_EQUAL(CREDITS - (2*NUM_TAGS+1), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(2*NUM_TAGS+1, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_cache_resps().size());

	//recv 2*NUM_TAGS+1 msgs: NUM_TAGS+1 CM_I_to_S, NUM_TAGS mem response
	//however, the 1st NUM_TAGS CM_I_to_S msgs are sent over the local L1 port, so they
	//don't affect credits.
	int n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(NUM_TAGS+1-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

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

	int VICTIM_OWNER = random() % 1024;

	//put the state of all the lines in the set to E
	for(int i=0; i<NUM_TAGS; i++) {
	    hash_entry* entry = m_cachep->my_table->get_entry(addrs[i]);
	    assert(entry);
	    MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	    manager->state = MESI_MNG_E;
	    if(i==0)
	    manager->owner = VICTIM_OWNER;
	}



	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a request which will miss and cause an eviction
	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->rw = 0;

	bool req1_is_local = false;
	if(random() % 2 == 0) {
	    req1_is_local = true;
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

	bool req2_is_local = false;
	if(random() % 2 == 0) {
	    req2_is_local = true;
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




        //verify credits
	//1 msgs sent:  MC_E_to_I to victim owner
	int n_local=0;
	if(VICTIM_OWNER == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (1-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv 2 msgs: CM_I_to_S
	n_local = 0;
	if(req1_is_local)
	    n_local++;
	if(req2_is_local)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(2-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

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
	int CREDITS = random() % 100 + 4; //downstream credits; at least 4
	mySetUp(CREDITS);

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

        int VICTIM_OWNER = random() % 1024;
	if(random() % 2 == 0)
	    VICTIM_OWNER = NODE_ID;

        int VICTIM_OWNER2 = random() % 1024;
	if(random() % 2 == 0)
	    VICTIM_OWNER2 = NODE_ID;
	//put the state of all the lines in the set to E
	for(int i=0; i<NUM_TAGS; i++) {
	    hash_entry* entry = m_cachep->my_table->get_entry(addrs[i]);
	    assert(entry);
	    MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[entry->get_idx()]);
	    manager->state = MESI_MNG_E;
	    if(i==0)
	    manager->owner = VICTIM_OWNER;
	    if(i==1)
	    manager->owner = VICTIM_OWNER2;
	}



	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a request which will miss and cause an eviction
	Coh_msg* req = new Coh_msg();
	req->type = Coh_msg :: COH_REQ;
	req->addr = ADDR;
	req->msg = MESI_CM_I_to_S; //a load miss
	req->rw = 0;

	bool req1_is_local = false;
	if(random() % 2 == 0) {
	    req1_is_local = true;
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

	bool req2_is_local = false;
	if(random() % 2 == 0) {
	    req2_is_local = true;
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

	if(VICTIM_OWNER == NODE_ID) {
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

	if(req1_is_local) {
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



        //verify credits
	//4 msgs sent:  MC_E_to_I to victim owner; mem req and GRANT_E for 1st req; E_to_I (eviction) to victim2 for 2nd req
	int n_local=0;
	if(VICTIM_OWNER == NODE_ID)
	    n_local++;
	if(VICTIM_OWNER2 == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (4-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(4-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: 2*CM_I_to_S, UNBLOCK_I for victim, mem reply for 1st req, UNBLOCK_E for 1st req
	n_local = 0;
	if(req1_is_local)
	    n_local += 2;
	if(req2_is_local)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(5-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

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



        //verify credits
	//msgs sent:  MC_FWD_E to owner
	int n_local=0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (1-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: CM_I_to_E, CM_UNBLOCK_E from SOURCE_ID, E_to_I from OWNER_ID
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	if(OWNER_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

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



        //verify credits
	//msgs sent:  MC_FWD_S to owner
	int n_local=0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (1-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: CM_I_to_E, CM_UNBLOCK_E from SOURCE_ID, E_to_I from OWNER_ID
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	if(OWNER_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

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



        //verify credits
	//msgs sent:  MC_FWD_E to owner
	int n_local=0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (1-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: CM_I_to_E, CM_UNBLOCK_E from SOURCE_ID, E_to_I from OWNER_ID
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	if(OWNER_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());



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
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

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


        //verify credits
	//msgs sent:  MC_FWD_S to owner
	int n_local=0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (1-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(1-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: CM_I_to_E, CM_UNBLOCK_E from SOURCE_ID, E_to_I from OWNER_ID
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	if(OWNER_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


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
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

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



        //verify credits
	//msgs sent:  MC_DEMAND_I to owner, mem req, MC_GRANT_E_DATA to source
	int n_local=0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (3-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: CM_I_to_S, CM_UNBLOCK_E from SOURCE_ID, mem reply, UNBLOCK_I from owner
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	if(OWNER_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(4-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());

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
	int CREDITS = random() % 100 + 4; //downstream credits; at least 4
	mySetUp(CREDITS);

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



        //verify credits
	//msgs sent:  MC_DEMAND_I to owner, writeback to mem, mem req and MC_GRANT_E_DATA to source
	int n_local=0;
	if(OWNER_ID == NODE_ID)
	    n_local++;
	if(SOURCE_ID == NODE_ID)
	    n_local++;
	CPPUNIT_ASSERT_EQUAL(CREDITS - (4-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(4-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: CM_I_to_S,  mem reply, UNBLOCK_I from owner
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 1;
	if(OWNER_ID == NODE_ID)
	    n_local++;
        CPPUNIT_ASSERT_EQUAL(3-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


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
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10

	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	if(CREDITS < NUM_SHARERS + 2)
	    CREDITS = NUM_SHARERS + 2;
	mySetUp(CREDITS);

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



        //verify credits
	//msgs sent:  NUM_SHARERS MC_DEMAND_I to sharers,  MC_GRANT_E_DATA and FWD_E to source
	int n_local=0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	for(int i=0; i<NUM_SHARERS; i++) {
	    if(SHARERS_ID[i] == NODE_ID)
		n_local += 1;
	}
	CPPUNIT_ASSERT_EQUAL(CREDITS - (NUM_SHARERS+2-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: CM_I_to_E from source, CM_I_to_E from sharer, NUM_SHAERS UNBLOCK_I from sharers, UNBLOCK_E from source, UNBLOCK_E from sharer
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	for(int i=0; i<NUM_SHARERS; i++) {
	    if(SHARERS_ID[i] == NODE_ID)
		n_local += 1;
	}
	if(SHARERS_ID[0] == NODE_ID)
	    n_local += 2;
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+4-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());


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
	const int NUM_SHARERS = random() % 9 + 2; // 2 to 10

	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	if(CREDITS < NUM_SHARERS + 2)
	    CREDITS = NUM_SHARERS + 2;
	mySetUp(CREDITS);

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




        //verify credits
	//msgs sent:  NUM_SHARERS MC_DEMAND_I to sharers,  MC_GRANT_E_DATA and FWD_E to source
	int n_local=0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	for(int i=0; i<NUM_SHARERS; i++) {
	    if(SHARERS_ID[i] == NODE_ID)
		n_local += 1;
	}
	CPPUNIT_ASSERT_EQUAL(CREDITS - (NUM_SHARERS+2-n_local), m_cachep->m_downstream_credits);

        //verify msgs
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2-n_local, (int)m_muxp->get_pkts().size());
        CPPUNIT_ASSERT_EQUAL(n_local, (int)m_l1p->get_cache_resps().size());

	//recv msgs: CM_I_to_E from source, CM_I_to_E from sharer, NUM_SHAERS UNBLOCK_I from sharers, UNBLOCK_E from source, UNBLOCK_E from sharer
	n_local = 0;
	if(SOURCE_ID == NODE_ID)
	    n_local += 2;
	for(int i=0; i<NUM_SHARERS; i++) {
	    if(SHARERS_ID[i] == NODE_ID)
		n_local += 1;
	}
	if(SHARERS_ID[0] == NODE_ID)
	    n_local += 2;
        CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+4-n_local, (int)m_netp->get_credits().size());
        CPPUNIT_ASSERT_EQUAL(0, (int)m_l1p->get_credits().size());




    }








    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MESI_LLS_cacheTest");
	/*
	*/
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_I_0", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_I_1", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_I_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_E_1", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_E_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_E_2", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_E_2));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_load_l1_I_l2_S_1", &MESI_LLS_cacheTest::test_process_client_request_load_l1_I_l2_S_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_I_0", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_I_1", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_I_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_E_1", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_E_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_E_2", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_E_2));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_S_1", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_S_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_store_l1_I_l2_S_2", &MESI_LLS_cacheTest::test_process_client_request_store_l1_I_l2_S_2));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_I_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_I_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_I_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_2", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_2));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_2", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_2));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_S_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_S_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_I_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_I_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_I_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_E_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_E_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_E_2", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_E_2));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_S_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_S_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_S_2", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_S_2));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_hit_l1_S_l2_S_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_hit_l1_S_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_hit_l1_S_l2_S_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_hit_l1_S_l2_S_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_store_hit_l1_S_l2_S_2", &MESI_LLS_cacheTest::test_process_client_request_and_reply_store_hit_l1_S_l2_S_2));
	//eviction of client line
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_client_eviction_victim_E_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_client_eviction_victim_E_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_client_eviction_victim_E_l2_E_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_client_eviction_victim_E_l2_E_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_client_eviction_victim_M_l2_E_0", &MESI_LLS_cacheTest::test_process_client_request_and_reply_client_eviction_victim_M_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_LLS_cacheTest>("test_process_client_request_and_reply_client_eviction_victim_M_l2_E_1", &MESI_LLS_cacheTest::test_process_client_request_and_reply_client_eviction_victim_M_l2_E_1));
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


