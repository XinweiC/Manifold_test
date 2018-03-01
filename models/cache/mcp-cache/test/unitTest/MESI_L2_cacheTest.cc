//! This program tests MESI_L2_cache as a component. We use a Mock
//! component and connect the MESI_L2_cache to the component.
//!
//!                    --------
//!                   | MockL1 |
//!                    --------
//!                         | NetworkPacket
//!                  ---------------
//!                 | MESI_L2_cache |
//!                  ---------------

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
#include "MESI_L2_cache.h"
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

//! This class emulates an L1 and a memory controller.
class MockL1 : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};
    void send_req(NetworkPacket* pkt)
    {
	Send(OUT, pkt);
    }

    void handle_incoming(int, NetworkPacket* pkt)
    {
    //cout << "MockProc handle_incoming @ " << Manifold::NowTicks() << endl;
        if(pkt->type == L2_cache :: MEM_MSG) {
	    m_mem_reqs.push_back(*pkt);
	    Mem_msg* msg = (Mem_msg*)(pkt->data);
	    if(msg->op_type == OpMemLd)
		Send(OUT, pkt);
	    else
	        delete pkt;
	}
	else if(pkt->type == L2_cache :: COH_MSG) {
	    m_cache_resps.push_back(*pkt);
	    delete pkt;
	}
	else {
	    assert(pkt->type == L2_cache :: CREDIT_MSG);
	    delete pkt;
	}
       // m_ticks.push_back(Manifold :: NowTicks());
    }
    vector<NetworkPacket>& get_cache_resps() { return m_cache_resps; }
    vector<NetworkPacket>& get_mem_reqs() { return m_mem_reqs; }
    //vector<Ticks_t>& get_ticks() { return m_ticks; }
private:
    vector<NetworkPacket> m_cache_resps;
    vector<NetworkPacket> m_mem_reqs;
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


const int NODE_ID = 7;
const int MC_ID = 8;

//####################################################################
//! Class MESI_L2_cacheTest is the test class for class MESI_L2_cache. 
//####################################################################
class MESI_L2_cacheTest : public CppUnit::TestFixture {
private:
    static const int HT_SIZE = 0x1 << 14; //2^14 = 16k;
    static const int HT_ASSOC = 4;
    static const int HT_BLOCK_SIZE = 32;
    static const int HT_HIT = 2;
    static const int HT_LOOKUP = 11;

    static const unsigned MSHR_SIZE = 8;

    //latencies
    static const Ticks_t L1_L2 = 1;

    MESI_L2_cache* m_cachep;
    MockL1* m_l1p;

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

        const int COH = 123;
        const int MEM = 456;
        const int CREDIT = 789;
	L2_cache :: Set_msg_types(COH, MEM, CREDIT);

	L2_cache_settings settings;
	settings.mc_map = new MyMcMap1(MC_ID); //all addresses map to mc MC_ID
	settings.mshr_sz = MSHR_SIZE;
	settings.downstream_credits = 30;
	CompId_t cacheId = Component :: Create<MESI_L2_cache>(0, NODE_ID, parameters, settings);
	m_cachep = Component::GetComponent<MESI_L2_cache>(cacheId);

        //connect the components
	//L1 to L2
	Manifold::Connect(l1Id, MockL1::OUT, &MockL1::handle_incoming,
	                  cacheId, L2_cache::PORT_L1, &L2_cache::handle_incoming<Mem_msg>,
			  MasterClock, MasterClock, L1_L2, L1_L2);
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
	int SOURCE_ID ;
	while((SOURCE_ID = random() % 1024) == NODE_ID); //ensure SOURCE_ID != NODE_ID

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	Manifold::StopAt(When + L1_L2 + 2*HT_LOOKUP + 10);
	Manifold::Run();

        //verify the mock object receives a memory request
        vector<NetworkPacket>& mreqs = m_l1p->get_mem_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)mreqs.size());
	NetworkPacket& pkt = mreqs[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Mem_msg), pkt.data_size);

	Mem_msg& mreq = *((Mem_msg*)(mreqs[0].data));

	CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);

        //verify the mock object receives a MESI_MC_GRANT_E_DATA
        vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);


	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);


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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

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


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify the mock object receives a MESI_MC_FWD_S request.
        vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);


	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);


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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

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


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify the mock object receives a MESI_MC_GRAND_S_DATA request.
        vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);


	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_S_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);


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
	const int SOURCE_ID = random();

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	Manifold::StopAt(When + L1_L2 + 2*HT_LOOKUP + 10);
	Manifold::Run();

        //verify the mock object receives a memory request
        vector<NetworkPacket>& mreqs = m_l1p->get_mem_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)mreqs.size());
	NetworkPacket& pkt = mreqs[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Mem_msg), pkt.data_size);


	Mem_msg& mreq = *((Mem_msg*)(mreqs[0].data));

	CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);

        //verify the mock object receives a MESI_MC_GRANT_E_DATA
        vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);


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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

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


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify the mock object receives a MESI_MC_FWD_E request.
        vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);


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
    //! In a STORE miss, client sends MESI_CM_I_to_E to manager. As the manager is
    //! in S, it sends MESI_MC_DEMAND_I to all sharers; the client is set to be owner
    //! and manager enters SIE.
    //! 1. C--->M, MESI_CM_I_to_E
    //! 2. M--->all sharers, MESI_MC_DEMAND_I
    void test_process_client_request_store_l1_I_l2_S_0()
    {
	//create a STORE request
	const paddr_t ADDR = random();
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

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

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	Manifold::StopAt(When + L1_L2 + HT_LOOKUP + 10);
	Manifold::Run();


        //verify the mock object receives a MESI_MC_DEMAND_I request.
        vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS, (int)cresps.size());


	list<int> dest_ids(NUM_SHARERS);
	for(int i=0; i<NUM_SHARERS; i++) {
	    NetworkPacket& pkt =  cresps[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);
	    Coh_msg& cresp = *((Coh_msg*)(pkt.data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
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
	const int SOURCE_ID = random();

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + 2*HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_UNBLOCK_E;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();

        //verify the mock object receives a memory request
	{
	    vector<NetworkPacket>& mreqs = m_l1p->get_mem_reqs();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mreqs.size());
	    NetworkPacket& pkt = mreqs[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Mem_msg), pkt.data_size);

	    Mem_msg& mreq = *((Mem_msg*)(mreqs[0].data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);
	}

        //verify the mock object receives a MESI_MC_GRANT_E_DATA
	{
	    vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	    NetworkPacket& pkt =  cresps[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	    Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
	}


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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

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


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_CLEAN;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = ADDR;
	req3.msg = MESI_CM_UNBLOCK_S;
	req3.src_id = SOURCE_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_FWD_S which was from manager to owner
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);

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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

	//manually put the ADDR in the hash table, and set the manager state to S.
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
	//verify client is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(SOURCE_ID));
	MESI_manager_state_t old_state = manager->state;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_WRITEBACK;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = ADDR;
	req3.msg = MESI_CM_UNBLOCK_S;
	req3.src_id = SOURCE_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_FWD_S which was from manager to owner
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);

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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

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

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_UNBLOCK_S;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives a MESI_MC_GRANT_S_DATA request.
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_S_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);


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
	const int SOURCE_ID = random();

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + 2*HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_UNBLOCK_E;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();


        //verify the mock object receives a memory request
	{
	    vector<NetworkPacket>& mreqs = m_l1p->get_mem_reqs();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mreqs.size());
	    NetworkPacket& pkt = mreqs[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Mem_msg), pkt.data_size);

	    Mem_msg& mreq = *((Mem_msg*)(mreqs[0].data));

	    CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);
	}

        //verify the mock object receives a MESI_MC_GRANT_E_DATA
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);


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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

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

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_UNBLOCK_E;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives a MESI_MC_FWD_E request.
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);


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
	//create a STORE request
	const paddr_t ADDR = random();
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

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


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=0; i<NUM_SHARERS; i++) {
	    Coh_msg req2;
	    req2.type = Coh_msg :: COH_RPLY;
	    req2.addr = ADDR;
	    req2.msg = MESI_CM_UNBLOCK_I;
	    req2.src_id = SHARERS_ID[i];

	    NetworkPacket* l1pkt2 = new NetworkPacket;
	    l1pkt2->type = L2_cache :: COH_MSG;
	    l1pkt2->src = SOURCE_ID;
	    *((Coh_msg*)(l1pkt2->data)) = req2;
	    l1pkt2->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_UNBLOCK_E;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives  MESI_MC_DEMAND_I requests.
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+1, (int)cresps.size());

	list<int> dest_ids(NUM_SHARERS);
	for(int i=0; i<NUM_SHARERS; i++) {
	    NetworkPacket& pkt =  cresps[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	    Coh_msg& cresp = *((Coh_msg*)(cresps[i].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    dest_ids.push_back(cresp.dst_id);
	}

	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

        //verify the mock object receives  MESI_MC_GRANT_E_DATA request.
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, ((Coh_msg*)cresps[NUM_SHARERS].data)->msg);


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
	//create a STORE request
	const paddr_t ADDR = random();
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

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
	    do {
		SHARERS_ID[i] = random() % 1024;
	    } while(SHARERS_ID[i] == SOURCE_ID || SHARERS_ID[i] == NODE_ID);
//cout << "id= " << SHARERS_ID[i] << endl;
	    manager->sharersList.set(SHARERS_ID[i]);
	}
	//add requestor to sharersList.
	manager->sharersList.set(SOURCE_ID);
	SHARERS_ID[0] = SOURCE_ID; //requestor is a sharer.


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=1; i<NUM_SHARERS; i++) { //the first is requestor; it should not reply.
	    Coh_msg req2;
	    req2.type = Coh_msg :: COH_RPLY;
	    req2.addr = ADDR;
	    req2.msg = MESI_CM_UNBLOCK_I;
	    req2.src_id = SHARERS_ID[i];

	    NetworkPacket* l1pkt2 = new NetworkPacket;
	    l1pkt2->type = L2_cache :: COH_MSG;
	    l1pkt2->src = SOURCE_ID;
	    *((Coh_msg*)(l1pkt2->data)) = req2;
	    l1pkt2->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_UNBLOCK_E;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives  MESI_MC_DEMAND_I requests.
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS-1+1, (int)cresps.size());

	list<int> dest_ids(NUM_SHARERS);
	for(int i=0; i<NUM_SHARERS-1; i++) {
	    NetworkPacket& pkt =  cresps[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	    Coh_msg& cresp = *((Coh_msg*)(cresps[i].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    dest_ids.push_back(cresp.dst_id);
	}

	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=1; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

        //verify the mock object receives  MESI_MC_GRANT_E_DATA request.
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, ((Coh_msg*)cresps[NUM_SHARERS-1].data)->msg);


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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_E_to_I;
	req.src_id = SOURCE_ID;
	req.rw = 0;

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


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_UNBLOCK_I;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_GRANT_I which was from manager to owner
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_I, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);

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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_M_to_I;
	req.src_id = SOURCE_ID;
	req.rw = 0;

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


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = ADDR;
	req2.msg = MESI_CM_UNBLOCK_I;
	req2.src_id = SOURCE_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_GRANT_I which was from manager to owner
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_I, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);

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

	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.src_id = SOURCE_ID;
	req.rw = 0;

	//manually put victim's state to E.
	hash_entry* victim_hash_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	victim_hash_entry->have_data = true;
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[victim_hash_entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = m_cachep->my_table->get_line_addr(VICTIM_ADDR);
	req2.msg = MESI_CM_UNBLOCK_I;
	req2.src_id = OWNER_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	//after the eviction, L2 processes the original request: a mem read and then send MC_GRANT_E_DATA
	When += L1_L2 + 2*HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_DEMAND_I which was from manager to owner
	{
	    vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size()); //MC_DEMAND_I, MC_GRANT_E_DATA
	    NetworkPacket& pkt =  cresps[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	    Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	}

        //verify the mock object receives a memory request
	{
	    vector<NetworkPacket>& mreqs = m_l1p->get_mem_reqs();
	    CPPUNIT_ASSERT_EQUAL(1, (int)mreqs.size());
	    NetworkPacket& pkt = mreqs[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Mem_msg), pkt.data_size);

	    Mem_msg& mreq = *((Mem_msg*)(mreqs[0].data));

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

	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.src_id = SOURCE_ID;
	req.rw = 0;

	//manually put victim's state to E.
	hash_entry* victim_hash_entry = m_cachep->my_table->get_entry(VICTIM_ADDR);
	victim_hash_entry->have_data = true;
	MESI_manager* manager = dynamic_cast<MESI_manager*>(m_cachep->managers[victim_hash_entry->get_idx()]);
	manager->state = MESI_MNG_E;
	const int OWNER_ID = random() % 1024;
	manager->owner = OWNER_ID;
	//verify owner is not in sharers list.
	CPPUNIT_ASSERT_EQUAL(false, manager->sharersList.get(OWNER_ID));
	MESI_manager_state_t old_state = manager->state;


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req2;
	req2.type = Coh_msg :: COH_RPLY;
	req2.addr = m_cachep->my_table->get_line_addr(VICTIM_ADDR);
	req2.msg = MESI_CM_UNBLOCK_I_DIRTY;
	req2.src_id = OWNER_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	//after eviction, 2 mem ops and MC_GRANT_E_DATA
	When += L1_L2 + 3*HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_DEMAND_I which was from manager to owner
	{
	    vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	    CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size()); //MC_DEMAND_I, MC_GRANT_E_DATA
	    NetworkPacket& pkt =  cresps[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	    Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));


	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	}

        //verify the mock object receives a memory request
	{
	    vector<NetworkPacket>& mreqs = m_l1p->get_mem_reqs();
	    CPPUNIT_ASSERT_EQUAL(2, (int)mreqs.size()); //write back of evicted L2 line, read for new LOAD request
	    NetworkPacket& pkt = mreqs[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Mem_msg), pkt.data_size);

	    Mem_msg& mreq = *((Mem_msg*)(mreqs[0].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), mreq.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemSt, mreq.op_type);

	    Mem_msg& mreq1 = *((Mem_msg*)(mreqs[1].data));
	    CPPUNIT_ASSERT_EQUAL(ADDR, mreq1.addr);
	    CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq1.op_type);
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

	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.src_id = SOURCE_ID;
	req.rw = 0;

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
	    do {
		SHARERS_ID[i] = random() % 1024;
	    } while(SHARERS_ID[i] == NODE_ID || SHARERS_ID[i] == SOURCE_ID);
	    manager->sharersList.set(SHARERS_ID[i]);
	}


	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache_req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=0; i<NUM_SHARERS; i++) {
	    Coh_msg req2;
	    req2.type = Coh_msg :: COH_RPLY;
	    req2.addr = m_cachep->my_table->get_line_addr(VICTIM_ADDR);
	    req2.msg = MESI_CM_UNBLOCK_I;
	    req2.src_id = SHARERS_ID[i];

	    NetworkPacket* l1pkt2 = new NetworkPacket;
	    l1pkt2->type = L2_cache :: COH_MSG;
	    l1pkt2->src = SOURCE_ID;
	    *((Coh_msg*)(l1pkt2->data)) = req2;
	    l1pkt2->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	}
	When += L1_L2 + 2*HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives MESI_MC_DEMAND_I
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+1, (int)cresps.size()); // N  MC_DEMAND_I, MC_GRANT_E_DATA

	list<int> dest_ids(NUM_SHARERS);
	for(int i=0; i<NUM_SHARERS; i++) {
	    NetworkPacket& pkt =  cresps[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	    Coh_msg& cresp = *((Coh_msg*)(cresps[i].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    dest_ids.push_back(cresp.dst_id);
	}
	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

	Coh_msg& cresp = *((Coh_msg*)(cresps[NUM_SHARERS].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);

        //verify the mock object receives a memory request
	vector<NetworkPacket>& mreqs = m_l1p->get_mem_reqs();
	CPPUNIT_ASSERT_EQUAL(1, (int)mreqs.size());
	NetworkPacket& pkt = mreqs[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: MEM_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(MC_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Mem_msg), pkt.data_size);

	Mem_msg& mreq = *((Mem_msg*)(mreqs[0].data));

	CPPUNIT_ASSERT_EQUAL(ADDR, mreq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, mreq.op_type);


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
	    Coh_msg req;
	    req.type = Coh_msg :: COH_REQ;
	    req.addr = addrs[i];
	    req.msg = MESI_CM_I_to_S;

	    NetworkPacket* l1pkt = new NetworkPacket;
	    l1pkt->type = L2_cache :: COH_MSG;
	    *((Coh_msg*)(l1pkt->data)) = req;
	    l1pkt->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule for the MockProc to send the cache_req
	//This request would MSHR_STALL
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.src_id = SOURCE_ID;
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
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
	    Coh_msg req;
	    req.type = Coh_msg :: COH_REQ;
	    req.addr = addrs[i];
	    req.msg = MESI_CM_I_to_S;

	    NetworkPacket* l1pkt = new NetworkPacket;
	    l1pkt->type = L2_cache :: COH_MSG;
	    *((Coh_msg*)(l1pkt->data)) = req;
	    l1pkt->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule for the MockProc to send the cache_req
	//This request would MSHR_STALL
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.src_id = SOURCE_ID;
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a reply to one of the requests to free an mshr
	Coh_msg reply;
	reply.type = Coh_msg :: COH_RPLY;
	const paddr_t REPLY_ADDR = addrs[random() % NUM_TAGS];
	reply.addr = REPLY_ADDR;
	reply.msg = MESI_CM_UNBLOCK_E;

	NetworkPacket* l1rply = new NetworkPacket;
	l1rply->type = L2_cache :: COH_MSG;
	l1rply->src = SOURCE_ID;
	*((Coh_msg*)(l1rply->data)) = reply;
	l1rply->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1rply);
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
	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	//l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request for the same line
	//This request would PREV_PEND_STALL
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == ADDR);

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), m_cachep->my_table->get_line_addr(ADDR2));

	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR2;
	req2.msg = MESI_CM_I_to_S; //a load miss
	req2.rw = 0;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	//l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
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
	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	//l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request for the same line
	//This request would PREV_PEND_STALL
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == ADDR);

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), m_cachep->my_table->get_line_addr(ADDR2));

	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR2;
	req2.msg = MESI_CM_I_to_S; //a load miss
	req2.rw = 0;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	//l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a reply to allow the 1st request to finish.
	Coh_msg reply;
	reply.type = Coh_msg :: COH_RPLY;
	reply.addr = ADDR;
	reply.msg = MESI_CM_UNBLOCK_E; //a load miss

	NetworkPacket* l1rply = new NetworkPacket;
	l1rply->type = L2_cache :: COH_MSG;
	//l1rply->src = SOURCE_ID;
	*((Coh_msg*)(l1rply->data)) = reply;
	l1rply->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1rply);
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
	    Coh_msg req;
	    req.type = Coh_msg :: COH_REQ;
	    req.addr = addrs[i];
	    req.msg = MESI_CM_I_to_S; //a load miss
	    req.rw = 0;

	    NetworkPacket* l1pkt = new NetworkPacket;
	    l1pkt->type = L2_cache :: COH_MSG;
	    //l1pkt->src = SOURCE_ID;
	    *((Coh_msg*)(l1pkt->data)) = req;
	    l1pkt->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	//l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt); //this would LRU_BUSY_STALL
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
	    Coh_msg req;
	    req.type = Coh_msg :: COH_REQ;
	    req.addr = addrs[i];
	    req.msg = MESI_CM_I_to_S; //a load miss
	    req.rw = 0;

	    NetworkPacket* l1pkt = new NetworkPacket;
	    l1pkt->type = L2_cache :: COH_MSG;
	    //l1pkt->src = SOURCE_ID;
	    *((Coh_msg*)(l1pkt->data)) = req;
	    l1pkt->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request for the same set
	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	//l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt); //this would LRU_BUSY_STALL
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a response to allow the victim to complete its request and then be evicted.
	Coh_msg reply;
	reply.type = Coh_msg :: COH_RPLY;
	reply.addr = VICTIM_ADDR;
	reply.msg = MESI_CM_UNBLOCK_E;
	reply.rw = 0;

	NetworkPacket* l1rply = new NetworkPacket;
	l1rply->type = L2_cache :: COH_MSG;
	//l1rply->src = SOURCE_ID;
	*((Coh_msg*)(l1rply->data)) = reply;
	l1rply->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1rply);
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
	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	//l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request which will hit on the victim
	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(VICTIM_ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == VICTIM_ADDR);

	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR2;
	req2.msg = MESI_CM_I_to_S; //a load miss
	req2.rw = 0;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	//l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
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
	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S; //a load miss
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	//l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule another request which will hit on the victim
	//create another LOAD request with addr of same line
	paddr_t ADDR2 = m_cachep->my_table->get_line_addr(VICTIM_ADDR); //get the tag and index portion
	do {
	    ADDR2 |= random() % (0x1 << (m_cachep->my_table->get_offset_bits()));
	} while(ADDR2 == VICTIM_ADDR);

	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR2;
	req2.msg = MESI_CM_I_to_S; //a load miss
	req2.rw = 0;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	//l1pkt2->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a reply to the victim to allow the eviction to clomplete and wake up the 1st request.
	Coh_msg reply;
	reply.type = Coh_msg :: COH_RPLY;
	reply.addr = VICTIM_ADDR;
	reply.msg = MESI_CM_UNBLOCK_I;
	reply.rw = 0;

	NetworkPacket* l1reply = new NetworkPacket;
	l1reply->type = L2_cache :: COH_MSG;
	//l1ply->src = SOURCE_ID;
	*((Coh_msg*)(l1reply->data)) = reply;
	l1reply->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1reply);
	When += L1_L2 + HT_LOOKUP + 10;

	//schedule a reply to the 1st request to allow it to complete and wake up the 2nd request.
	Coh_msg reply2;
	reply2.type = Coh_msg :: COH_RPLY;
	reply2.addr = ADDR;
	reply2.msg = MESI_CM_UNBLOCK_E;
	reply2.rw = 0;

	NetworkPacket* l1reply2 = new NetworkPacket;
	l1reply2->type = L2_cache :: COH_MSG;
	//l1ply2->src = SOURCE_ID;
	*((Coh_msg*)(l1reply2->data)) = reply2;
	l1reply2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1reply2);
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

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_CM_E_to_I;
	req2.src_id = OWNER_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = OWNER_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);

	When += L1_L2 + HT_LOOKUP + 10;

	//1st requestor sends UNBLOCK_E to allow the first request to finish.
	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = ADDR;
	req3.msg = MESI_CM_UNBLOCK_E;
	req3.src_id = SOURCE_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives FWD_E, only
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);

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

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_CM_E_to_I;
	req2.src_id = OWNER_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = OWNER_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);

	When += L1_L2 + HT_LOOKUP + 10;

	//owner sends CC_E_DATA to allow the first request to finish.
	//This causes the requetor to send CM_UNBLOCK_E to manager.
	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = ADDR;
	req3.msg = MESI_CM_UNBLOCK_E;
	req3.src_id = SOURCE_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives FWD_S, only
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);

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

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_CM_M_to_I;
	req2.src_id = OWNER_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = OWNER_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);

	When += L1_L2 + HT_LOOKUP + 10;

	//1st requestor sends UNBLOCK_E to allow the first request to finish.
	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = ADDR;
	req3.msg = MESI_CM_UNBLOCK_E;
	req3.src_id = SOURCE_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives FWD_E, only
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);

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

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_CM_M_to_I;
	req2.src_id = OWNER_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = OWNER_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);

	When += L1_L2 + HT_LOOKUP + 10;

	//owner sends CC_M_DATA to requestor to allow the first request to finish.
	//This causes requestor to send CM_UNBLOCK_E to manager.
	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = ADDR;
	req3.msg = MESI_CM_UNBLOCK_E;
	req3.src_id = SOURCE_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);


	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives FWD_S, only
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	NetworkPacket& pkt =  cresps[0];
	CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_S, cresp.msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);

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

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would TRANS_STALL.
	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = VICTIM_ADDR;
	req2.msg = MESI_CM_E_to_I;
	req2.src_id = OWNER_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = OWNER_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);

	When += L1_L2 + HT_LOOKUP + 10;

	//owner sends UNBLOCK_I to allow eviction to finish.
	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = VICTIM_ADDR;
	req3.msg = MESI_CM_UNBLOCK_I;
	req3.src_id = OWNER_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = OWNER_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);

	When += L1_L2 + HT_LOOKUP + 10;

	//Now first request is processed.
	//1st requestor sends UNBLOCK_E to allow 1st request to finish.
	Coh_msg req4;
	req4.type = Coh_msg :: COH_RPLY;
	req4.addr = ADDR;
	req4.msg = MESI_CM_UNBLOCK_E;
	req4.src_id = SOURCE_ID;

	NetworkPacket* l1pkt4 = new NetworkPacket;
	l1pkt4->type = L2_cache :: COH_MSG;
	l1pkt4->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt4->data)) = req4;
	l1pkt4->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt4);

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives DEMAND_I for owner, and GRANT_E for 1st requestor.
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());
	{
	    NetworkPacket& pkt =  cresps[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);
	}

	{
	    NetworkPacket& pkt =  cresps[1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);
	}

	{
	    Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	}

	{
	    Coh_msg& cresp = *((Coh_msg*)(cresps[1].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
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
    //! 5. Owner--->M, MESI_CM_UNBLOCK_I.
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

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_S;
	req.src_id = SOURCE_ID;
	req.rw = 0;

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	//schedule for the MockProc to send the req
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);

	When += L1_L2 + HT_LOOKUP + 10;

	//create a Invalidation request from owner; this would PREV_PEND_STALL.
	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = VICTIM_ADDR;
	req2.msg = MESI_CM_M_to_I;
	req2.src_id = OWNER_ID;

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = OWNER_ID;
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);

	When += L1_L2 + HT_LOOKUP + 10;

	//owner sends UNBLOCK_I_DIRTY to allow eviction to finish.
	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = VICTIM_ADDR;
	req3.msg = MESI_CM_UNBLOCK_I_DIRTY;
	req3.src_id = OWNER_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = OWNER_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);

	When += L1_L2 + HT_LOOKUP + 10;

	//Now first request is processed.
	//1st requestor sends UNBLOCK_E to allow 1st request to finish.
	Coh_msg req4;
	req4.type = Coh_msg :: COH_RPLY;
	req4.addr = ADDR;
	req4.msg = MESI_CM_UNBLOCK_E;
	req4.src_id = SOURCE_ID;

	NetworkPacket* l1pkt4 = new NetworkPacket;
	l1pkt4->type = L2_cache :: COH_MSG;
	l1pkt4->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt4->data)) = req4;
	l1pkt4->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt4);

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();



        //verify the mock object receives DEMAND_I for owner, and GRANT_E for 1st requestor.
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());

	{
	    NetworkPacket& pkt =  cresps[0];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);
	}

	{
	    NetworkPacket& pkt =  cresps[1];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, pkt.dst);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);
	}

	{
	    Coh_msg& cresp = *((Coh_msg*)(cresps[0].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(VICTIM_ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(OWNER_ID, cresp.dst_id);
	}

	{
	    Coh_msg& cresp = *((Coh_msg*)(cresps[1].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, cresp.msg);
	    CPPUNIT_ASSERT_EQUAL(SOURCE_ID, cresp.dst_id);
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
    //! @brief Test request race: DEMAND_I and I_to_E (I_to_E is actually means S_to_E).
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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

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

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	//schedule for requestor to send 
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	//sharer 1 sends I_to_E
	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_CM_I_to_E;
	req2.src_id = SHARERS_ID[0];

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SHARERS_ID[0];
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);

	When += L1_L2 + 10;

	for(int i=0; i<NUM_SHARERS; i++) { //all sharers reply
	    Coh_msg req2;
	    req2.type = Coh_msg :: COH_RPLY;
	    req2.addr = ADDR;
	    req2.msg = MESI_CM_UNBLOCK_I;
	    req2.src_id = SHARERS_ID[i];

	    NetworkPacket* l1pkt2 = new NetworkPacket;
	    l1pkt2->type = L2_cache :: COH_MSG;
	    l1pkt2->src = SHARERS_ID[i];
	    *((Coh_msg*)(l1pkt2->data)) = req2;
	    l1pkt2->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//requestor reply with UNBLOCK_E
	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = ADDR;
	req3.msg = MESI_CM_UNBLOCK_E;
	req3.src_id = SOURCE_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);

	When += L1_L2 + HT_LOOKUP + 10;

	//sharer 1 reply with UNBLOCK_E
	Coh_msg req4;
	req4.type = Coh_msg :: COH_RPLY;
	req4.addr = ADDR;
	req4.msg = MESI_CM_UNBLOCK_E;
	req4.src_id = SOURCE_ID;

	NetworkPacket* l1pkt4 = new NetworkPacket;
	l1pkt4->type = L2_cache :: COH_MSG;
	l1pkt4->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt4->data)) = req4;
	l1pkt4->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt4);

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives  MESI_MC_DEMAND_I requests.
	vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2, (int)cresps.size());
	list<int> dest_ids(NUM_SHARERS);
	for(int i=0; i<NUM_SHARERS; i++) {
	    NetworkPacket pkt =  cresps[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	    Coh_msg cresp = *((Coh_msg*)(cresps[i].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    dest_ids.push_back(cresp.dst_id);
	}

	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

        //verify the mock object receives GRANT_E_DATA and FWD_E
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, ((Coh_msg*)cresps[NUM_SHARERS].data)->msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, ((Coh_msg*)cresps[NUM_SHARERS].data)->dst_id);

	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, ((Coh_msg*)cresps[NUM_SHARERS+1].data)->msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, ((Coh_msg*)cresps[NUM_SHARERS+1].data)->dst_id);


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
	const int SOURCE_ID = random() % 1024;

	Coh_msg req;
	req.type = Coh_msg :: COH_REQ;
	req.addr = ADDR;
	req.msg = MESI_CM_I_to_E;
	req.src_id = SOURCE_ID;
	req.rw = 1;

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

	NetworkPacket* l1pkt = new NetworkPacket;
	l1pkt->type = L2_cache :: COH_MSG;
	l1pkt->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt->data)) = req;
	l1pkt->data_size = sizeof(Coh_msg);

	//schedule for requestor to send 
	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt);
	When += L1_L2 + HT_LOOKUP + 10;

	//sharer 1 sends I_to_E
	Coh_msg req2;
	req2.type = Coh_msg :: COH_REQ;
	req2.addr = ADDR;
	req2.msg = MESI_CM_I_to_E;
	req2.src_id = SHARERS_ID[0];

	NetworkPacket* l1pkt2 = new NetworkPacket;
	l1pkt2->type = L2_cache :: COH_MSG;
	l1pkt2->src = SHARERS_ID[0];
	*((Coh_msg*)(l1pkt2->data)) = req2;
	l1pkt2->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);

	When += L1_L2 + HT_LOOKUP + 10;

	for(int i=0; i<NUM_SHARERS; i++) { //all sharers reply
	    Coh_msg req2;
	    req2.type = Coh_msg :: COH_RPLY;
	    req2.addr = ADDR;
	    req2.msg = MESI_CM_UNBLOCK_I;
	    req2.src_id = SHARERS_ID[i];

	    NetworkPacket* l1pkt2 = new NetworkPacket;
	    l1pkt2->type = L2_cache :: COH_MSG;
	    l1pkt2->src = SHARERS_ID[0];
	    *((Coh_msg*)(l1pkt2->data)) = req2;
	    l1pkt2->data_size = sizeof(Coh_msg);

	    Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt2);
	}
	When += L1_L2 + HT_LOOKUP + 10;

	//requestor reply with UNBLOCK_E
	Coh_msg req3;
	req3.type = Coh_msg :: COH_RPLY;
	req3.addr = ADDR;
	req3.msg = MESI_CM_UNBLOCK_E;
	req3.src_id = SOURCE_ID;

	NetworkPacket* l1pkt3 = new NetworkPacket;
	l1pkt3->type = L2_cache :: COH_MSG;
	l1pkt3->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt3->data)) = req3;
	l1pkt3->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt3);

	When += L1_L2 + HT_LOOKUP + 10;

	//sharer 1 reply with UNBLOCK_E
	Coh_msg req4;
	req4.type = Coh_msg :: COH_RPLY;
	req4.addr = ADDR;
	req4.msg = MESI_CM_UNBLOCK_E;
	req4.src_id = SOURCE_ID;

	NetworkPacket* l1pkt4 = new NetworkPacket;
	l1pkt4->type = L2_cache :: COH_MSG;
	l1pkt4->src = SOURCE_ID;
	*((Coh_msg*)(l1pkt4->data)) = req4;
	l1pkt4->data_size = sizeof(Coh_msg);

	Manifold::Schedule(When, &MockL1::send_req, m_l1p, l1pkt4);

	When += L1_L2 + HT_LOOKUP + 10;


	Manifold::StopAt(When);
	Manifold::Run();




        //verify the mock object receives  MESI_MC_DEMAND_I requests.
        vector<NetworkPacket>& cresps = m_l1p->get_cache_resps();
	CPPUNIT_ASSERT_EQUAL(NUM_SHARERS+2, (int)cresps.size());
	list<int> dest_ids(NUM_SHARERS);
	for(int i=0; i<NUM_SHARERS; i++) {
	    NetworkPacket pkt =  cresps[i];
	    CPPUNIT_ASSERT_EQUAL(L2_cache :: COH_MSG, pkt.type);
	    CPPUNIT_ASSERT_EQUAL(NODE_ID, pkt.src);
	    CPPUNIT_ASSERT_EQUAL((int)sizeof(Coh_msg), pkt.data_size);

	    Coh_msg cresp = *((Coh_msg*)(cresps[i].data));

	    CPPUNIT_ASSERT_EQUAL(m_cachep->my_table->get_line_addr(ADDR), cresp.addr);
	    CPPUNIT_ASSERT_EQUAL((int)MESI_MC_DEMAND_I, cresp.msg);
	    dest_ids.push_back(cresp.dst_id);
	}

	//verify all SHARERS_ID can be found in the dest_ids.
	for(int i=0; i<NUM_SHARERS; i++) {
	    CPPUNIT_ASSERT(find(dest_ids.begin(), dest_ids.end(), SHARERS_ID[i]) != dest_ids.end());
	}

        //verify the mock object receives GRANT_E_DATA and FWD_E
	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_GRANT_E_DATA, ((Coh_msg*)cresps[NUM_SHARERS].data)->msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, ((Coh_msg*)cresps[NUM_SHARERS].data)->dst_id);

	CPPUNIT_ASSERT_EQUAL((int)MESI_MC_FWD_E, ((Coh_msg*)cresps[NUM_SHARERS+1].data)->msg);
	CPPUNIT_ASSERT_EQUAL(SOURCE_ID, ((Coh_msg*)cresps[NUM_SHARERS+1].data)->dst_id);

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
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MESI_L2_cacheTest");
	/*
	*/
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_load_l1_I_l2_I_0", &MESI_L2_cacheTest::test_process_client_request_load_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_load_l1_I_l2_E_0", &MESI_L2_cacheTest::test_process_client_request_load_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_load_l1_I_l2_S_0", &MESI_L2_cacheTest::test_process_client_request_load_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_store_l1_I_l2_I_0", &MESI_L2_cacheTest::test_process_client_request_store_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_store_l1_I_l2_E_0", &MESI_L2_cacheTest::test_process_client_request_store_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_store_l1_I_l2_S_0", &MESI_L2_cacheTest::test_process_client_request_store_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_I_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_E_owner_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_load_l1_I_l2_S_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_load_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_I_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_E_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_store_l1_I_l2_S_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_store_l1_I_l2_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_store_hit_l1_S_l2_S_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_store_hit_l1_S_l2_S_0));
	//eviction of client line
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_client_eviction_victim_E_l2_E_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_client_eviction_victim_E_l2_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_client_eviction_victim_M_l2_E_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_client_eviction_victim_M_l2_E_0));
	//eviction of L2 line
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_l2_eviction_victim_E_l1_E_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_l2_eviction_victim_E_l1_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_l2_eviction_victim_E_l1_M_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_l2_eviction_victim_E_l1_M_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_l2_eviction_victim_E_l1_S_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_l2_eviction_victim_E_l1_S_0));
	//MSHR_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_MSHR_STALL_0", &MESI_L2_cacheTest::test_process_client_request_MSHR_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_MSHR_STALL_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_MSHR_STALL_0));
	//PREV_PEND_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_PREV_PEND_STALL_0", &MESI_L2_cacheTest::test_process_client_request_PREV_PEND_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_PREV_PEND_STALL_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_PREV_PEND_STALL_0));
	//LRU_BUSY_STALL
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_LRU_BUSY_STALL_0", &MESI_L2_cacheTest::test_process_client_request_LRU_BUSY_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_LRU_BUSY_STALL_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_LRU_BUSY_STALL_0));
	//TRANS_STALL, e.g., the hit entry is being evicted
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_TRANS_STALL_0", &MESI_L2_cacheTest::test_process_client_request_TRANS_STALL_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_process_client_request_and_reply_TRANS_STALL_0", &MESI_L2_cacheTest::test_process_client_request_and_reply_TRANS_STALL_0));
	//Request race
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_request_race_l1_E_to_I_l2_FWD_E_0", &MESI_L2_cacheTest::test_request_race_l1_E_to_I_l2_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_request_race_l1_E_to_I_l2_FWD_S_0", &MESI_L2_cacheTest::test_request_race_l1_E_to_I_l2_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_request_race_l1_M_to_I_l2_FWD_E_0", &MESI_L2_cacheTest::test_request_race_l1_M_to_I_l2_FWD_E_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_request_race_l1_M_to_I_l2_FWD_S_0", &MESI_L2_cacheTest::test_request_race_l1_M_to_I_l2_FWD_S_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_request_race_l1_E_to_I_l2_DEMAND_I_0", &MESI_L2_cacheTest::test_request_race_l1_E_to_I_l2_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_request_race_l1_M_to_I_l2_DEMAND_I_0", &MESI_L2_cacheTest::test_request_race_l1_M_to_I_l2_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_request_race_l1_I_to_E_l2_DEMAND_I_0", &MESI_L2_cacheTest::test_request_race_l1_I_to_E_l2_DEMAND_I_0));
	mySuite->addTest(new CppUnit::TestCaller<MESI_L2_cacheTest>("test_request_race_l1_I_to_E_in_IE_l2_DEMAND_I_0", &MESI_L2_cacheTest::test_request_race_l1_I_to_E_in_IE_l2_DEMAND_I_0));
	/*
	*/



	return mySuite;
    }
};


Clock MESI_L2_cacheTest :: MasterClock(MESI_L2_cacheTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MESI_L2_cacheTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


