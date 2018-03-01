//! This program tests MESI_L1_cache. We use two Mock
//! components and connect the MESI_L1_cache to the components.
//!
//!                    ----------
//!                   | MockProc |
//!                    ----------
//!                         |
//!                  ---------------
//!                 | MESI_L1_cache |
//!                  ---------------
//!                         |
//!                 -----------------
//!                | MockL2 plays L2 |
//!                 -----------------

#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <stdlib.h>
#include <iostream>
#include <vector>
#include "MESI_L1_cache.h"
#include "coherence/MESI_client.h"
#include "coh_mem_req.h"

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
struct Proc_req {
    enum {LOAD, STORE};
    Proc_req(int rid, int nid, unsigned long ad, int t) :
        addr(ad), type(t)
	{}
    unsigned long get_addr() { return addr; }
    bool is_read() { return type == LOAD; }

    unsigned long addr;
    int type;
};


//! This class emulates a processor model.
class MockProc : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};
    void send_creq(Proc_req* req)
    {
	Send(OUT, req);
    }
    void handle_incoming(int, Proc_req* cresp)
    {
    //cout << "MockProc handle_incoming @ " << Manifold::NowTicks() << endl;
        m_cache_resps.push_back(*cresp);
        m_ticks.push_back(Manifold :: NowTicks());
    }
    vector<Proc_req>& get_cache_resps() { return m_cache_resps; }
    vector<Ticks_t>& get_ticks() { return m_ticks; }
private:
    vector<Proc_req> m_cache_resps;
    vector<Ticks_t> m_ticks;
};


//! This class emulates a component that's downstream from the cache.
class MockLower : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};

    void handle_incoming(int, NetworkPacket* pkt)
    {
        if(pkt->type == L1_cache :: CREDIT_MSG)
	    m_credit_pkts.push_back(*pkt);
	else
	    m_pkts.push_back(*pkt);

	delete pkt;
    }

    //with this function, the mock object can act as a manager
    void send_pkt(NetworkPacket* pkt)
    {
	Send(OUT, pkt);
    }


    vector<NetworkPacket>& get_pkts() { return m_pkts; }
    vector<NetworkPacket>& get_credit_pkts() { return m_credit_pkts; }
    //vector<Ticks_t>& get_ticks() { return m_ticks; }

private:
    //Save the requests and timestamps for verification.
    vector<NetworkPacket> m_pkts;
    vector<NetworkPacket> m_credit_pkts;
    //vector<Ticks_t> m_ticks;
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

const int NODE_ID = 11; //cache's node ID
const int L2_ID = 13; //L2's node ID


//####################################################################
//! Class MESI_L1_cacheTest is the test class for class MESI_L1_cache. 
//####################################################################
class MESI_L1_cacheTest : public CppUnit::TestFixture {
private:
    static const int HT_SIZE = 0x1 << 14; //2^14 = 16k;
    static const int HT_ASSOC = 4;
    static const int HT_BLOCK_SIZE = 32;
    static const int HT_HIT = 2;
    static const int HT_LOOKUP = 11;

    static const unsigned MSHR_SIZE = 8;

    //latencies
    static const Ticks_t PROC_CACHE = 1;
    static const Ticks_t CACHE_TO_LOWER = 1;
    static const Ticks_t LOWER_TO_CACHE = 3;


    MESI_L1_cache* m_cachep;
    MockProc* m_procp;
    MockLower* m_lowerp;
    MyMcMap1* m_l2map;

public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    void mySetUp(int downstream_credits)
    {
	cache_settings cache_parameters;

	cache_parameters.name = "testCache";
	//cache_parameters.type = CACHE_DATA;
	cache_parameters.size = HT_SIZE;
	cache_parameters.assoc = HT_ASSOC;
	cache_parameters.block_size = HT_BLOCK_SIZE;
	cache_parameters.hit_time = HT_HIT;
	cache_parameters.lookup_time = HT_LOOKUP;
	cache_parameters.replacement_policy = RP_LRU;


        //create a MockProc, a cache, and a MockLower
	CompId_t procId = Component :: Create<MockProc>(0);
	m_procp = Component::GetComponent<MockProc>(procId);

	m_l2map = new MyMcMap1(L2_ID); //all addresses map to L2

	L1_cache_settings settings;
	settings.l2_map = m_l2map;
	settings.mshr_sz = MSHR_SIZE;
	settings.downstream_credits = downstream_credits;

        L1_cache :: COH_MSG = random() % 1024;
        while((L1_cache :: CREDIT_MSG = random() % 1024) == L1_cache :: COH_MSG);

	CompId_t cacheId = Component :: Create<MESI_L1_cache>(0, NODE_ID, cache_parameters, settings);
	CompId_t lowerId = Component :: Create<MockLower>(0);
	m_cachep = Component::GetComponent<MESI_L1_cache>(cacheId);
	m_lowerp = Component::GetComponent<MockLower>(lowerId);

        //connect the components
	//proc to cache
	Manifold::Connect(procId, MockProc::OUT, &MockProc::handle_incoming,
	                  cacheId, L1_cache::PORT_PROC, &L1_cache::handle_processor_request<Proc_req>,
			  MasterClock, MasterClock, PROC_CACHE, PROC_CACHE);
	//cache to downstream
	Manifold::Connect(cacheId, L1_cache::PORT_L2, &L1_cache::handle_peer_and_manager_request,
	                  lowerId, MockLower::IN, &MockLower::handle_incoming,
			  MasterClock, MasterClock, CACHE_TO_LOWER, LOWER_TO_CACHE);

        Clock::Register(MasterClock, (L1_cache*)m_cachep, &L1_cache::tick, (void(L1_cache::*)(void))0);
    }






    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): load miss.
    //!
    //! Empty cache; load with a random address; since this is a miss, client
    //! sends MESI_CM_I_TO_S. Downstream_credits is decremented.
    void test_handle_processor_request_load_l1_I_0()
    {
	Manifold::unhalt();

	int CREDITS = random() % 100 + 1; //downstream credits
	mySetUp(CREDITS);

        CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* preq = new Proc_req(0, NODE_ID, ADDR, Proc_req::LOAD);

	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, preq);

	Manifold::StopAt(When + PROC_CACHE + HT_LOOKUP + 10);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits);

    }



    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): load hit
    //!
    //! Empty cache; manually put a tag in the hash table and set the state of
    //! the corresponding client to E, S, or M. Send a request with the same
    //! address; it should hit, and a response is sent back.
    //! Downstream_credits remains unchanged.
    void test_handle_processor_request_load_l1_ESM_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits
	mySetUp(CREDITS);

        CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* preq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	//manually put the ADDR in the hash table, and set the client state to
	//anything but I.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);

	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	MESI_client_state_t valid_states[3] = {MESI_C_E, MESI_C_S, MESI_C_M};
	client->state = valid_states[random() % 3];
	MESI_client_state_t old_state = client->state;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, preq);

	Manifold::StopAt(When + PROC_CACHE+ 10);
	Manifold::Run();

        //verify the downstream credits is unchanged.
        CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits);

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): store miss.
    //!
    //! Empty cache; store with a random address; since this is a miss, client
    //! sends a MESI_CM_I_TO_E message.
    //! Downstream credits is decremented.
    void test_handle_processor_request_store_l1_I_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	Proc_req* preq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, preq);

	Manifold::StopAt(When + PROC_CACHE + HT_LOOKUP + 10);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits);

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): store hit in M state
    //!
    //! Empty cache; manually put a tag in the hash table and set the state of
    //! the corresponding client to M. Send a request with the same
    //! address; it should hit, and a response is sent back.
    //! Downstream_credits remains unchanged.
    void test_handle_processor_request_store_l1_M_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits
	mySetUp(CREDITS);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	//manually put the ADDR in the hash table, and set the client state to
	//anything but I.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);

	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_M;
	MESI_client_state_t old_state = client->state;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);

	Manifold::StopAt(When + PROC_CACHE+ 10);
	Manifold::Run();

        //verify the downstream credits is unchanged.
        CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits);

    }



    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): store hit in E state
    //!
    //! Empty cache; manually put a tag in the hash table and set the state of
    //! the corresponding client to E. Send a request with the same
    //! address; it should hit, and a response is sent back. Client state changes to M.
    //! Downstream_credits remains unchanged.
    void test_handle_processor_request_store_l1_E_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits
	mySetUp(CREDITS);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	//manually put the ADDR in the hash table, and set the client state to
	//anything but I.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);

	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_E;
	MESI_client_state_t old_state = client->state;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt + PROC_CACHE+ 10);
	Manifold::Run();

        //verify the downstream credits is unchanged.
        CPPUNIT_ASSERT_EQUAL(CREDITS, m_cachep->m_downstream_credits);

    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): store hit in S state
    //!
    //! Empty cache; manually put a tag in the hash table and set the state of
    //! the corresponding client to S. Send a request with the same
    //! address; it should hit; a MESI_CM_I_to_E request is sent to manager;
    //! client enters SE.
    //! Downstream credits is decremented.
    void test_handle_processor_request_store_l1_S_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	//manually put the ADDR in the hash table, and set the client state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);

	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_S;
	MESI_client_state_t old_state = client->state;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt + PROC_CACHE+ 10);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits);

    }
























//########
//response
//########

    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): load miss; manager in E; owner in E
    //!
    //! Empty cache; load with a random address; the following sequence happens:
    //! 1. Client --> manager, MESI_CM_I_to_S; client enters IE state.
    //! 2. Manager(in E) --> owner, MC_FWD_S.
    //! 3. Owner --> client, CC_S_DATA.
    //! 3. Client --> manager, CM_UNBLOCK_S; client enters S state.
    //! 4. L1 cache now sends a response back to processor.
    //!
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //! We use the mock object to act as the both forwarding client and manager.
    //! Downstream credits is decremented.
    void test_handle_peer_and_manager_request_load_l1_I_l2_E_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 10 + HT_LOOKUP;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_CC_S_DATA;

	NetworkPacket* ownerPkt = new NetworkPacket;
	ownerPkt->type = m_cachep->COH_MSG;
	ownerPkt->dst = NODE_ID;
	*((Coh_msg*)(ownerPkt->data)) = req;
	ownerPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, ownerPkt);
	When += PROC_CACHE + 10 + HT_LOOKUP;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

	//verify downstream gets credits
	vector<NetworkPacket>& credits = m_lowerp->get_credit_pkts();
        CPPUNIT_ASSERT_EQUAL(1, (int)credits.size());

    }




    //======================================================================
    //======================================================================
    //! Same as above; downstream credits initialized to 1. Only 1 message is
    //! sent out.
    void test_handle_peer_and_manager_request_load_l1_I_l2_E_1()
    {
	int CREDITS = 1;  //downstream credits initialized to 1.
	mySetUp(CREDITS);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_CC_S_DATA;

	NetworkPacket* ownerPkt = new NetworkPacket;
	ownerPkt->type = m_cachep->COH_MSG;
	ownerPkt->dst = NODE_ID;
	*((Coh_msg*)(ownerPkt->data)) = req;
	ownerPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, ownerPkt);
	When += PROC_CACHE + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify downstream gets 1 pkt
	vector<NetworkPacket>& pkts = m_lowerp->get_pkts();
        CPPUNIT_ASSERT_EQUAL(1, (int)pkts.size());

        CPPUNIT_ASSERT_EQUAL(0, m_cachep->m_downstream_credits);

	//verify downstream gets credits
	vector<NetworkPacket>& credits = m_lowerp->get_credit_pkts();
        CPPUNIT_ASSERT_EQUAL(1, (int)credits.size());

    }




    //======================================================================
    //======================================================================
    //! Same as above; downstream credits initialized to 1. Send a credit from
    //! downstream to cache. 2 msgs are sent out.
    void test_handle_peer_and_manager_request_load_l1_I_l2_E_2()
    {
	int CREDITS = 1;  //downstream credits initialized to 1.
	mySetUp(CREDITS);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_CC_S_DATA;

	NetworkPacket* ownerPkt = new NetworkPacket;
	ownerPkt->type = m_cachep->COH_MSG;
	ownerPkt->dst = NODE_ID;
	*((Coh_msg*)(ownerPkt->data)) = req;
	ownerPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, ownerPkt);
	When += PROC_CACHE + 10;

	NetworkPacket* credit = new NetworkPacket;
	credit->type = m_cachep->CREDIT_MSG;

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, credit);
	When += PROC_CACHE + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify downstream gets 2 pkts
	vector<NetworkPacket>& pkts = m_lowerp->get_pkts();
        CPPUNIT_ASSERT_EQUAL(2, (int)pkts.size());

        CPPUNIT_ASSERT_EQUAL(0, m_cachep->m_downstream_credits);

	//verify downstream gets credits
	vector<NetworkPacket>& credits = m_lowerp->get_credit_pkts();
        CPPUNIT_ASSERT_EQUAL(1, (int)credits.size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): load miss; manager in E; owner in M
    //!
    //! Empty cache; load with a random address; the following sequence happens:
    //! 1. Client --> manager, CM_I_to_S; client enters IE state.
    //! 2. Manager(in E) --> owner, MC_FWD_S.
    //! 3. Owner --> client, CC_S_DATA.
    //! 4. Client --> manager, MESI_CM_UNBLOCK_S; client enters S state.
    //! 4. L1 cache now sends a response back to processor.
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //! We use the mock object to act as the both forwarding client and manager.
    //! Downstream credits is decremented.
    void test_handle_peer_and_manager_request_load_l1_I_l2_E_owner_M_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + HT_LOOKUP + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_CC_S_DATA;

	NetworkPacket* ownerPkt = new NetworkPacket;
	ownerPkt->type = m_cachep->COH_MSG;
	ownerPkt->dst = NODE_ID;
	*((Coh_msg*)(ownerPkt->data)) = req;
	ownerPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, ownerPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): load miss; manager in S.
    //!
    //! Empty cache; load with a random address; the following sequence happens:
    //! 1. Client --> manager, CM_I_to_S; client enters IE state.
    //! 2. Manager(in S) --> client, MC_GRANT_S_DATA.
    //! 3. Client --> manager, CM_UNBLOCK_S ; client enters S state.
    //! 4. L1 cache now sends a response back to processor.
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //! We use the mock object to act as manager.
    void test_handle_peer_and_manager_request_load_l1_I_l2_S_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + HT_LOOKUP + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_S_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);


	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): load miss; manager in I.
    //!
    //! Empty cache; load with a random address; the following sequence happens:
    //! 1. Client --> manager, CM_I_to_S; client enters IE state.
    //! 2. Manager --> client,  MC_GRANT_E_DATA; manager enters IE state.
    //! 3. Client --> manager, CM_UNBLOCK_E; client enters E state.
    //! 4. L1 cache now sends a response back to processor.
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //! We use the mock object to act as the manager.
    void test_handle_peer_and_manager_request_load_l1_I_l2_I_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const unsigned long ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + HT_LOOKUP + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);


	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);


    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): store miss; manager in E; owner in E.
    //!
    //! Empty cache; store with a random address; the following sequence happens:
    //! 1. Client --> manager, CM_I_to_E; client enters IE state.
    //! 2. Manager(in E) --> owner,  MC_FWD_E.
    //! 3. Owner --> client, CC_E_DATA.
    //! 4. Client --> manager, CM_UNBLOCK_E to manager; client enters E state, then to M state.
    //! 5. L1 cache now sends a response back to processor.
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //! We use the mock object to act as both owner and manager.
    void test_handle_peer_and_manager_request_store_l1_I_l2_E_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + HT_LOOKUP + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_CC_E_DATA;

	NetworkPacket* ownerPkt = new NetworkPacket;
	ownerPkt->type = m_cachep->COH_MSG;
	ownerPkt->dst = NODE_ID;
	*((Coh_msg*)(ownerPkt->data)) = req;
	ownerPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, ownerPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

    }



    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): store miss; manager in E; owner in M.
    //!
    //! Empty cache; store with a random address; the following sequence happens:
    //! 1. Client sends MESI_CM_I_to_E to manager; client enters IE state.
    //! 2. Manager in E state, so it sends MESI_MC_FWD_E to owner, which in turn sends
    //! MESI_CC_M_DATA to client.
    //! 3. Client sends MESI_CM_UNBLOCK_E to manager; client enters M state.
    //! 4. L1 cache now sends a response back to processor.
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //! We use the mock object to act as both owner and manager.
    void test_handle_peer_and_manager_request_store_l1_I_l2_E_owner_M_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + HT_LOOKUP + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_CC_M_DATA;

	NetworkPacket* ownerPkt = new NetworkPacket;
	ownerPkt->type = m_cachep->COH_MSG;
	ownerPkt->dst = NODE_ID;
	*((Coh_msg*)(ownerPkt->data)) = req;
	ownerPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, ownerPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

    }



    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): store miss; manager in S
    //!
    //! Empty cache; store with a random address; the following sequence happens:
    //! 1. Client sends MESI_CM_I_to_E to manager; client enters IE state.
    //! 2. Manager in S state, so it invalidates all shares, then sends
    //! MESI_MC_GRANT_E_DATA to client.
    //! 3. Client sends MESI_CM_UNBLOCK_E to manager; client enters E state, then M state.
    //! 4. L1 cache now sends a response back to processor.
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //! We use the mock object to act as manager.
    void test_handle_peer_and_manager_request_store_l1_I_l2_S_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + HT_LOOKUP + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

    }



    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): store miss; manager in I.
    //!
    //! Empty cache; store with a random address; the following sequence happens:
    //! 1. Client sends MESI_CM_I_to_E to manager; client enters IE state.
    //! 2. Manager in I state, so it sends MESI_MC_GRANT_E_DATA to client; manager enters IE state.
    //! 3. Client sends MESI_CM_UNBLOCK_E to manager; client enters E state, then to M state.
    //! 4. L1 cache now sends a response back to processor.
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //! We use the mock object to act as the manager.
    void test_handle_peer_and_manager_request_store_l1_I_l2_I_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + HT_LOOKUP + 10;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): store hit in S state
    //!
    //! Empty cache; manually put a tag in the hash table and set the state of
    //! the corresponding client to S. Send a request with the same
    //! address; it should hit; a MESI_CM_I_to_E request is sent to manager;
    //! client enters SE.
    //! When manager responds with MC_GRANT_E_DATA, client enters E state.
    //!
    //! 1. Client --> manager, CM_I_to_E; client enters SE
    //! 2. Manager --> client, MC_GRANT_E_DATA.
    //! 3. Client --> manager, CM_UNBLOCK_E
    //!
    //! At the end there should be an entry in the hash table, but not in the MSHR
    //! or the stall buffer.
    //!
    void test_handle_peer_and_manager_request_store_hit_l1_S_l2_S_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2 since we need to send 2 msgs out.
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	//manually put the ADDR in the hash table, and set the client state to S.
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);

	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_S;
	MESI_client_state_t old_state = client->state;

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);

	When += PROC_CACHE + HT_LOOKUP + 10;

	//schedule a manager response
        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

    }






















//#####################
//Owner/sharer behavior
//#####################

    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): receive MESI_MC_FWD_E; owner in E
    //!
    //! Test a owner's behavior when it's in E state and receives MESI_MC_FWD_E: it sends
    //! MESI_CC_E_DATA and invalidates itself.
    //! 1. C--->M, MESI_CM_I_to_E; client enters IE state.
    //! 2. C<---M, MESI_MC_GRANT_E_DATA; client enters E state.
    //! (now client is in E)
    //! 3. C<---M, MESI_MC_FWD_E;
    //! 4. C--->forwardee, MESI_CC_E_DATA; client invalidated.
    void test_handle_peer_and_manager_request_owner_E_recv_FWD_E_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 50;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + 50;

        Coh_msg req2;
	req2.type = Coh_msg::COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_MC_FWD_E;

	NetworkPacket* mgrPkt2 = new NetworkPacket;
	mgrPkt2->type = m_cachep->COH_MSG;
	mgrPkt2->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt2->data)) = req2;
	mgrPkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt2);
	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits); //CM_I_to_E, CM_UNBLOCK_E, CC_E_DATA

    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): receive MESI_MC_FWD_E; owner in M
    //!
    //! Test a owner's behavior when it's in M state and receives MESI_MC_FWD_E: it sends
    //! MESI_CC_M_DATA and invalidates itself.
    //! 1. C--->M, MESI_CM_I_to_E for STORE; client enters IE state.
    //! 2. C<---M, MESI_MC_GRANT_E_DATA; client enters E state and then M state.
    //! (now client is in M)
    //! 3. C<---M, MESI_MC_FWD_E;
    //! 4. C--->forwardee, MESI_CC_M_DATA; client invalidated.
    //!
    void test_handle_peer_and_manager_request_owner_M_recv_FWD_E_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 50;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + 50;

        Coh_msg req2;
	req2.type = Coh_msg::COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_MC_FWD_E;

	NetworkPacket* mgrPkt2 = new NetworkPacket;
	mgrPkt2->type = m_cachep->COH_MSG;
	mgrPkt2->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt2->data)) = req2;
	mgrPkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt2);
	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits); //CM_I_to_E, CM_UNBLOCK_E, CC_M_DATA

    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): receive MESI_MC_FWD_S; owner in E
    //!
    //! Test a owner's behavior when it's in E state and receives MESI_MC_FWD_S: it sends
    //! MESI_CC_S_DATA and enters S state.
    //! 1. C--->M, MESI_CM_I_to_S; client enters IE state.
    //! 2. C<---M, MESI_MC_GRANT_E_DATA; client enters E state.
    //! (now client is in E)
    //! 3. C<---M, MESI_MC_FWD_E;
    //! 4. C--->forwardee, MESI_CC_S_DATA; client enters S.
    void test_handle_peer_and_manager_request_owner_E_recv_FWD_S_0()
    {
	int CREDITS = random() % 100 + 4; //downstream credits; at least 4
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 50;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + 50;

        Coh_msg req2;
	req2.type = Coh_msg::COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_MC_FWD_S;

	NetworkPacket* mgrPkt2 = new NetworkPacket;
	mgrPkt2->type = m_cachep->COH_MSG;
	mgrPkt2->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt2->data)) = req2;
	mgrPkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt2);
	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 4, m_cachep->m_downstream_credits); //CM_I_to_S, CM_UNBLOCK_E, CC_S_DATA, CM_CLEAN

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): receive MESI_MC_FWD_S; owner in M
    //!
    //! Test a owner's behavior when it's in M state and receives MESI_MC_FWD_S: it sends
    //! MESI_CC_S_DATA and MESI_CM_WRITEBACK, and enters S state.
    //! 1. C--->M, MESI_CM_I_to_E for STORE; client enters IE state.
    //! 2. C<---M, MESI_MC_GRANT_E_DATA; client enters E state and then M state.
    //! (now client is in M)
    //! 3. C<---M, MESI_MC_FWD_S;
    //! 4. C--->forwardee, MESI_CC_S_DATA
    //! 5. C--->M, MESI_CM_WRITEBACK
    //!
    void test_handle_peer_and_manager_request_owner_M_recv_FWD_S_0()
    {
	int CREDITS = random() % 100 + 4; //downstream credits; at least 4
	mySetUp(CREDITS);

	//create a STORE request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 50;

        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + 50;

        Coh_msg req2;
	req2.type = Coh_msg::COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_MC_FWD_S;

	NetworkPacket* mgrPkt2 = new NetworkPacket;
	mgrPkt2->type = m_cachep->COH_MSG;
	mgrPkt2->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt2->data)) = req2;
	mgrPkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt2);
	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 4, m_cachep->m_downstream_credits); //CM_I_to_E, CM_UNBLOCK_E, CC_S_DATA, CM_WRITEBACK
    }












//#########
// eviction
//#########
    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): load miss; evicted in E;
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the evicted entry is in E, it sends MESI_CM_E_to_I to manager.
    void test_handle_processor_request_load_eviction_l1_E_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in E state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_E;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt + PROC_CACHE + 10);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits);
    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): load miss; evicted in S;
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the evicted entry is in S, it transits to I state; no messages to the manager.
    void test_handle_processor_request_load_eviction_l1_S_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in S state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_S;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt + PROC_CACHE + 10);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //CM_I_to_S after eviction

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): load miss; evicted in M;
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the evicted entry is in M, it sends MESI_CM_M_to_I to manager.
    void test_handle_processor_request_load_eviction_l1_M_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in M state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_M;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt + PROC_CACHE + 10);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //CM_M_to_I

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request() and handle_peer_and_manager_request():
    //! load miss; evicted in E;
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! 1. victim--->M, MESI_CM_E_to_I.
    //! 2. victim<---M, MESI_MC_GRANT_I.
    //! 3. victim--->M, MESI_CM_UNBLOCK_I.
    //!
    //! Now the new request is woken up.
    //! 1. C--->M, MESI_CM_I_to_S.
    void test_handle_peer_and_manager_request_load_eviction_l1_E_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in E state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_E;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 50;

        Coh_msg req2;
	req2.type = Coh_msg::COH_RPLY;
	req2.addr = VICTIM_ADDR;
	req2.msg = MESI_MC_GRANT_I;

	NetworkPacket* mgrPkt2 = new NetworkPacket;
	mgrPkt2->type = m_cachep->COH_MSG;
	mgrPkt2->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt2->data)) = req2;
	mgrPkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt2);
	When += PROC_CACHE + 50;


	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits); // CM_E_to_I, CM_UNBLOCK_I, CM_I_to_S

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request() and handle_peer_and_manager_request():
    //! load miss; evicted in S;
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //!
    //! Since victim is in S state, it silently transits to I, and the new request
    //! continues:
    //! 1. C--->M, MESI_CM_I_to_S.
    void test_handle_peer_and_manager_request_load_eviction_l1_S_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in S state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_S;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); //CM_I_to_S after eviction

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request() and handle_peer_and_manager_request():
    //! load miss; evicted in M;
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! 1. victim--->M, MESI_CM_M_to_I.
    //! 2. victim<---M, MESI_MC_GRANT_I.
    //! 3. victim--->M, MESI_CM_UNBLOCK_I.
    //!
    //! Now the new request is woken up.
    //! 1. C--->M, MESI_CM_I_to_S.
    void test_handle_peer_and_manager_request_load_eviction_l1_M_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in M state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_M;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 50;

        Coh_msg req2;
	req2.type = Coh_msg::COH_RPLY;
	req2.addr = VICTIM_ADDR;
	req2.msg = MESI_MC_GRANT_I;

	NetworkPacket* mgrPkt2 = new NetworkPacket;
	mgrPkt2->type = m_cachep->COH_MSG;
	mgrPkt2->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt2->data)) = req2;
	mgrPkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt2);
	When += PROC_CACHE + 50;


	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits); // CM_M_to_I, CM_UNBLOCK_I, CM_I_to_S

    }



















//############
//Invalidation
//############

    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): DEMAND_I, L1 in E state.
    //!
    //! When manager needs to invalidate a client, it sends MESI_MC_DEMAND_I.
    //! If client is in E state, it sends MESI_CM_UNBLOCK_I, and enters I state.
    //!
    void test_handle_peer_and_manager_request_invalidation_l1_E_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	//manually put the ADDR in the hash table, and set the client state to E
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);

	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_E;

	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a MESI_MC_DEMAND_I
        Coh_msg req;
	req.type = Coh_msg::COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); // CM_UNBLOCK_I

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): DEMAND_I, L1 in M state.
    //!
    //! When manager needs to invalidate a client, it sends MESI_MC_DEMAND_I.
    //! If client is in M state, it sends MESI_CM_UNBLOCK_I_DIRTY, and enters I state.
    //!
    void test_handle_peer_and_manager_request_invalidation_l1_M_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	const paddr_t ADDR = random();

	//manually put the ADDR in the hash table, and set the client state to E
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);

	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_M;

	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a MESI_MC_DEMAND_I
        Coh_msg req;
	req.type = Coh_msg::COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); // CM_UNBLOCK_I_DIRTY

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): DEMAND_I, L1 in S state.
    //!
    //! When manager needs to invalidate a client, it sends MESI_MC_DEMAND_I.
    //! If client is in S state, it sends MESI_CM_UNBLOCK_I, and enters I state.
    //!
    void test_handle_peer_and_manager_request_invalidation_l1_S_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	//manually put the ADDR in the hash table, and set the client state to E
	m_cachep->my_table->reserve_block_for(ADDR);
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	CPPUNIT_ASSERT(entry != 0);

	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_S;

	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a MESI_MC_DEMAND_I
        Coh_msg req;
	req.type = Coh_msg::COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits); // CM_UNBLOCK_I

    }
















//##########
//MSHR_STALL
//##########

    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request():
    //! no mshr entry available (i.e., C_MSHR_STALL).
    //!
    //! Create N requests to fill up the MSHR; then create one more request; it should
    //! have a C_MSHR_STALL.
    //!
    //! 1. C--->M, N MESI_CM_I_to_S.
    //! 2. Another request, which is stalled.
    //!
    //! Verify C_MSHR_STALL is created and put in the stall buffer.
    void test_handle_processor_request_MSHR_STALL_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();


	const paddr_t TAG_MASK = m_cachep->mshr->get_tag_mask();
	const int NUM_TAGS = m_cachep->mshr->assoc;
	assert(MSHR_SIZE == m_cachep->mshr->assoc);

        if(m_cachep->m_downstream_credits < NUM_TAGS) {
	    CREDITS = m_cachep->m_downstream_credits = NUM_TAGS + 1;
	}

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

	//create N LOAD requests
	Proc_req* crequests[NUM_TAGS];
	for(int i=0; i<NUM_TAGS; i++) {
	    crequests[i] = new Proc_req(0, 123, addrs[i], Proc_req::LOAD);
	}


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	for(int i=0; i<NUM_TAGS; i++) {
	    Manifold::Schedule(When, &MockProc::send_creq, m_procp, crequests[i]);
	}
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 10;

	//create a LOAD request
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq); //this would C_MSHR_STALL
	When += PROC_CACHE + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_TAGS, m_cachep->m_downstream_credits); // NUM_TAGS CM_I_to_S

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request():
    //! no mshr entry available (i.e., C_MSHR_STALL).
    //!
    //! Create N requests to fill up the MSHR; then create one more request; it should
    //! have a C_MSHR_STALL.
    //! Send a response to one of the previous N requests; this would free up an mshr entry.
    //! Verify the freed mshr entry is given to the latest request and it's freed from the
    //! stall buffer.
    //!
    //! 1. C--->M, N MESI_CM_I_to_S.
    //! 2. Another request, which is stalled.
    //! 3. C<---M, MESI_MC_GRANT_E_DATA.
    //! 4. C--->M, MESI_CM_I_to_E, or MESI_CM_I_to_S.
    //!
    //! Verify C_MSHR_STALL is woken up and removed from stall buffer.
    void test_handle_peer_and_manager_request_MSHR_STALL_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();


	const paddr_t TAG_MASK = m_cachep->mshr->get_tag_mask();
	const int NUM_TAGS = m_cachep->mshr->assoc;
	assert(MSHR_SIZE == m_cachep->mshr->assoc);

        if(m_cachep->m_downstream_credits < NUM_TAGS+2) {
	    CREDITS = m_cachep->m_downstream_credits = NUM_TAGS + 2;
	}

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



	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	for(int i=0; i<NUM_TAGS; i++) {
	    Proc_req* crequest = new Proc_req(0, 123, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_creq, m_procp, crequest);
	}
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += PROC_CACHE + 10;

	//create a LOAD request
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq); //this would C_MSHR_STALL
	When += PROC_CACHE + HT_LOOKUP + 10;

	//randomly pick one request to reply to.
        Coh_msg reply;
	reply.type = Coh_msg::COH_RPLY;
	const paddr_t REPLY_ADDR = addrs[random() % NUM_TAGS];
	reply.addr = REPLY_ADDR;
	reply.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = reply;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_TAGS - 2, m_cachep->m_downstream_credits); // NUM_TAGS CM_I_to_S, CM_UNBLOCK_S, CM_I_to_S

    }


















//###############
//PREV_PEND_STALL
//###############

    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request():
    //! an mshr entry for the same line already exists (i.e., PREV_PEND_STALL).
    //!
    //! Send a request to L1; then send another with same address. The 2nd one
    //! should have a C_PREV_PEND_STALL.
    //!
    //! Verify C_PREV_PEND_STALL is created and put in the stall buffer.
    void test_handle_processor_request_PREV_PEND_STALL_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	When += PROC_CACHE + 10;

	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == ADDR);

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), m_cachep->my_table->get_line_addr(ADDR2));

	Proc_req* creq2 = new Proc_req(0, 123, ADDR2, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq2); //this would C_PREV_PEND_STALL
	When += PROC_CACHE + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits);

    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): load request;
    //! an mshr entry for the same line already exists (i.e., PREV_PEND_STALL).
    //!
    //! Send a request the L1; then send another with same address. The 2nd one
    //! should have a C_PREV_PEND_STALL.
    //! Send a response to the first request; the stored request should be processed.
    //!
    //! Verify C_PREV_PEND_STALL is removed from the stall buffer.
    //!
    void test_handle_peer_and_manager_request_PREV_PEND_STALL_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	Manifold::unhalt();
	Ticks_t When = 1;

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	When += PROC_CACHE + 10;

	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == ADDR);

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), m_cachep->my_table->get_line_addr(ADDR2));

	Proc_req* creq2 = new Proc_req(0, 123, ADDR2, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq2); //this would C_PREV_PEND_STALL
	When += PROC_CACHE + HT_LOOKUP + 10;

	//schedule a response to the 1st request
        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): store request;
    //! an mshr entry for the same line already exists (i.e., PREV_PEND_STALL).
    //!
    //! Send a request the L1; then send another with same address. The 2nd one
    //! should have a C_PREV_PEND_STALL.
    //! Send a response to the first request; the stored request should be processed.
    //!
    //! Verify C_PREV_PEND_STALL is removed from the stall buffer.
    //!
    void test_handle_peer_and_manager_request_PREV_PEND_STALL_1()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	Manifold::unhalt();
	Ticks_t When = 1;

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	When += PROC_CACHE + 10;

	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == ADDR);

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), m_cachep->my_table->get_line_addr(ADDR2));

	Proc_req* creq2 = new Proc_req(0, 123, ADDR2, Proc_req::STORE);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq2); //this would C_PREV_PEND_STALL
	When += PROC_CACHE + HT_LOOKUP + 10;

	//schedule a response to the 1st request
        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits); //MESI_CM_I_to_S, MESI_CM_UNBLOCK_E, both for 1st request
	                                            //2nd request store hit, so it changes to M without sending
						    //messages.

    }






















//##############
//LRU_BUSY_STALL
//##############

    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): LRU_BUSY_STALL.
    //!
    //! Fill up a cache set; then send a request with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the evicted entry is in a transient state, the new request should have
    //! a LRU_BUSY_STALL.
    void test_handle_processor_request_LRU_BUSY_STALL_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

        if(m_cachep->m_downstream_credits < NUM_TAGS) {
	    CREDITS = m_cachep->m_downstream_credits = NUM_TAGS;
	}

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
	    Proc_req* creq = new Proc_req(0, 123, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	}
	When += PROC_CACHE + 10;

	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD); //this one would LRU_BUSY_STALL
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	When += PROC_CACHE + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_TAGS, m_cachep->m_downstream_credits);
    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): LRU_BUSY_STALL.
    //!
    //! Fill up a cache set; then send a request with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the evicted entry is in a transient state, the new request should have
    //! a LRU_BUSY_STALL.
    //! Send a response to allow the victim to finish; this would remove the LRU_BUSY_STALL
    //! from the stall buffer and process it.
    void test_handle_peer_and_manager_request_LRU_BUSY_STALL_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();

	const paddr_t TAG_MASK = m_cachep->my_table->get_tag_mask();
	const int NUM_TAGS = m_cachep->my_table->assoc;

        if(m_cachep->m_downstream_credits < NUM_TAGS+2) {
	    CREDITS = m_cachep->m_downstream_credits = NUM_TAGS+2;
	}

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
	    Proc_req* creq = new Proc_req(0, 123, addrs[i], Proc_req::LOAD);
	    Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	}
	When += PROC_CACHE + HT_LOOKUP + 10;

	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD); //this one would LRU_BUSY_STALL
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	When += PROC_CACHE + HT_LOOKUP + 10;

	//schedule a response to the 1st request (victim)
        Coh_msg req;
	req.type = Coh_msg::COH_RPLY;
	req.addr = VICTIM_ADDR;
	req.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - NUM_TAGS-2, m_cachep->m_downstream_credits);
    }























//###########
//TRANS_STALL
//###########


    //======================================================================
    //======================================================================
    //! @brief Test handle_processor_request(): TRANS_STALL: hit entry beying evicted.
    //!
    //! Fill up a cache set and put them in the E state, then send a request with a new
    //! tag for the same set. This causes the LRU entry to be evicted.
    //! While the victim is being evicted, send another request that hits on the victim.
    //! This request would have a C_TRANS_STALL.
    //! Verify a C_TRANS_STALL is created and put in the stall buffer.
    void test_handle_processor_request_TRANS_STALL_0()
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
	    m_cachep->my_table->reserve_block_for(ad);
	    addrs[i] = ad;
	}

	const paddr_t VICTIM_ADDR = addrs[0];

	//put the state of all the lines in the set to E
	for(int i=0; i<NUM_TAGS; i++) {
	    hash_entry* entry = m_cachep->my_table->get_entry(addrs[i]);
	    assert(entry);
	    MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	    client->state = MESI_C_E;
	}



	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a request which will miss and cause an eviction
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	When += PROC_CACHE + 10;

	//schedule another request which will hit on the victim
	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(VICTIM_ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == VICTIM_ADDR);

	Proc_req* creq2 = new Proc_req(0, 123, ADDR2, Proc_req::LOAD); //this one would TRANS_STALL
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq2);
	When += PROC_CACHE + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits);

    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_peer_and_manager_request(): TRANS_STALL: hit entry beying evicted.
    //!
    //! Fill up a cache set and put them in the E state, then send a request with a new
    //! tag for the same set. This causes the LRU entry to be evicted.
    //! While the victim is being evicted, send another request that hits on the victim.
    //! This request would have a C_TRANS_STALL.
    //! Send a response to the victim's eviction request; this causes the victim to be evicted
    //! and the 1st request woken up.
    //! Sned a response to the 1st request; this causes the 2nd request to be woken up.
    //!
    //! Verify a C_TRANS_STALL is removed from the stall buffer and processed.
    void test_handle_peer_and_manager_request_TRANS_STALL_0()
    {
	int CREDITS = random() % 100 + 5; //downstream credits; at least 5
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

	//put the state of all the lines in the set to E
	for(int i=0; i<NUM_TAGS; i++) {
	    hash_entry* entry = m_cachep->my_table->get_entry(addrs[i]);
	    assert(entry);
	    MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	    client->state = MESI_C_E;
	}



	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a request which will miss and cause an eviction
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	When += PROC_CACHE + HT_LOOKUP + 10;

	//schedule another request which will hit on the victim
	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(VICTIM_ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == VICTIM_ADDR);

	Proc_req* creq2 = new Proc_req(0, 123, ADDR2, Proc_req::LOAD); //this one would TRANS_STALL
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq2);
	When += PROC_CACHE + HT_LOOKUP + 10;

	//schedule a response to the victim's eviction request
        Coh_msg resp;
	resp.type = Coh_msg::COH_RPLY;
	resp.addr = VICTIM_ADDR;
	resp.msg = MESI_MC_GRANT_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = resp;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	//schedule a response to the 1st request
        Coh_msg resp2;
	resp2.type = Coh_msg::COH_RPLY;
	resp2.addr = ADDR;
	resp2.msg = MESI_MC_GRANT_E_DATA;

	NetworkPacket* mgrPkt2 = new NetworkPacket;
	mgrPkt2->type = m_cachep->COH_MSG;
	mgrPkt2->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt2->data)) = resp2;
	mgrPkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt2);
	When += PROC_CACHE + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 5, m_cachep->m_downstream_credits);//CM_E_to_I, CM_UNBLOCK_I, CM_I_to_S, CM_UNBLOCK_E, CM_E_to_I

    }





















//####################
//Respond with default
//####################

    //======================================================================
    //======================================================================
    //! @brief Test respond_with_default().
    //!
    //! Client receives a MESI_MC_DEMAND_I, but there's no mshr or hash table entry
    //! for the address. This happens because when client goes from S to I when it's
    //! being evicted, it doesn't notify the manager, so the manager still thinks it's
    //! in S state.
    //!
    //! 1. M--->C, MESI_MC_DEMAND_I
    //! 2. M<---C, MESI_MC_UNBLOCK_I
    //!
    void test_respond_with_default_0()
    {
	int CREDITS = random() % 100 + 1; //downstream credits; at least 1
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();


	Manifold::unhalt();
	Ticks_t When = 1;

	//schedule a MESI_MC_DEMAND_I
	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.src_id = m_l2map->lookup(ADDR);
	req.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 1, m_cachep->m_downstream_credits);//CM_UNBLOCK_I
    }




    //======================================================================
    //======================================================================
    //! @brief Test respond_with_default().
    //!
    //! Client receives a MESI_MC_DEMAND_I, there is an MSHR but no hash entry.
    //! This happens when the address was previously in S state and was evicted.
    //! Since it's in S, it didn't notify manager of the eviction. Later on, if
    //! there's another request for the address, an MSHR entry is allocated. If
    //! at this point, the line is being evicted, since manager still thinks client
    //! is a sharer, a DEMAND_I is sent. Client need respond with UNBLOCK_I.
    //!
    //! 1. processor --> C, read request; an MSHR entry is allocated.
    //! 2. M--->C, MESI_MC_DEMAND_I
    //! 3. M<---C, MESI_MC_UNBLOCK_I
    //!
    void test_respond_with_default_1()
    {

	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//Fill up a set.
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

	//put the state of all the lines in the set to E
	for(int i=0; i<NUM_TAGS; i++) {
	    hash_entry* entry = m_cachep->my_table->get_entry(set_entries[i]);
	    assert(entry);
	    MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	    client->state = MESI_C_E;
	}



	Manifold::unhalt();
	Ticks_t When = 1;


	//create a LOAD request that causes eviction: there is MSHR but no hash entry for ADDR
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);

	When += PROC_CACHE + HT_LOOKUP + 10;

	//schedule a MESI_MC_DEMAND_I
	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.src_id = m_l2map->lookup(ADDR);
	req.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = req;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);//CM_E_to_I, CM_UNBLOCK_I

    }























//##################
//request race
//##################

    //======================================================================
    //======================================================================
    //! @brief Test request race: client in EI, receives FWD_E.
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the client is in E, it sends MESI_CM_E_to_I to manager. Before
    //! the eviction completes, manager sends a FWD_E request, which is processed.
    //! Client enters I, and wakes up the waiting request.
    //!
    //! 1. Fill up a set, put LRU in E state. Make a read request to the same set.
    //!    This causes eviction of LRU.
    //! 2. Client --> manager, CM_E_to_I. Client enters EI.
    //! 3. Manager --> client, MC_FWD_E.
    //! 4. Client --> requestor, CC_E_DATA.
    //! 5. Eviction completes. Original request is processed. Client --> manager, CM_I_to_S.
    //!
    void test_request_race_owner_EI_recv_FWD_E_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in E state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_E;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in EI now; before the eviction is completed, send MC_FWD_E to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = VICTIM_ADDR;
	umreq.msg = MESI_MC_FWD_E;
	const int FWDID = random() % 1024;
	umreq.forward_id = FWDID;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);

	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits);//CM_E_to_I, CC_E_DATA, CM_I_to_S

    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: client in EI, receives FWD_S.
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the client is in E, it sends MESI_CM_E_to_I to manager. Before
    //! the eviction completes, manager sends a FWD_S request, which is processed.
    //! Client enters I, and wakes up the waiting request.
    //!
    //! 1. Fill up a set, put LRU in E state. Make a read request to the same set.
    //!    This causes eviction of LRU.
    //! 2. Client --> manager, CM_E_to_I. Client enters EI.
    //! 3. Manager --> client, MC_FWD_S.
    //! 4. Client --> requestor, CC_E_DATA.
    //! 6. Eviction completes. Original request is processed. Client --> manager, CM_I_to_S.
    //!
    void test_request_race_owner_EI_recv_FWD_S_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in E state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_E;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in EI now; before the eviction is completed, send MC_FWD_S to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = VICTIM_ADDR;
	umreq.msg = MESI_MC_FWD_S;
	const int FWDID = random() % 1024;
	umreq.forward_id = FWDID;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);

	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits);//CM_E_to_I, CC_E_DATA, CM_I_to_S

    }



    //======================================================================
    //======================================================================
    //! @brief Test request race: client in MI, receives FWD_E.
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the client is in M, it sends MESI_CM_M_to_I to manager. Before
    //! the eviction completes, manager sends a FWD_E request, which is processed.
    //! Client enters I, and wakes up the waiting request.
    //!
    //! 1. Fill up a set, put LRU in E state. Make a read request to the same set.
    //!    This causes eviction of LRU.
    //! 2. Client --> manager, CM_M_to_I. Client enters MI.
    //! 3. Manager --> client, MC_FWD_E.
    //! 4. Client --> requestor, CC_M_DATA.
    //! 5. Eviction completes. Original request is processed. Client --> manager, CM_I_to_S.
    //!
    void test_request_race_owner_MI_recv_FWD_E_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in M state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_M;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in MI now; before the eviction is completed, send MC_FWD_E to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = VICTIM_ADDR;
	umreq.msg = MESI_MC_FWD_E;
	const int FWDID = random() % 1024;
	umreq.forward_id = FWDID;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits);//CM_M_to_I, CC_M_DATA, CM_I_to_S

    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: client in MI, receives FWD_S.
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the client is in M, it sends MESI_CM_M_to_I to manager. Before
    //! the eviction completes, manager sends a FWD_S request, which is processed.
    //! Client enters I, and wakes up the waiting request.
    //!
    //! 1. Fill up a set, put LRU in E state. Make a read request to the same set.
    //!    This causes eviction of LRU.
    //! 2. Client --> manager, CM_M_to_I. Client enters MI.
    //! 3. Manager --> client, MC_FWD_S.
    //! 4. Client --> requestor, CC_S_DATA. //note it's CC_S_DATA, not CC_M_DATA.
    //! 5. Client --> manager, CM_WRITEBACK.
    //! 6. Eviction completes. Original request is processed. Client --> manager, CM_I_to_S.
    //!
    void test_request_race_owner_MI_recv_FWD_S_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in M state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_M;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in MI now; before the eviction is completed, send MC_FWD_S to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = VICTIM_ADDR;
	umreq.msg = MESI_MC_FWD_S;
	const int FWDID = random() % 1024;
	umreq.forward_id = FWDID;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);
	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits);//CM_M_to_I, CC_M_DATA, CM_I_to_S

    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: client in EI, receives DEMAND_I.
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the client is in E, it sends MESI_CM_E_to_I to manager. Before
    //! the eviction completes, manager sends a DEMAND_I request, which is processed.
    //! Client enters I, and wakes up the waiting request.
    //!
    //! 1. Fill up a set, put LRU in E state. Make a read request to the same set.
    //!    This causes eviction of LRU.
    //! 2. Client --> manager, CM_E_to_I. Client enters EI.
    //! 3. Manager --> client, MC_DEMAND_I.
    //! 4. Client --> manager, CM_UNBLOCK_I.
    //! 5. Eviction completes. Original request is processed. Client --> manager, CM_I_to_S.
    //!
    void test_request_race_owner_EI_recv_DEMAND_I_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in E state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_E;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in EI now; before the eviction is completed, send MC_DEMAND_I to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = VICTIM_ADDR;
	umreq.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);

	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits);//CM_E_to_I, CC_M_DATA, CM_I_to_S

    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: client in MI, receives DEMAND_I.
    //!
    //! Fill up a cache set; then LOAD with a new tag for the same set. This
    //! causes the LRU entry to be evicted.
    //! Since the client is in M, it sends MESI_CM_M_to_I to manager. Before
    //! the eviction completes, manager sends a DEMAND_I request, which is processed.
    //! Client enters I, and wakes up the waiting request.
    //!
    //! 1. Fill up a set, put LRU in M state. Make a read request to the same set.
    //!    This causes eviction of LRU.
    //! 2. Client --> manager, CM_M_to_I. Client enters MI.
    //! 3. Manager --> client, MC_DEMAND_I.
    //! 4. Client --> manager, CM_UNBLOCK_I_DIRTY.
    //! 5. Eviction completes. Original request is processed. Client --> manager, CM_I_to_S.
    //!
    void test_request_race_owner_MI_recv_DEMAND_I_0()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a LOAD request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::LOAD);

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

	//put the victim's client in E state
	hash_entry* victim_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	assert(victim_entry);
	MESI_client* victim_client = dynamic_cast<MESI_client*>(m_cachep->clients[victim_entry->get_idx()]);
	victim_client->state = MESI_C_M;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in MI now; before the eviction is completed, send MC_DEMAND_I to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = VICTIM_ADDR;
	umreq.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);

	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits);//CM_M_to_I, CM_UNBLOCK_I_DIRTY, CM_I_to_S

    }



    //======================================================================
    //======================================================================
    //! @brief Test request race: client in SE, receives DEMAND_I.
    //!
    //! Put a tag in the hash table, set client's state to S. Send a write request
    //! to client, which sends I_to_E to manager and enters SE state.
    //! At this point, send DEMAND_I to client. Client replies with UNBLOCK_I.
    //!
    //! 1. Client --> manager, CM_I_to_E. Client enters SE.
    //! 2. Manager --> client, MC_DEMAND_I.
    //! 3. Client --> manager, CM_UNBLOCK_I.
    //!
    void test_request_race_client_SE_recv_DEMAND_I_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	//put ADDR in hash table.
	m_cachep->my_table->reserve_block_for(ADDR);

	//put the client in S state
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	assert(entry);
	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_S;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in SE now; send MC_DEMAND_I to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = ADDR;
	umreq.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);

	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);//CM_I_to_E, CM_UNBLOCK_I

    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: client in SE, receives DEMAND_I.
    //!
    //! Put a tag in the hash table, set client's state to S. Send a write request
    //! to client, which sends I_to_E to manager and enters SE state.
    //! At this point, send DEMAND_I to client. Client replies with UNBLOCK_I.
    //! Finally, client receives CC_M_DATA, and replies to manager with UNBLOCK_E.
    //!
    //! 1. Client --> manager, CM_I_to_E. Client enters SE.
    //! 2. Manager --> client, MC_DEMAND_I.
    //! 3. Client --> manager, CM_UNBLOCK_I.
    //! 4. Owner --> client, CC_M_DATA.
    //! 5. Client --> manager, UNBLOCK_E.
    //!
    void test_request_race_client_SE_recv_DEMAND_I_1()
    {
	int CREDITS = random() % 100 + 3; //downstream credits; at least 3
	mySetUp(CREDITS);

	//create a request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	//put ADDR in hash table.
	m_cachep->my_table->reserve_block_for(ADDR);

	//put the client in S state
	hash_entry* entry = m_cachep->my_table->get_entry(ADDR);
	assert(entry);
	MESI_client* client = dynamic_cast<MESI_client*>(m_cachep->clients[entry->get_idx()]);
	client->state = MESI_C_S;


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in SE now; send MC_DEMAND_I to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = ADDR;
	umreq.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);

	When += PROC_CACHE + 50;

	//the client still in SE; send CC_M_DATA to it.
        Coh_msg umreq2;
	umreq2.type = Coh_msg::COH_RPLY; //CC_M_DATA is a reply !!!
	umreq2.addr = ADDR;
	umreq2.msg = MESI_CC_M_DATA;

	NetworkPacket* ownerPkt = new NetworkPacket;
	ownerPkt->type = m_cachep->COH_MSG;
	ownerPkt->dst = NODE_ID;
	*((Coh_msg*)(ownerPkt->data)) = umreq2;
	ownerPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, ownerPkt);
	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 3, m_cachep->m_downstream_credits);//CM_I_to_E, CM_UNBLOCK_I, CM_UNBLOCK_E

    }




    //======================================================================
    //======================================================================
    //! @brief Test request race: client in IE, receives DEMAND_I.
    //!
    //! Send a read or write request to cache; client sends I_to_E to manager
    //! and enters IE state.
    //! At this point, send DEMAND_I to client. Client replies with UNBLOCK_I.
    //!
    //! 1. Client --> manager, CM_I_to_E. Client enters IE.
    //! 2. Manager --> client, MC_DEMAND_I.
    //! 3. Client --> manager, CM_UNBLOCK_I.
    //!
    void test_request_race_client_IE_recv_DEMAND_I_0()
    {
	int CREDITS = random() % 100 + 2; //downstream credits; at least 2
	mySetUp(CREDITS);

	//create a request
	const paddr_t ADDR = random();
	Proc_req* creq = new Proc_req(0, 123, ADDR, Proc_req::STORE);

	//verify ADDR not in hash table.
	CPPUNIT_ASSERT(m_cachep->my_table->has_match(ADDR) == false);


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockProc::send_creq, m_procp, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	When += PROC_CACHE + 50;

	//the client is in IE now; send MC_DEMAND_I to it.
        Coh_msg umreq;
	umreq.type = Coh_msg::COH_REQ;
	umreq.addr = ADDR;
	umreq.msg = MESI_MC_DEMAND_I;

	NetworkPacket* mgrPkt = new NetworkPacket;
	mgrPkt->type = m_cachep->COH_MSG;
	mgrPkt->dst = NODE_ID;
	*((Coh_msg*)(mgrPkt->data)) = umreq;
	mgrPkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockLower::send_pkt, m_lowerp, mgrPkt);

	When += PROC_CACHE + 50;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the downstream credits is decremented.
        CPPUNIT_ASSERT_EQUAL(CREDITS - 2, m_cachep->m_downstream_credits);//CM_I_to_E, CM_UNBLOCK_I

    }




    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	/*
	*/
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MESI_L1_cacheTest");
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_load_l1_I_0", &MESI_L1_cacheTest::test_handle_processor_request_load_l1_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_load_l1_ESM_0", &MESI_L1_cacheTest::test_handle_processor_request_load_l1_ESM_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_store_l1_I_0", &MESI_L1_cacheTest::test_handle_processor_request_store_l1_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_store_l1_M_0", &MESI_L1_cacheTest::test_handle_processor_request_store_l1_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_store_l1_E_0", &MESI_L1_cacheTest::test_handle_processor_request_store_l1_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_store_l1_S_0", &MESI_L1_cacheTest::test_handle_processor_request_store_l1_S_0));
        //response
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_l1_I_l2_E_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_l1_I_l2_E_1", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_l1_I_l2_E_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_l1_I_l2_E_2", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_l1_I_l2_E_2));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_l1_I_l2_E_owner_M_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_l1_I_l2_E_owner_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_l1_I_l2_S_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_l1_I_l2_I_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_store_l1_I_l2_E_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_store_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_store_l1_I_l2_E_owner_M_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_store_l1_I_l2_E_owner_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_store_l1_I_l2_S_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_store_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_store_l1_I_l2_I_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_store_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_store_hit_l1_S_l2_S_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_store_hit_l1_S_l2_S_0));
	//owner behavior
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_owner_E_recv_FWD_E_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_owner_E_recv_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_owner_M_recv_FWD_E_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_owner_M_recv_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_owner_E_recv_FWD_S_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_owner_E_recv_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_owner_M_recv_FWD_S_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_owner_M_recv_FWD_S_0));
	//eviction
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_load_eviction_l1_E_0", &MESI_L1_cacheTest::test_handle_processor_request_load_eviction_l1_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_load_eviction_l1_S_0", &MESI_L1_cacheTest::test_handle_processor_request_load_eviction_l1_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_load_eviction_l1_M_0", &MESI_L1_cacheTest::test_handle_processor_request_load_eviction_l1_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_eviction_l1_E_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_eviction_l1_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_eviction_l1_S_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_eviction_l1_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_load_eviction_l1_M_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_load_eviction_l1_M_0));
	//invalidation
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_invalidation_l1_E_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_invalidation_l1_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_invalidation_l1_M_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_invalidation_l1_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_invalidation_l1_S_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_invalidation_l1_S_0));
	//MSHR_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_MSHR_STALL_0", &MESI_L1_cacheTest::test_handle_processor_request_MSHR_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_MSHR_STALL_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_MSHR_STALL_0));
	//PREV_PEND_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_PREV_PEND_STALL_0", &MESI_L1_cacheTest::test_handle_processor_request_PREV_PEND_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_PREV_PEND_STALL_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_PREV_PEND_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_PREV_PEND_STALL_1", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_PREV_PEND_STALL_1));
	//LRU_BUSY_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_LRU_BUSY_STALL_0", &MESI_L1_cacheTest::test_handle_processor_request_LRU_BUSY_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_LRU_BUSY_STALL_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_LRU_BUSY_STALL_0));
	//TRANS_STALL, e.g., the hit entry is being evicted
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_processor_request_TRANS_STALL_0", &MESI_L1_cacheTest::test_handle_processor_request_TRANS_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_handle_peer_and_manager_request_TRANS_STALL_0", &MESI_L1_cacheTest::test_handle_peer_and_manager_request_TRANS_STALL_0));
	//Respond with default
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_respond_with_default_0", &MESI_L1_cacheTest::test_respond_with_default_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_respond_with_default_1", &MESI_L1_cacheTest::test_respond_with_default_1));
	//Request race. A request to manager is being processed, and client receives a request
	//for the same line from manager. There are 8 cases.
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_owner_EI_recv_FWD_E_0", &MESI_L1_cacheTest::test_request_race_owner_EI_recv_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_owner_EI_recv_FWD_S_0", &MESI_L1_cacheTest::test_request_race_owner_EI_recv_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_owner_MI_recv_FWD_E_0", &MESI_L1_cacheTest::test_request_race_owner_MI_recv_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_owner_MI_recv_FWD_S_0", &MESI_L1_cacheTest::test_request_race_owner_MI_recv_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_owner_EI_recv_DEMAND_I_0", &MESI_L1_cacheTest::test_request_race_owner_EI_recv_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_owner_MI_recv_DEMAND_I_0", &MESI_L1_cacheTest::test_request_race_owner_MI_recv_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_client_SE_recv_DEMAND_I_0", &MESI_L1_cacheTest::test_request_race_client_SE_recv_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_client_SE_recv_DEMAND_I_1", &MESI_L1_cacheTest::test_request_race_client_SE_recv_DEMAND_I_1));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L1_cacheTest>("test_request_race_client_IE_recv_DEMAND_I_0", &MESI_L1_cacheTest::test_request_race_client_IE_recv_DEMAND_I_0));
	/*
	*/

	return mySuite;
    }
};


Clock MESI_L1_cacheTest :: MasterClock(MESI_L1_cacheTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MESI_L1_cacheTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


