//! This program tests simple_cache as a component. We use two Mock
//! components and connect the simple_cache to the components.
//! The simple_cache is an L1 but NOT last-level cache.
//!
//!                      --------
//!                     | source | processor
//!                      --------
//!                         |
//!                  --------------
//!                 | simple_cache |  L1
//!                  --------------
//!                         |
//!                       ------
//!                      | sink | L2
//!                       ------

#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <vector>
#include <stdlib.h>
#include "simple_cache.h"

#include "kernel/manifold-decl.h"
#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace std;
using namespace manifold::simple_cache;
using namespace manifold::kernel;
using namespace manifold::uarch;

//####################################################################
// helper classes
//####################################################################

struct CacheReq {
    CacheReq(uint64_t a, bool r) : addr(a), read(r) {}

    uint64_t get_addr() { return addr; }
    bool is_read() { return read; }

    uint64_t addr;
    bool read;
};


//! This class plays the role of a processor
class Source : public manifold::kernel::Component {
public:
    void send_creq(CacheReq* creq)
    {
	Send(0, creq);
    }
    void handle_incoming(int, CacheReq* cresp)
    {
    //cout << "Source handle_incoming @ " << Manifold::NowTicks() << endl;
        m_cache_resps.push_back(*cresp);
        m_ticks.push_back(Manifold :: NowTicks());
    }
    vector<CacheReq>& get_cache_resps() { return m_cache_resps; }
    vector<Ticks_t>& get_ticks() { return m_ticks; }
private:
    vector<CacheReq> m_cache_resps;
    vector<Ticks_t> m_ticks;
};



//! This class plays the role of L2 cache.
class Sink : public manifold::kernel::Component {
public:
    static const int LATENCY = 7;

    Sink() : m_send_reply(false) {}

    void handle_incoming_req(int, Cache_req* req)
    {
    //cout << "Sink called at " << Manifold :: NowTicks() << endl;
        m_reqs.push_back(*req);
        m_ticks.push_back(Manifold :: NowTicks());

	if(m_send_reply)
	    if(req->op_type == OpMemLd) //no response for write
		SendTick(0, req, LATENCY);
	else
	    delete req;
    }


    void set_send_reply(bool s)
    {
        m_send_reply = s;
    }

    vector<Cache_req>& get_reqs() { return m_reqs; }
    vector<Ticks_t>& get_ticks() { return m_ticks; }

private:
    bool m_send_reply;

    //Save the requests and timestamps for verification.
    vector<Cache_req> m_reqs;
    vector<Ticks_t> m_ticks;
};


//! This mapping maps any address to the same id, which is initialized
//! in construction.
class MyDestMap1 : public DestMap {
public:
    MyDestMap1(int x) : m_id(x) {}
    int lookup(paddr_t addr) { return m_id; }
private:
    const int m_id;
};


//####################################################################
//####################################################################
class simple_cacheTest : public CppUnit::TestFixture {
private:
    static const int HT_SIZE = 0x1 << 14; //2^14 = 16k;
    static const int HT_ASSOC = 4;
    static const int HT_BLOCK_SIZE = 32;
    static const int HT_HIT = 2;
    static const int HT_LOOKUP = 11;

    Simple_cache_parameters m_parameters;
    Source* m_sourcep;
    Sink* m_sinkp;
    simple_cache* m_cachep;

    static const int SRC_CACHE = 1;
    static const int CACHE_SINK = 1;

public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    void setUp()
    {
	m_parameters.size = HT_SIZE;
	m_parameters.assoc = HT_ASSOC;
	m_parameters.block_size = HT_BLOCK_SIZE;
	m_parameters.hit_time = HT_HIT;
	m_parameters.lookup_time = HT_LOOKUP;

        //create a Source, a simple_cache, and a Sink
	CompId_t srcId = Component :: Create<Source>(0);
	m_sourcep = Component::GetComponent<Source>(srcId);

	MyDestMap1* destmap = new MyDestMap1(0); //all addresses map to node 0
	Simple_cache_init init;
	init.dmap = destmap;
	init.first_level = true;
	init.last_level = false;
	CompId_t cacheId = Component :: Create<simple_cache>(0, 123, "test", m_parameters, init);
	CompId_t sinkId = Component :: Create<Sink>(0);
	m_cachep = Component::GetComponent<simple_cache>(cacheId);
	m_sinkp = Component::GetComponent<Sink>(sinkId);

        //connect the components
	//source to cache
	Manifold::Connect(srcId, 0, cacheId, simple_cache::PORT_LOWER,
			  &simple_cache::handle_processor_request<CacheReq>, SRC_CACHE);
	//cache to source
	Manifold::Connect(cacheId, simple_cache::PORT_LOWER, srcId, 0,
			  &Source::handle_incoming, SRC_CACHE);
	//cache to sink
	Manifold::Connect(cacheId, simple_cache::PORT_UPPER, sinkId, 0,
			  &Sink::handle_incoming_req, CACHE_SINK);
	//sink to cache
	Manifold::Connect(sinkId, 0, cacheId, simple_cache::PORT_UPPER,
			  &simple_cache::handle_cache_response, CACHE_SINK);
    }






    //======================================================================
    //======================================================================
    //! @brief test handle_processor_request(): load miss
    //!
    //!Empty cache; load with a random address; sink component
    //! should receive a mem_req with the same address.
    void test_handle_processor_request_load_miss_0()
    {
	//create a LOAD request
	paddr_t addr = random();
	CacheReq* creq = new CacheReq(addr, true);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the Source to send the cache req
	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt + SRC_CACHE+HT_LOOKUP + 100);
	Manifold::Run();


	//verify hash table
	hash_entry* hentry = m_cachep->my_table->get_entry(addr);
	CPPUNIT_ASSERT(hentry == 0); //only enter hash table after response


        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(1, int(m_cachep->m_mshr.size()));
	CPPUNIT_ASSERT_EQUAL(addr, m_cachep->m_mshr.front()->creq->addr);
	GenericPreqWrapper<CacheReq>* wrapper = dynamic_cast<GenericPreqWrapper<CacheReq>*>(m_cachep->m_mshr.front()->creq->preqWrapper);
	CPPUNIT_ASSERT(creq == wrapper->preq);

        //verify the sink gets a req.
        vector<Cache_req>& reqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();
	Cache_req req = reqs[0];

	CPPUNIT_ASSERT_EQUAL(addr, req.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, req.op_type);
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SRC_CACHE+HT_LOOKUP, ticks[0]);
    }





    //======================================================================
    //======================================================================
    //! @brief 2 loads to same address; one after another.
    //!
    //! Empty cache; load with a random address; sink
    //! should receive a req with the same address; response sent back;
    //! load same address; hit. NOTE: handle_response cannot be tested alone.
    void test_handle_processor_request_and_handle_response_load_load_hit_0()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create 2 LOAD requests for same address
	paddr_t addr = random();
	CacheReq* creq = new CacheReq(addr, true);
	CacheReq* creq1 = new CacheReq(addr, true);


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the source to send the cache req
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;
	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq1);
	Ticks_t scheduledAt1 = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt1 + rtt + 100); 
	Manifold::Run();



	//verify hash table
	hash_entry* hentry = m_cachep->my_table->get_entry(addr);
	CPPUNIT_ASSERT(hentry != 0);
	CPPUNIT_ASSERT_EQUAL(true, hentry->valid);
	CPPUNIT_ASSERT_EQUAL(false, hentry->dirty);


        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(0, int(m_cachep->m_mshr.size()));



	//verify source gets 2 responses
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        vector<Ticks_t>& pticks = m_sourcep->get_ticks();

	CPPUNIT_ASSERT_EQUAL(addr, cresps[0].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP+Sink::LATENCY+HT_HIT, pticks[0]);

	CPPUNIT_ASSERT_EQUAL(addr, cresps[1].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt1+SRC_CACHE+HT_HIT, pticks[1]);


        //verify the sink gets a req.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(1, (int)mreqs.size());
	CPPUNIT_ASSERT_EQUAL(addr, mreqs[0].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, ticks[0]);


	m_sinkp->set_send_reply(false);
    }




    //======================================================================
    //======================================================================
    //! @brief a load and a store to same address; one after another.
    //!
    //! Empty cache; load with a random address; sink
    //! should receive a mem_req with the same address; response sent back;
    //! store to same address;
    void test_handle_processor_request_and_handle_response_load_store_hit_0()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create a load and a store requests for same address
	paddr_t addr = random();
	CacheReq* creq = new CacheReq(addr, true);
	CacheReq* creq1 = new CacheReq(addr, false);


	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the source to send the cache req
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;
	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq1);
	Ticks_t scheduledAt1 = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt1 + rtt + 100); 
	Manifold::Run();



	//verify hash table
	hash_entry* hentry = m_cachep->my_table->get_entry(addr);
	CPPUNIT_ASSERT(hentry != 0);
	CPPUNIT_ASSERT_EQUAL(true, hentry->valid);
	CPPUNIT_ASSERT_EQUAL(false, hentry->dirty);


        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(0, int(m_cachep->m_mshr.size()));



	//verify source gets 2 responses
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        vector<Ticks_t>& pticks = m_sourcep->get_ticks();

	CPPUNIT_ASSERT_EQUAL(addr, cresps[0].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP+Sink::LATENCY+HT_HIT, pticks[0]);

	CPPUNIT_ASSERT_EQUAL(addr, cresps[1].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt1+SRC_CACHE+HT_HIT, pticks[1]);


        //verify the sink gets 2 req.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(2, (int)mreqs.size());
	CPPUNIT_ASSERT_EQUAL(addr, mreqs[0].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, ticks[0]);
	CPPUNIT_ASSERT_EQUAL(addr, mreqs[1].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt1+SRC_CACHE+HT_HIT, ticks[1]);


	m_sinkp->set_send_reply(false);
    }




    //======================================================================
    //======================================================================
    //! @brief load each byte of the same block; one request at a time.
    //!
    //! Empty cache; load with a random address x; then try all address
    //! y that differs from x only in the offset portion; should get a hit.
    void test_handle_processor_request_and_handle_response_load_1()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create a LOAD request
	paddr_t org_addr = random(); //original addr
	CacheReq* creq = new CacheReq(org_addr, true);

	paddr_t offset_mask = HT_BLOCK_SIZE - 1;
	offset_mask = ~offset_mask; //offset_mask is used to clear the offset portion of
				    //an address

	int num_offsets = HT_BLOCK_SIZE;
        CacheReq* creqs[num_offsets];
	paddr_t addrs[num_offsets];

	for(int i=0; i<num_offsets; i++) { //try all offsets from 0 to num_offsets-1
	    addrs[i] = (org_addr & offset_mask) | i;
	    creqs[i] = new CacheReq(addrs[i], true);
        }

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache req
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;

        Ticks_t scheduledAts[num_offsets];
	for(int i=0; i<num_offsets; i++) {
	    Manifold::Schedule(When, &Source::send_creq, m_sourcep, creqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	    When += rtt+100;
	}

	Manifold::StopAt(When + rtt + 100);
	Manifold::Run();


	//verify hash table
	hash_entry* hentry = m_cachep->my_table->get_entry(org_addr);
	CPPUNIT_ASSERT(hentry != 0);
	CPPUNIT_ASSERT_EQUAL(true, hentry->valid);
	CPPUNIT_ASSERT_EQUAL(false, hentry->dirty);


        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(0, int(m_cachep->m_mshr.size()));


	//verify source get num_offsets+1 responses
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        vector<Ticks_t>& pticks = m_sourcep->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_offsets+1, (int)cresps.size());

	CPPUNIT_ASSERT_EQUAL(org_addr, cresps[0].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP+Sink::LATENCY+HT_HIT, pticks[0]);

        for(int i=0; i<num_offsets; i++) {
	    CPPUNIT_ASSERT_EQUAL(addrs[i], cresps[i+1].addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SRC_CACHE+HT_HIT, pticks[i+1]);
	}


        //verify the sink component gets only ONE  req.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(1, (int)mreqs.size());

	CPPUNIT_ASSERT_EQUAL(org_addr, mreqs[0].addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, mreqs[0].op_type);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, ticks[0]);

	m_sinkp->set_send_reply(false);
    }




    //======================================================================
    //======================================================================
    //! @brief load address x, then load address y that differs from x in index.
    //! one request at a time.
    //!
    //! Empty cache; load with a random address x; then try all address
    //! y that differs from x only in the index portion; should get a miss.
    void test_handle_processor_request_and_handle_response_load_2()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create a LOAD request
	paddr_t org_addr = random(); //original addr
	CacheReq* creq = new CacheReq(org_addr, true);

        //compute index mask
	int offset_num_bits=0;
	int temp = HT_BLOCK_SIZE-1;
	while(temp > 0) {
	    temp >>= 1;
	    offset_num_bits++;
	}
	paddr_t index_mask = HT_SIZE/HT_BLOCK_SIZE/HT_ASSOC - 1;
	index_mask <<= offset_num_bits; //index_mask: the index portion is all 0's, and
	index_mask = ~index_mask;       //the tag and offset portions are all 1's;

	int num_of_indices = HT_SIZE/HT_BLOCK_SIZE/HT_ASSOC;

        CacheReq* creqs[num_of_indices];
	paddr_t addrs[num_of_indices];

	for(int i=0; i<num_of_indices; i++) { //try all indices
	    //one of the addrs[i] is the same as org_addr; the rest differs
	    //only in the index portion
	    addrs[i] = (org_addr & index_mask) | (i << offset_num_bits);
	    creqs[i] = new CacheReq(addrs[i], true);
        }

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache req
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;

        //schedule num_of_indices LOADS; one will hit; the rest will miss.
        Ticks_t scheduledAts[num_of_indices];
	for(int i=0; i<num_of_indices; i++) {
	    Manifold::Schedule(When, &Source::send_creq, m_sourcep, creqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	    When += rtt+100;
	}

	Manifold::StopAt(When + rtt + 100);
	Manifold::Run();



        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(0, int(m_cachep->m_mshr.size()));


	//verify source get num_of_indices+1 responses
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        vector<Ticks_t>& pticks = m_sourcep->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_of_indices+1, (int)cresps.size());

	CPPUNIT_ASSERT_EQUAL(org_addr, cresps[0].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP+Sink::LATENCY+HT_HIT, pticks[0]);

        int hits=0;
        for(int i=0; i<num_of_indices; i++) {
	    CPPUNIT_ASSERT_EQUAL(addrs[i], cresps[i+1].addr);
	    if(addrs[i] == org_addr) {
		CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SRC_CACHE+HT_HIT, pticks[i+1]);
		hits++;
	    }
            else {
		CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SRC_CACHE+HT_LOOKUP+Sink::LATENCY+HT_HIT, pticks[i+1]);
	    }
	}
	CPPUNIT_ASSERT_EQUAL(1, hits);


        //verify sink gets 1+(num_of_indices -1) reqs
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_of_indices, (int)mreqs.size());

	m_sinkp->set_send_reply(false);
    }





    //======================================================================
    //======================================================================
    //! @brief load address x, then load address y that differs from x in tag.
    //! one request at a time.
    //!
    //! Empty cache; load with a random address x; then try all address
    //! y that differs from x only in the tag portion; should get a miss.
    void test_handle_processor_request_and_handle_response_load_3()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create a LOAD request
	paddr_t org_addr = random(); //original addr
	CacheReq* creq = new CacheReq(org_addr, true);
	//cout << "org addr= " << hex << org_addr << endl;

	// | tag || index || offset |
	int index_offset_num_bits=0; //number of index and offset bits
	int temp = HT_SIZE/HT_ASSOC-1;
	while(temp > 0) {
	    temp >>= 1;
	    index_offset_num_bits++;
	}

        //number of possible tags is usually big
	paddr_t largest_tag = (~0x0);
	largest_tag >>= index_offset_num_bits;

        //since addresses with the same index and offset are put in the same set,
	//up to 200 different tags should be enough for the test purposes
	int num_tags = (200 < largest_tag) ? 200 : largest_tag; //take the minimum

        CacheReq* creqs[num_tags];
	paddr_t addrs[num_tags];

	paddr_t tag_mask = largest_tag << index_offset_num_bits; //tag portion is all 1's; rest 0's
	for(int i=0; i<num_tags; i++) {
	    set<paddr_t> addrset; //use a set to generate unique values
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
	                                           //value is already in the set
	    //make sure ad is new and different from org_addr
	    do {
		ad = org_addr & (~tag_mask); //clear the tag portion
		ad |= ((random() & largest_tag) << index_offset_num_bits); // OR-in a random tag
		if(ad != org_addr)
		    ret = addrset.insert(ad);
	    } while(ad != org_addr && ret.second == false);

	    addrs[i] = ad;
	    creqs[i] = new CacheReq(addrs[i], true);
	    //cout << "ad= " << hex << ad << endl;
        }

 
	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the first cache req with the original address
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;

        //schedule num_tags LOADS; all will miss.
        Ticks_t scheduledAts[num_tags];
	for(int i=0; i<num_tags; i++) {
	    Manifold::Schedule(When, &Source::send_creq, m_sourcep, creqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	    When += rtt+100;
	}

	Manifold::StopAt(When + rtt + 100);
	Manifold::Run();


        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(0, int(m_cachep->m_mshr.size()));


	//verify source get num_tags + 1 responses
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        vector<Ticks_t>& pticks = m_sourcep->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_tags+1, (int)cresps.size());

	CPPUNIT_ASSERT_EQUAL(org_addr, cresps[0].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP+Sink::LATENCY+HT_HIT, pticks[0]);

        for(int i=0; i<num_tags; i++) {
	    CPPUNIT_ASSERT_EQUAL(addrs[i], cresps[i+1].addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SRC_CACHE+HT_LOOKUP+Sink::LATENCY+HT_HIT, pticks[i+1]);
	}


        //verify sink gets 1+num_tags reqs
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_tags + 1, (int)mreqs.size());

	m_sinkp->set_send_reply(false);
    }





    //======================================================================
    //======================================================================
    //! @brief test handle_processor_request(): store miss
    //!
    //! Empty cache; store with a random address; sink
    //! should receive a req with the same address.
    void test_handle_processor_request_store_miss_0()
    {
	//create a STORE request
	paddr_t addr = random();
	CacheReq* creq = new CacheReq(addr, false);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the source to send the cache req
	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Manifold::StopAt(scheduledAt + SRC_CACHE+HT_LOOKUP + 100);
	Manifold::Run();



	//verify hash table
	hash_entry* hentry = m_cachep->my_table->get_entry(addr);
	CPPUNIT_ASSERT(hentry == 0);


        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(0, int(m_cachep->m_mshr.size()));


        //verify source gets a reply.
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        vector<Ticks_t>& pticks = m_sourcep->get_ticks();

	CPPUNIT_ASSERT_EQUAL(1, (int)cresps.size());
	CPPUNIT_ASSERT_EQUAL(addr, cresps[0].addr);
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SRC_CACHE+HT_LOOKUP, pticks[0]);


        //verify sink gets a req.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();
	Cache_req mreq = mreqs[0];

	CPPUNIT_ASSERT_EQUAL(addr, mreq.addr);
	CPPUNIT_ASSERT_EQUAL(OpMemSt, mreq.op_type);
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SRC_CACHE+HT_LOOKUP, ticks[0]);
    }




    //======================================================================
    //======================================================================
    //! @brief Send a store, followed by a load to the same addr; load should miss.
    //!
    //! Empty cache; store with a random address; then a load to the same addr.
    void test_handle_processor_request_and_handle_response_store_load_0()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create 2 STORE requests for same address
	paddr_t addr = random();
	CacheReq* creq = new CacheReq(addr, false);
	CacheReq* creq1 = new CacheReq(addr, true);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache req
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq1);
	Ticks_t scheduledAt1 = When + MasterClock.NowTicks();

	Manifold::StopAt(When + rtt + 100); 
	Manifold::Run();



        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(0, int(m_cachep->m_mshr.size()));

	//verify Source get 2 responses.
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        vector<Ticks_t>& pticks = m_sourcep->get_ticks();
	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());
	CPPUNIT_ASSERT_EQUAL(2, (int)pticks.size());
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, pticks[0]);
	CPPUNIT_ASSERT_EQUAL(scheduledAt1+SRC_CACHE+HT_LOOKUP+Sink::LATENCY+HT_HIT, pticks[1]);


        //verify sink gets 2 req.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(2, (int)mreqs.size());

	CPPUNIT_ASSERT_EQUAL(addr, mreqs[0].addr);
	CPPUNIT_ASSERT_EQUAL(OpMemSt, mreqs[0].op_type);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, ticks[0]);
	CPPUNIT_ASSERT_EQUAL(addr, mreqs[1].addr);
	CPPUNIT_ASSERT_EQUAL(OpMemLd, mreqs[1].op_type);
	CPPUNIT_ASSERT_EQUAL(scheduledAt1+SRC_CACHE+HT_LOOKUP, ticks[1]);

	//reset
	m_sinkp->set_send_reply(false);
    }




    //======================================================================
    //======================================================================
    //! @brief Send a store, followed by another store to the same addr. both miss.
    //!
    //! Empty cache; store with a random address; then another store to the same addr.
    void test_handle_processor_request_and_handle_response_store_store_0()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create 2 STORE requests for same address
	paddr_t addr = random();
	CacheReq* creq = new CacheReq(addr, false);
	CacheReq* creq1 = new CacheReq(addr, false);

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the MockProc to send the cache req
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq1);
	Ticks_t scheduledAt1 = When + MasterClock.NowTicks();

	Manifold::StopAt(When + rtt + 100); 
	Manifold::Run();



	//verify Source get 2 responses.
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        vector<Ticks_t>& pticks = m_sourcep->get_ticks();

	CPPUNIT_ASSERT_EQUAL(2, (int)cresps.size());

        //verify sink gets 2 req.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(2, (int)mreqs.size());

	CPPUNIT_ASSERT_EQUAL(addr, mreqs[0].addr);
	CPPUNIT_ASSERT_EQUAL(OpMemSt, mreqs[0].op_type);
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, ticks[0]);
	CPPUNIT_ASSERT_EQUAL(addr, mreqs[1].addr);
	CPPUNIT_ASSERT_EQUAL(OpMemSt, mreqs[1].op_type);
	CPPUNIT_ASSERT_EQUAL(scheduledAt1+SRC_CACHE+HT_LOOKUP, ticks[1]);

	//reset
	m_sinkp->set_send_reply(false);
    }








    //======================================================================
    //======================================================================
    //! @brief Send stores for each byte of a block.
    //!
    //! Empty cache; store with a random address x; then try all address
    //! y that differs from x only in the offset portion.
    void test_handle_processor_request_and_handle_response_store_1()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create a STORE request
	paddr_t org_addr = random();
	CacheReq* creq = new CacheReq(org_addr, false);

	paddr_t offset_mask = HT_BLOCK_SIZE - 1;
	offset_mask = ~offset_mask; //offset_mask is used to clear the offset portion of
				    //an address

	int num_offsets = HT_BLOCK_SIZE;
        CacheReq* creqs[num_offsets];
	paddr_t addrs[num_offsets];

	for(int i=0; i<num_offsets; i++) { //try all offsets from 0 to num_offsets-1
	    addrs[i] = (org_addr & offset_mask) | i;
	    creqs[i] = new CacheReq(addrs[i], false);
        }

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for source to send the cache req
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;

        Ticks_t scheduledAts[num_offsets];
	for(int i=0; i<num_offsets; i++) {
	    Manifold::Schedule(When, &Source::send_creq, m_sourcep, creqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	    When += rtt+100;
	}

	Manifold::StopAt(When + rtt + 100);
	Manifold::Run();


	//verify source get n responses
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        //vector<Ticks_t>& pticks = m_procp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_offsets+1, (int)cresps.size());


        //verify sink gets num_offsets+1 requests.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_offsets+1, (int)mreqs.size());

	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, ticks[0]);

        for(int i=0; i<num_offsets; i++) {
	    CPPUNIT_ASSERT_EQUAL(OpMemSt, mreqs[i+1].op_type);
	    CPPUNIT_ASSERT_EQUAL(addrs[i], mreqs[i+1].addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SRC_CACHE+HT_LOOKUP, ticks[i+1]);
	}

	//reset
	m_sinkp->set_send_reply(false);
    }



    //======================================================================
    //======================================================================
    //! @brief Send store with address x; then stores with address that differs from
    //! x only in the index portion; all miss
    //!
    //! Empty cache; store with a random address x; then try all address
    //! y that differs from x only in the index portion; all miss
    void test_handle_processor_request_and_handle_response_store_2()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create a STORE request
	paddr_t org_addr = random();
	CacheReq* creq = new CacheReq(org_addr, false);

        //compute index mask
	int offset_num_bits=0;
	int temp = HT_BLOCK_SIZE-1;
	while(temp > 0) {
	    temp >>= 1;
	    offset_num_bits++;
	}
	paddr_t index_mask = HT_SIZE/HT_BLOCK_SIZE/HT_ASSOC - 1;
	index_mask <<= offset_num_bits; //index_mask: the index portion is all 0's, and
	index_mask = ~index_mask;       //the tag and offset portions are all 1's;

	int num_of_indices = HT_SIZE/HT_BLOCK_SIZE/HT_ASSOC;

        CacheReq* creqs[num_of_indices];
	paddr_t addrs[num_of_indices];

	for(int i=0; i<num_of_indices; i++) { //try all indices
	    //one of the addrs[i] is the same as org_addr; the rest differs
	    //only in the index portion
	    addrs[i] = (org_addr & index_mask) | (i << offset_num_bits);
	    creqs[i] = new CacheReq(addrs[i], false);
        }

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for source to send the cache req
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;

        //schedule num_of_indices requests; one will hit; the rest will miss.
        Ticks_t scheduledAts[num_of_indices];
	for(int i=0; i<num_of_indices; i++) {
	    Manifold::Schedule(When, &Source::send_creq, m_sourcep, creqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	    When += rtt+100;
	}

	Manifold::StopAt(When + rtt + 100);
	Manifold::Run();


	//verify source responses.
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        //vector<Ticks_t>& pticks = m_procp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_of_indices+1, (int)cresps.size());


        //verify sink gets num_of_indices +1 reqs
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_of_indices+1, (int)mreqs.size());
	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, ticks[0]);


        for(int i=0; i<num_of_indices; i++) {
	    CPPUNIT_ASSERT_EQUAL(OpMemSt, mreqs[i+1].op_type);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SRC_CACHE+HT_LOOKUP, ticks[i+1]);
	}

	//reset
	m_sinkp->set_send_reply(false);
    }





    //======================================================================
    //======================================================================
    //! @brief Send store with address x; then stores with address that differs from
    //! x only in the tag portion; all miss
    //!
    //! Empty cache; store with a random address x; then try all address
    //! y that differs from x only in the tag portion; all miss
    void test_handle_processor_request_and_handle_response_store_3()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create a STORE request
	paddr_t org_addr = random();
	CacheReq* creq = new CacheReq(org_addr, false);

	// | tag || index || offset |
	int index_offset_num_bits=0; //number of index and offset bits
	int temp = HT_SIZE/HT_ASSOC-1;
	while(temp > 0) {
	    temp >>= 1;
	    index_offset_num_bits++;
	}

        //number of possible tags is usually big
	paddr_t largest_tag = (~0x0);
	largest_tag >>= index_offset_num_bits;

        //since addresses with the same index and offset are put in the same set,
	//up to 200 different tags should be enough for the test purposes
	int num_tags = (200 < largest_tag) ? 200 : largest_tag; //take the minimum

        CacheReq* creqs[num_tags];
	paddr_t addrs[num_tags];

	paddr_t tag_mask = largest_tag << index_offset_num_bits; //tag portion is all 1's; rest 0's
	for(int i=0; i<num_tags; i++) {
	    set<paddr_t> addrset; //use a set to generate unique values
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
	                                           //value is already in the set
	    //make sure ad is new and different from org_addr
	    do {
		ad = org_addr & (~tag_mask); //clear the tag portion
		ad |= ((random() & largest_tag) << index_offset_num_bits); // OR-in a random tag
		if(ad != org_addr)
		    ret = addrset.insert(ad);
	    } while(ad != org_addr && ret.second == false);

	    addrs[i] = ad;
	    creqs[i] = new CacheReq(addrs[i], false);
	    //cout << "ad= " << hex << ad << endl;
        }

 
	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for source to send the first cache req with the original address
	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();
	When += rtt+100;

        //schedule num_tags requests; all will miss.
        Ticks_t scheduledAts[num_tags];
	for(int i=0; i<num_tags; i++) {
	    Manifold::Schedule(When, &Source::send_creq, m_sourcep, creqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	    When += rtt+100;
	}

	Manifold::StopAt(When);
	Manifold::Run();


	//verify source responses.
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();
        //vector<Ticks_t>& pticks = m_procp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_tags+1, (int)cresps.size());


        //verify sink gets num_tags + 1 reqs
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        vector<Ticks_t>& ticks = m_sinkp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_tags+1, (int)mreqs.size());

	CPPUNIT_ASSERT_EQUAL(scheduledAt0+SRC_CACHE+HT_LOOKUP, ticks[0]);
	for(int i=0; i<num_tags+1; i++) {
	    CPPUNIT_ASSERT_EQUAL(OpMemSt, mreqs[i].op_type);
	}

        for(int i=0; i<num_tags; i++) {
	    CPPUNIT_ASSERT_EQUAL(addrs[i], mreqs[i+1].addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SRC_CACHE+HT_LOOKUP, ticks[i+1]);
	}

	//reset
	m_sinkp->set_send_reply(false);

    }



//##############################
// multiple outstanding requests
//##############################

    //======================================================================
    //======================================================================
    //! @brief test handle_processor_request(): multiple outstanding requests
    //!
    //! Send multiple requests for the same addr. Since response is not sent back,
    //! the MSHR holds all read requests.
    void test_handle_processor_request_multiple_outstanding_requests_0()
    {
	//create a LOAD request
	paddr_t addr = random();
	CacheReq* creq = new CacheReq(addr, true);

	//create a few requests to the same addr; random read/write
	const int N=10;
	CacheReq* creqs[N];
	int num_loads = 0;
	for(int i=0; i<N; i++) {
	    bool load = (random()%2==0) ? true : false;
	    creqs[i] = new CacheReq(addr, load);
	    if(load)
	        num_loads++;
	}

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the Source to send the cache req
	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();

	When += 1;
	Ticks_t scheduledAts[N];
	for(int i=0; i<N; i++) {
	    Manifold::Schedule(When, &Source::send_creq, m_sourcep, creqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	}

	Manifold::StopAt(scheduledAts[N-1] + SRC_CACHE+HT_LOOKUP + 100);
	Manifold::Run();


	//verify hash table
	hash_entry* hentry = m_cachep->my_table->get_entry(addr);
	CPPUNIT_ASSERT(hentry == 0); //only enter hash table after response


        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(1 + num_loads, int(m_cachep->m_mshr.size()));
	CPPUNIT_ASSERT_EQUAL(addr, m_cachep->m_mshr.front()->creq->addr);
	GenericPreqWrapper<CacheReq>* wrapper = dynamic_cast<GenericPreqWrapper<CacheReq>*>(m_cachep->m_mshr.front()->creq->preqWrapper);
	CPPUNIT_ASSERT(creq == wrapper->preq);

        //verify the sink gets req.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        //vector<Ticks_t>& ticks = m_sinkp->get_ticks();
	CPPUNIT_ASSERT_EQUAL(1+N-num_loads, int(mreqs.size()));

    }




    //======================================================================
    //======================================================================
    //! @brief test handle_processor_request(): multiple outstanding requests
    //!
    //! Send multiple requests to the same addr. When the reply to the first
    //! load comes back, all MSHR entries are released.
    void test_handle_processor_request_handle_response_multiple_outstanding_requests_0()
    {
	//tell sink to send reply
	m_sinkp->set_send_reply(true);

	//create a LOAD request
	paddr_t addr = random();
	CacheReq* creq = new CacheReq(addr, true);

	//create a few requests to the same addr; random read/write
	const int N=10;
	CacheReq* creqs[N];
	int num_loads = 0;
	for(int i=0; i<N; i++) {
	    bool load = (random()%2==0) ? true : false;
	    creqs[i] = new CacheReq(addr, load);
	    if(load)
	        num_loads++;
	}

	Manifold::unhalt();
	Ticks_t When = 1;
	//schedule for the Source to send the cache req
	Manifold::Schedule(When, &Source::send_creq, m_sourcep, creq);
	Ticks_t scheduledAt0 = When + MasterClock.NowTicks();

	Ticks_t rtt = SRC_CACHE + HT_LOOKUP + CACHE_SINK + Sink::LATENCY + HT_HIT; //round trip time

	When += 1;
	Ticks_t scheduledAts[N];
	for(int i=0; i<N; i++) {
	    Manifold::Schedule(When, &Source::send_creq, m_sourcep, creqs[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();
	}

	Manifold::StopAt(When + rtt + 100);
	Manifold::Run();


	//verify hash table
	hash_entry* hentry = m_cachep->my_table->get_entry(addr);
	CPPUNIT_ASSERT(hentry != 0);


        //verify MSHR
	CPPUNIT_ASSERT_EQUAL(0, int(m_cachep->m_mshr.size()));


	//verify source responses.
        vector<CacheReq>& cresps = m_sourcep->get_cache_resps();

	CPPUNIT_ASSERT_EQUAL(N+1, (int)cresps.size());


        //verify the sink gets req.
        vector<Cache_req>& mreqs = m_sinkp->get_reqs();
        //vector<Ticks_t>& ticks = m_sinkp->get_ticks();
	CPPUNIT_ASSERT_EQUAL(1+N-num_loads, int(mreqs.size()));

	//reset
	m_sinkp->set_send_reply(false);
    }





    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("simple_cacheTest");

        /*
	*/
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_load_miss_0", &simple_cacheTest::test_handle_processor_request_load_miss_0));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_load_load_hit_0", &simple_cacheTest::test_handle_processor_request_and_handle_response_load_load_hit_0));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_load_store_hit_0", &simple_cacheTest::test_handle_processor_request_and_handle_response_load_store_hit_0));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_load_1", &simple_cacheTest::test_handle_processor_request_and_handle_response_load_1));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_load_2", &simple_cacheTest::test_handle_processor_request_and_handle_response_load_2));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_load_3", &simple_cacheTest::test_handle_processor_request_and_handle_response_load_3));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_store_miss_0", &simple_cacheTest::test_handle_processor_request_store_miss_0));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_store_load_0", &simple_cacheTest::test_handle_processor_request_and_handle_response_store_load_0));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_store_store_0", &simple_cacheTest::test_handle_processor_request_and_handle_response_store_store_0));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_store_1", &simple_cacheTest::test_handle_processor_request_and_handle_response_store_1));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_store_2", &simple_cacheTest::test_handle_processor_request_and_handle_response_store_2));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_and_handle_response_store_3", &simple_cacheTest::test_handle_processor_request_and_handle_response_store_3));
	//multiple outstanding requests
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_multiple_outstanding_requests_0", &simple_cacheTest::test_handle_processor_request_multiple_outstanding_requests_0));
	mySuite->addTest(new CppUnit::TestCaller<simple_cacheTest>("test_handle_processor_request_handle_response_multiple_outstanding_requests_0", &simple_cacheTest::test_handle_processor_request_handle_response_multiple_outstanding_requests_0));
	/*
	*/

	return mySuite;
    }
};


Clock simple_cacheTest :: MasterClock(simple_cacheTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( simple_cacheTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


