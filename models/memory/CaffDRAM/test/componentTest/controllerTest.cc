//! This program tests CaffDram as a component. We use a Mock
//! component and connect the CaffDram's controller to the component.
//!
//!                   ------------
//!                  | MockSender |
//!                   ------------
//!                         |
//!                   ------------
//!                  | Controller |
//!                   ------------

#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include "Controller.h"

#include "kernel/manifold-decl.h"
#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace manifold::uarch;
using namespace std;
using namespace manifold::caffdram;
//using namespace manifold::simple_cache;

//####################################################################
// helper classes
//####################################################################

struct mem_req {
    enum {LOAD, STORE};
    int source_id;
    int source_port;
    int dest_id;
    int dest_port;
    unsigned long addr;
    int rw;

    mem_req() {}
    mem_req(int src, unsigned long ad, int r_w) : source_id(src), addr(ad), rw(r_w) {}
    int get_src() { return source_id; }
    void set_src(int s) { source_id = s; }
    int get_src_port() { return source_port; }
    void set_src_port(int sp) { source_port = sp; }
    int get_dst() { return dest_id; }
    void set_dst(int d) { dest_id = d; }
    int get_dst_port() { return dest_port; }
    void set_dst_port(int dp) { dest_port = dp; }
    unsigned long get_addr() { return addr; }
    bool is_read() { return rw == LOAD; }
    void set_mem_response() {}
};

#define MEM_MSG 11
#define CREDIT_MSG 22

//! This class emulates a component that sends requests to the MC.
class MockSender : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};

    MockSender(int nid) : m_nid(nid) {}

    int get_nid() { return m_nid; }

    void send_mreq(NetworkPacket* mreq)
    {
    //static int cnt=0;
    //std::cout << cnt++ << ": sender sending " << mreq->addr << " at " << Manifold::NowTicks() << std::endl;
	Send(OUT, mreq);
    }

    void handle_incoming(int, NetworkPacket* mresp)
    {
	if(mresp->type == CREDIT_MSG) {
	    m_credit_pkts.push_back(*mresp);
	    m_credit_ticks.push_back(Manifold :: NowTicks());
	}
        else {
	    m_mc_resps[mresp->dst_port] = *mresp; //the request's source_port is the response's dest_port.
	    m_ticks[mresp->dst_port] = Manifold :: NowTicks();
	}
	delete mresp;
    }
    map<int, NetworkPacket>& get_mc_resps() { return m_mc_resps; }
    list<NetworkPacket>& get_credit_pkts() { return m_credit_pkts; }
    map<int, Ticks_t>& get_ticks() { return m_ticks; }
    list<Ticks_t>& get_credit_ticks() { return m_credit_ticks; }

private:
    int m_nid; //node id
    //since responses can come out of order, we use a map to save them.
    //The key is the request's id. For example, the source port can be used as the id.
    map<int, NetworkPacket> m_mc_resps;
    list<NetworkPacket> m_credit_pkts;
    map<int, Ticks_t> m_ticks;
    list<Ticks_t> m_credit_ticks;
};



//####################################################################
//! Class ControllerTest is the test class for class Controller.
//####################################################################
class ControllerTest : public CppUnit::TestFixture {
private:
    struct CRB { //used to record a request's info
        unsigned ch;
	unsigned rank;
	unsigned bank;
	bool operator==(CRB& c)
	{
	    return (ch == c.ch && rank == c.rank && bank == c.bank);
	}
	bool operator<(const CRB& c) const
	{
	    if(ch < c.ch)
	        return true;
	    else if(ch > c.ch)
	        return false;
            else { //same channel
		if(rank < c.rank)
		    return true;
		else if(rank > c.rank)
		    return false;
		else { //same rank
		    if(bank < c.bank)
			return true;
		    else if(bank < c.bank)
			return false;
		    else
			return false;
		}
	    }
	}
    };
    class CRB_comp {
    public:
        bool operator()(const CRB& c1, const CRB& c2) const { return c1 < c2; }
    };

    //latencies
    static const Ticks_t SENDER_TO_MC = 1;

    MockSender* m_senderp;
    Controller* m_mcp;

    void mySetup(int nid, Dsettings& setting, int out_credits, bool st_resp)
    {
        //create a MockSender, and a Controller
	int s_nid; //sender node id.
	while((s_nid = random()%1024) == nid); //sender node id must be different.
	CompId_t senderId = Component :: Create<MockSender>(0, s_nid);
	m_senderp = Component::GetComponent<MockSender>(senderId);

	Controller :: Msg_type_set = false;
        Controller :: Set_msg_types(MEM_MSG, CREDIT_MSG);
	CompId_t mcId = Component :: Create<Controller>(0, nid, setting, out_credits, st_resp);
	m_mcp = Component::GetComponent<Controller>(mcId);

        //connect the components
	//sender to mc
	Manifold::Connect(senderId, MockSender::OUT, mcId, Controller::PORT0,
			  &Controller::handle_request<mem_req>, SENDER_TO_MC);
	//mc to sender
	Manifold::Connect(mcId, Controller::PORT0, senderId, MockSender::IN,
			  &MockSender::handle_incoming, SENDER_TO_MC);
    }

    //! Save the channel-rank-bank triple of an address. Copied from CaffDram
    static void save_crb(CRB* crb, uint64_t addr, const Dsettings& setting)
    {
	if (setting.numChannels == 1) {
	    crb->ch = 0;
	}
	else {
	    crb->ch = (addr >> (setting.channelShiftBits)) % (setting.numChannels);
	}
	crb->rank = (addr >> (setting.rankShiftBits)) & (setting.rankMask);
	crb->bank = (addr >> (setting.bankShiftBits)) & (setting.bankMask);
    }


    //! Get the row id of a given address, based on given settings.
    static unsigned get_rowId(uint64_t addr, const Dsettings& setting)
    {
	return (addr >> (setting.rowShiftBits)) & (setting.rowMask);
    }

    //! set rowId of addra to row, based on the CaffDram's setting.
    static void set_rowId(uint64_t* addra, uint64_t row, const Dsettings& setting)
    {
	assert(row == (row & setting.rowMask));

	uint64_t shiftedRowId = row << setting.rowShiftBits;
        uint64_t rowMask = setting.rowMask << setting.rowShiftBits;

	*addra &= (~rowMask);
	*addra |= shiftedRowId;
    }

    //! check if two addresses have the same row Id, based on the CaffDram's setting.
    static bool same_rowId(uint64_t addra, uint64_t addrb, Dsettings& setting)
    {
        uint64_t rowMask = setting.rowMask << setting.rowShiftBits;
	return ((addra & rowMask) == (addrb & rowMask));
    }


public:
    static Clock MasterClock;  //clock has to be global or static.
    enum { MASTER_CLOCK_HZ = 10 };

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }



    //======================================================================
    //======================================================================
    //! @brief Test handle_request with a single load.
    //!
    //! Create a Controller that sends NO response for stores. Send a single LOAD.
    //! Verify the response and its timing.
    void test_handle_request_0()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;
        mySetup(nid, setting, 1, false);

	//create a LOAD request
	unsigned long addr = random();
	mem_req mreq(m_senderp->get_nid(), addr, mem_req::LOAD);
	NetworkPacket* pkt = new NetworkPacket;
	pkt->type = MEM_MSG;
	*((mem_req*)(pkt->data)) = mreq;

	const int RID = random() % 1024;
	pkt->src = m_senderp->get_nid();
	pkt->src_port = RID;
	pkt->dst = nid;

	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;
	//schedule for the MockSender to send the mem_req
	Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkt);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time

	Manifold::StopAt(scheduledAt + rtt + 100);
	Manifold::Run();

        //verify the responses and their timing
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)mresps.size());

        map<int, Ticks_t>& ticks = m_senderp->get_ticks();
	NetworkPacket& resp_pkt = mresps[RID];

	CPPUNIT_ASSERT_EQUAL(nid, resp_pkt.src);
	CPPUNIT_ASSERT_EQUAL(m_senderp->get_nid(), resp_pkt.dst); //the response's dest_id is the Sender's id
	CPPUNIT_ASSERT_EQUAL(RID, resp_pkt.dst_port);

	mem_req& mresp = *((mem_req*)(resp_pkt.data));
	CPPUNIT_ASSERT_EQUAL(addr, mresp.addr);

	//1st access for an OPEN_PAGE MC
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SENDER_TO_MC+setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST + SENDER_TO_MC, ticks[RID]);


	list<NetworkPacket>& credit_pkts = m_senderp->get_credit_pkts();
	CPPUNIT_ASSERT_EQUAL(1, (int)credit_pkts.size());

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_request with a single store.
    //!
    //! Create a Controller that sends NO response for stores. Send a single STORE.
    //! Verify no response is received.
    void test_handle_request_1()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;
        mySetup(nid, setting, 1, false);

	//create a STORE request
	unsigned long addr = random();
	mem_req mreq(m_senderp->get_nid(), addr, mem_req::STORE);
	NetworkPacket* pkt = new NetworkPacket;
	pkt->type = MEM_MSG;
	*((mem_req*)(pkt->data)) = mreq;

	pkt->src = m_senderp->get_nid();
	pkt->dst = nid;

	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;
	//schedule for the MockSender to send the mem_req
	Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkt);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time

	Manifold::StopAt(scheduledAt + rtt + 100);
	Manifold::Run();


        //verify sender got no response
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();

	CPPUNIT_ASSERT_EQUAL(0, (int)mresps.size());

	list<NetworkPacket>& credit_pkts = m_senderp->get_credit_pkts();
	CPPUNIT_ASSERT_EQUAL(1, (int)credit_pkts.size());
	list<Ticks_t>& credit_ticks = m_senderp->get_credit_ticks();
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SENDER_TO_MC+setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST + SENDER_TO_MC, credit_ticks.front());
    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_request with N random LOADs and STOREs.
    //!
    //! Create an OPEN_PAGE Controller that sends NO response for stores. Send N random LOADs
    //! and STOREs. Verify the responses and their timing.
    //! At the time when we schedule the requests, we determine the time when
    //! the responses would come back. And at the end we verify the responses
    //! did come back at the expected times. The response time is determined
    //! based on the status of the MC's bank to which the request is destined.
    //! Therefore we use some data structures to record the states of the banks.
    void test_handle_request_2()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;
	//create Nreqs requests

	const int Nreqs = 10000;
        mySetup(nid, setting, Nreqs, false); //last parameter set to false, and the MC sends NO STORE responses.

	mem_req* reqs[Nreqs]; //the requests
	NetworkPacket* pkts[Nreqs]; //the NetworkPacket wrapper.

	//The following are data structures used to store the state of the banks.
	//When schedule request i, if the CRB of i hasn't been accessed before, accessed_map[CRB(i)]
	//would be false, then last_accessed[i] is set to -1, accessed_map[CRB(i)] is set to true, 
	//row_map[CRB(i)] is set to row(i), request_idx_map[CRB(i)] is set to i.
	//On the other hand, if the CRB of i has been accessed before, accessed_map[CRB(i)] would be
	//true, then last_accessed[i] is set to request_idx_map[CRB(i)], and request_idx_map[CRB(i)]
	//is updated to i, row_map[CRB(i)] is set to row(i). row_map is used so we can generate a
	//request with the same row as the last request to the same bank.
	CRB req_crbs[Nreqs]; //the request's channel-rank-bank
	int last_accessed[Nreqs]; //index of the request that last accessed the same
	                          //channel-rank-bank combo: -1 means the
	                          //bank hasn't been accessed before.
	map<CRB, bool, CRB_comp> accessed_map; //store whether a given channel-rank-bank triple has been accessed.
	map<CRB, unsigned, CRB_comp> row_map; //store row of the last request with sam channel-rank-bank triple.
	map<CRB, int, CRB_comp> request_idx_map; //store request index of the last request with sam channel-rank-bank triple.
	//mem_req reqs_copy[Nreqs]; //make a copy for verification; reqs will be deleted
	                          //by sender.


	int num_loads =0; //number of loads
        for(int i=0; i<Nreqs; i++) {
	    uint64_t addr = random();

	    //For each request, we need to know at the time of the request, whether the same channel-rank-bank
	    //has been accessed, and if so, the row id of the last access.
	    save_crb(&req_crbs[i], addr, setting); //save the request's channel, rank, bank
	    if(accessed_map[req_crbs[i]]) { //the same bank has been accessed.
	        last_accessed[i] = request_idx_map[req_crbs[i]]; //request_idx_map tells us the last request
		                                                 //with the same CRB
	    }
	    else {
	        last_accessed[i] = -1; //indicate at time of request i, the same bank hasn't been accessed
	    }
	    accessed_map[req_crbs[i]] = true; //update accessed_map.

	    if(last_accessed[i] != -1) { //for certain probablity, generate request with same rowId.
		if(random() / (RAND_MAX + 1.0) < 0.4) { // with certain probability, set rowId to the same
						        //as the previous access.
		    set_rowId(&addr, row_map[req_crbs[i]], setting); //row_map tells us the row of the last
		                                                     //access to the same bank.
		}
	    }
	    row_map[req_crbs[i]] = get_rowId(addr, setting); //update row_map
	    request_idx_map[req_crbs[i]] = i; //update reqeust_idx_map

	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(m_senderp->get_nid(), addr, mem_req::LOAD);
		num_loads++;
	    }
	    else {
		reqs[i] = new mem_req(m_senderp->get_nid(), addr, mem_req::STORE);
	    }
	    //In this program we use source port as a unique ID of the request, because responses
	    //can be in a different order than the requests.
	    reqs[i]->source_port = i;

	    //reqs_copy[i] = *reqs[i];

	    pkts[i] = new NetworkPacket;
	    pkts[i]->type = MEM_MSG;
	    *((mem_req*)(pkts[i]->data)) = *(reqs[i]);

	    pkts[i]->src = m_senderp->get_nid();
	    pkts[i]->src_port = i;
	    pkts[i]->dst = nid;
	}

        //now schedule all requests.
	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs]; //remember the schedule time for verification later
	Ticks_t expected_latency[Nreqs]; //expected latency is (expected_finish_time_of_the_request - time_when_request_reaches MC)


	const Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time
	const int BIG_INTERVAL = rtt + 100;
	for(int i=0; i<Nreqs; i++) {

	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkts[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();

	    if(last_accessed[i] == -1) {//first access to the bank and this is OPEN_PAGE
		expected_latency[i] = setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST;
	    }
	    else { //not first access
	        int last_request_idx = last_accessed[i];

		//Get the available_time of the bank.
		//Note available time is t_BURST before the last request finishes; and its an absolute time.
		uint64_t available_time = scheduledAts[last_request_idx] + SENDER_TO_MC + expected_latency[last_request_idx] - setting.t_BURST;

		//check if the request reaches the MC before or after its available_time
		//Note: in Dreq.cpp, the originTime of a request is the time when the MC gets a request + t_CMD.
		//scheduledAts[i] + SENDER_TO_MC is the time when request gets to the MC.
		if(scheduledAts[i] + SENDER_TO_MC + setting.t_CMD > available_time) {
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = setting.t_CMD+setting.t_CAS+setting.t_BURST;
		    else
			expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
		}
		else {//request's originTime is less than the bank's availableTime
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = available_time+setting.t_CAS+setting.t_BURST - (scheduledAts[i] + SENDER_TO_MC);
		    else
			expected_latency[i] = available_time+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST - (scheduledAts[i]+SENDER_TO_MC);
		}

	    }

	    if(random() / (RAND_MAX + 1.0) < 0.5) { //for certain probability, request sent before last one is finished.
		When += random() % 5 + 1; //a small interval
	    }
	    else {
	        When += BIG_INTERVAL;
	    }
	}


	Manifold::StopAt(scheduledAts[Nreqs-1] + expected_latency[Nreqs-1] + 100);
	Manifold::Run();

        //verify the responses and their timing
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
        map<int, Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_loads, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(num_loads, (int)ticks.size());


#if 0
for(int i=0; i<Nreqs; i++)
cout<<" req " << i << ": CRB= " << req_crbs[i].ch << "-" << req_crbs[i].rank << "-" << req_crbs[i].bank
<< " last accessed= " << last_accessed[i] << " row= " << get_rowId(reqs_copy[i].addr, setting) << " addr= " << dec << reqs_copy[i].addr << dec
<< "scheduled= " << scheduledAts[i] << " expected= " << expected_latency[i]
<<endl;
#endif

	map<int, NetworkPacket>::iterator it_req = mresps.begin();
	for(; it_req != mresps.end(); ++it_req) {
	    int i = (*it_req).first;
//cout << "i= " << i << " , addr= " << mresps[i].addr << " tick= " << ticks[i] << endl;
	    //CPPUNIT_ASSERT_EQUAL(reqs_copy[i].req_id, mresps[i].req_id);
	    CPPUNIT_ASSERT_EQUAL(reqs[i]->addr, ((mem_req*)(mresps[i].data))->addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SENDER_TO_MC+expected_latency[i]+SENDER_TO_MC, ticks[i]);
	    CPPUNIT_ASSERT_EQUAL(nid, (*it_req).second.src);
	    CPPUNIT_ASSERT_EQUAL(m_senderp->get_nid(), (*it_req).second.dst);

	}
	for(int i=0; i<Nreqs; i++)
	    delete reqs[i];

    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_request with N random LOADs and STOREs.
    //!
    //! Same as test_handle_request_2, except the MC is CLOSED_PAGE, and the
    //! timings are different.
    void test_handle_request_3()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;
	setting.memPagePolicy = CLOSED_PAGE;
	//create Nreqs requests
	const int Nreqs = 10000;

        mySetup(nid, setting, Nreqs, false); //last parameter set to false, and the MC sends NO STORE responses.

	mem_req* reqs[Nreqs]; //the requests
	NetworkPacket* pkts[Nreqs]; //the NetworkPacket wrapper.

	//The following are data structures used to store the state of the banks.
	//When schedule request i, if the CRB of i hasn't been accessed before, accessed_map[CRB(i)]
	//would be false, then last_accessed[i] is set to -1, accessed_map[CRB(i)] is set to true, 
	//row_map[CRB(i)] is set to row(i), request_idx_map[CRB(i)] is set to i.
	//On the other hand, if the CRB of i has been accessed before, accessed_map[CRB(i)] would be
	//true, then last_accessed[i] is set to request_idx_map[CRB(i)], and request_idx_map[CRB(i)]
	//is updated to i, row_map[CRB(i)] is set to row(i). row_map is used so we can generate a
	//request with the same row as the last request to the same bank.
	CRB req_crbs[Nreqs]; //the request's channel-rank-bank
	int last_accessed[Nreqs]; //index of the request that last accessed the same
	                          //channel-rank-bank combo: -1 means the
	                          //bank hasn't been accessed before.
	map<CRB, bool, CRB_comp> accessed_map; //store whether a given channel-rank-bank triple has been accessed.
	map<CRB, unsigned, CRB_comp> row_map; //store row of the last request with sam channel-rank-bank triple.
	map<CRB, int, CRB_comp> request_idx_map; //store request index of the last request with sam channel-rank-bank triple.
	//mem_req reqs_copy[Nreqs]; //make a copy for verification; reqs will be deleted
	                          //by sender.


	int num_loads =0; //number of loads
        for(int i=0; i<Nreqs; i++) {
	    uint64_t addr = random();

	    //For each request, we need to know at the time of the request, whether the same channel-rank-bank
	    //has been accessed, and if so, the row id of the last access.
	    save_crb(&req_crbs[i], addr, setting); //save the request's channel, rank, bank
	    if(accessed_map[req_crbs[i]]) { //the same bank has been accessed.
	        last_accessed[i] = request_idx_map[req_crbs[i]]; //request_idx_map tells us the last request
		                                                 //with the same CRB
	    }
	    else {
	        last_accessed[i] = -1; //indicate at time of request i, the same bank hasn't been accessed
	    }
	    accessed_map[req_crbs[i]] = true; //update accessed_map.

	    if(last_accessed[i] != -1) { //for certain probablity, generate request with same rowId.
		if(random() / (RAND_MAX + 1.0) < 0.4) { // with certain probability, set rowId to the same
						        //as the previous access.
		    set_rowId(&addr, row_map[req_crbs[i]], setting); //row_map tells us the row of the last
		                                                     //access to the same bank.
		}
	    }
	    row_map[req_crbs[i]] = get_rowId(addr, setting); //update row_map
	    request_idx_map[req_crbs[i]] = i; //update reqeust_idx_map

	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(m_senderp->get_nid(), addr, mem_req::LOAD);
		num_loads++;
	    }
	    else {
		reqs[i] = new mem_req(m_senderp->get_nid(), addr, mem_req::STORE);
	    }
	    //In this program we use source port as a unique ID of the request, because responses
	    //can be in a different order than the requests.
	    reqs[i]->source_port = i;

	    //reqs_copy[i] = *reqs[i];
	    pkts[i] = new NetworkPacket;
	    pkts[i]->type = MEM_MSG;
	    *((mem_req*)(pkts[i]->data)) = *(reqs[i]);

	    pkts[i]->src = m_senderp->get_nid();
	    pkts[i]->src_port = i;
	    pkts[i]->dst = nid;
	}

        //now schedule all requests.
	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs]; //remember the schedule time for verification later
	Ticks_t expected_latency[Nreqs]; //expected latency is (expected_finish_time_of_the_request - time_when_request_reaches MC)


	const Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time
	const int BIG_INTERVAL = rtt + 100;
	for(int i=0; i<Nreqs; i++) {
	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkts[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();

	    if(last_accessed[i] == -1) {//first access to the bank and this is CLOSED_PAGE
		expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
	    }
	    else { //not first access
	        int last_request_idx = last_accessed[i];

		//Get the available_time of the bank.
		//Note available time is t_BURST before the last request finishes; and its an absolute time.
		uint64_t available_time = scheduledAts[last_request_idx] + SENDER_TO_MC + expected_latency[last_request_idx] - setting.t_BURST;

		//check if the request reaches the MC before or after its available_time
		//Note: in Dreq.cpp, the originTime of a request is the time when the MC gets a request + t_CMD.
		//scheduledAts[i] + SENDER_TO_MC is the time when request gets to the MC.
		if(scheduledAts[i] + SENDER_TO_MC + setting.t_CMD > available_time) {
		    expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
		}
		else {//request's originTime is less than the bank's availableTime
		    expected_latency[i] = available_time+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST - (scheduledAts[i]+SENDER_TO_MC);
		}

	    }

	    if(random() / (RAND_MAX + 1.0) < 0.5) { //for certain probability, request sent before last one is finished.
		When += random() % 5 + 1; //a small interval
	    }
	    else {
	        When += BIG_INTERVAL;
	    }
	}

	Manifold::StopAt(scheduledAts[Nreqs-1] + expected_latency[Nreqs-1] + 100);
	Manifold::Run();

//for debug
#if 0
for(int i=0; i<Nreqs; i++)
cout<<" req " << i << ": CRB= " << req_crbs[i].ch << "-" << req_crbs[i].rank << "-" << req_crbs[i].bank
<< " last accessed= " << last_accessed[i] << " row= " << get_rowId(reqs_copy[i].addr, setting) << " addr= " << dec << reqs_copy[i].addr << dec
<< "scheduled= " << scheduledAts[i] << " expected= " << expected_latency[i]
<<endl;
#endif

        //verify the responses and their timing
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
        map<int, Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_loads, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(num_loads, (int)ticks.size());

	map<int, NetworkPacket>::iterator it_req = mresps.begin();
	for(; it_req != mresps.end(); ++it_req) {
	    int i = (*it_req).first;
//cout << "i= " << i << " , addr= " << mresps[i].addr << " tick= " << ticks[i] << endl;
	    //CPPUNIT_ASSERT_EQUAL(reqs_copy[i].req_id, mresps[i].req_id);
	    CPPUNIT_ASSERT_EQUAL(reqs[i]->addr, ((mem_req*)(mresps[i].data))->addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SENDER_TO_MC+expected_latency[i]+SENDER_TO_MC, ticks[i]);
	    CPPUNIT_ASSERT_EQUAL(nid, (*it_req).second.src);
	    CPPUNIT_ASSERT_EQUAL(m_senderp->get_nid(), (*it_req).second.dst);

	}

	for(int i=0; i<Nreqs; i++)
	    delete reqs[i];

    }














    //======================================================================
    //======================================================================
    //! @brief Test handle_request with a single load.
    //!
    //! Create a Controller that sends response for stores. Send a single LOAD.
    //! Verify the response and its timing.
    void test_handle_request_r0()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;
        mySetup(nid, setting, 1, true);

	//create a LOAD request
	unsigned long addr = random();
	mem_req mreq(m_senderp->get_nid(), addr, mem_req::LOAD);

	NetworkPacket* pkt = new NetworkPacket;
	pkt->type = MEM_MSG;
	*((mem_req*)(pkt->data)) = mreq;

	const int RID = 123; //request ID
	pkt->src = m_senderp->get_nid();
	pkt->src_port = RID;
	pkt->dst = nid;


	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;
	//schedule for the MockSender to send the mem_req
	Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkt);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time

	Manifold::StopAt(scheduledAt + rtt + 100);
	Manifold::Run();

        //verify the responses and their timing
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)mresps.size());

        map<int, Ticks_t>& ticks = m_senderp->get_ticks();
	NetworkPacket& resp_pkt= mresps[RID];

	CPPUNIT_ASSERT_EQUAL(nid, resp_pkt.src);
	CPPUNIT_ASSERT_EQUAL(m_senderp->get_nid(), resp_pkt.dst); //the response's dest_id is the Sender's id
	CPPUNIT_ASSERT_EQUAL(RID, resp_pkt.dst_port);

	mem_req& mresp = *((mem_req*)(resp_pkt.data));
	CPPUNIT_ASSERT_EQUAL(addr, mresp.addr);

	//1st access for an OPEN_PAGE MC
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SENDER_TO_MC+setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST+SENDER_TO_MC, ticks[RID]);
    }




    //======================================================================
    //======================================================================
    //! @brief Test handle_request with a single store.
    //!
    //! Create a Controller that sends response for stores. Send a single STORE.
    //! Verify the response and its timing.
    void test_handle_request_r1()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;
        mySetup(nid, setting, 1, true);

	//create a STORE request
	unsigned long addr = random();
	mem_req mreq(m_senderp->get_nid(), addr, mem_req::STORE);

	NetworkPacket* pkt = new NetworkPacket;
	pkt->type = MEM_MSG;
	*((mem_req*)(pkt->data)) = mreq;

	const int RID = 123; //request ID
	pkt->src = m_senderp->get_nid();
	pkt->src_port = RID;
	pkt->dst = nid;


	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;
	//schedule for the MockSender to send the mem_req
	Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkt);
	Ticks_t scheduledAt = When + MasterClock.NowTicks();

	Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time

	Manifold::StopAt(scheduledAt + rtt + 100);
	Manifold::Run();

        //verify the responses and their timing
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
	CPPUNIT_ASSERT_EQUAL(1, (int)mresps.size());

        map<int, Ticks_t>& ticks = m_senderp->get_ticks();
	NetworkPacket& resp_pkt= mresps[RID];

	CPPUNIT_ASSERT_EQUAL(nid, resp_pkt.src);
	CPPUNIT_ASSERT_EQUAL(m_senderp->get_nid(), resp_pkt.dst); //the response's dest_id is the Sender's id
	CPPUNIT_ASSERT_EQUAL(RID, resp_pkt.dst_port);

	mem_req& mresp = *((mem_req*)(resp_pkt.data));
	CPPUNIT_ASSERT_EQUAL(addr, mresp.addr);

	//1st access for an OPEN_PAGE MC
	CPPUNIT_ASSERT_EQUAL(scheduledAt+SENDER_TO_MC+setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST+SENDER_TO_MC, ticks[RID]);

    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_request with N random LOADs and STOREs.
    //!
    //! Create an OPEN_PAGE Controller that sends response for stores. Send N random LOADs
    //! and STOREs. Verify the responses and their timing.
    //! At the time when we schedule the requests, we determine the time when
    //! the responses would come back. And at the end we verify the responses
    //! did come back at the expected times. The response time is determined
    //! based on the status of the MC's bank to which the request is destined.
    //! Therefore we use some data structures to record the states of the banks.
    void test_handle_request_r2()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;
	//create Nreqs requests
	const int Nreqs = 10000;

        mySetup(nid, setting, Nreqs, true); //last parameter set to true, and the MC sends STORE responses.

	mem_req* reqs[Nreqs]; //the requests
	NetworkPacket* pkts[Nreqs]; //the NetworkPacket wrapper.

	//The following are data structures used to store the state of the banks.
	//When schedule request i, if the CRB of i hasn't been accessed before, accessed_map[CRB(i)]
	//would be false, then last_accessed[i] is set to -1, accessed_map[CRB(i)] is set to true, 
	//row_map[CRB(i)] is set to row(i), request_idx_map[CRB(i)] is set to i.
	//On the other hand, if the CRB of i has been accessed before, accessed_map[CRB(i)] would be
	//true, then last_accessed[i] is set to request_idx_map[CRB(i)], and request_idx_map[CRB(i)]
	//is updated to i, row_map[CRB(i)] is set to row(i). row_map is used so we can generate a
	//request with the same row as the last request to the same bank.
	CRB req_crbs[Nreqs]; //the request's channel-rank-bank
	int last_accessed[Nreqs]; //index of the request that last accessed the same
	                          //channel-rank-bank combo: -1 means the
	                          //bank hasn't been accessed before.
	map<CRB, bool, CRB_comp> accessed_map; //store whether a given channel-rank-bank triple has been accessed.
	map<CRB, unsigned, CRB_comp> row_map; //store row of the last request with sam channel-rank-bank triple.
	map<CRB, int, CRB_comp> request_idx_map; //store request index of the last request with sam channel-rank-bank triple.


        for(int i=0; i<Nreqs; i++) {
	    int req_id = random();
	    uint64_t addr = random();

	    //For each request, we need to know at the time of the request, whether the same channel-rank-bank
	    //has been accessed, and if so, the row id of the last access.
	    save_crb(&req_crbs[i], addr, setting); //save the request's channel, rank, bank
	    if(accessed_map[req_crbs[i]]) { //the same bank has been accessed.
	        last_accessed[i] = request_idx_map[req_crbs[i]]; //request_idx_map tells us the last request
		                                                 //with the same CRB
	    }
	    else {
	        last_accessed[i] = -1; //indicate at time of request i, the same bank hasn't been accessed
	    }
	    accessed_map[req_crbs[i]] = true; //update accessed_map.

	    if(last_accessed[i] != -1) { //for certain probablity, generate request with same rowId.
		if(random() / (RAND_MAX + 1.0) < 0.4) { // with certain probability, set rowId to the same
						        //as the previous access.
		    set_rowId(&addr, row_map[req_crbs[i]], setting); //row_map tells us the row of the last
		                                                     //access to the same bank.
		}
	    }
	    row_map[req_crbs[i]] = get_rowId(addr, setting); //update row_map
	    request_idx_map[req_crbs[i]] = i; //update reqeust_idx_map

	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(m_senderp->get_nid(), addr, mem_req::LOAD);
	    }
	    else {
		reqs[i] = new mem_req(m_senderp->get_nid(), addr, mem_req::STORE);
	    }
	    //In this program we use source port as a unique ID of the request, because responses
	    //can be in a different order than the requests.
	    reqs[i]->source_port = i;

	    pkts[i] = new NetworkPacket;
	    pkts[i]->type = MEM_MSG;
	    *((mem_req*)(pkts[i]->data)) = *(reqs[i]);

	    pkts[i]->src = m_senderp->get_nid();
	    pkts[i]->src_port = i;
	    pkts[i]->dst = nid;

	}

        //now schedule all requests.
	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs]; //remember the schedule time for verification later
	Ticks_t expected_latency[Nreqs]; //expected latency is (expected_finish_time_of_the_request - time_when_request_reaches MC)


	const Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time
	const int BIG_INTERVAL = rtt + 100;
	for(int i=0; i<Nreqs; i++) {
	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkts[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();

	    if(last_accessed[i] == -1) {//first access to the bank and this is OPEN
		expected_latency[i] = setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST;
	    }
	    else { //not first access
	        int last_request_idx = last_accessed[i];

		//Get the available_time of the bank.
		//Note available time is t_BURST before the last request finishes; and its an absolute time.
		uint64_t available_time = scheduledAts[last_request_idx] + SENDER_TO_MC + expected_latency[last_request_idx] - setting.t_BURST;

		//check if the request reaches the MC before or after its available_time
		//Note: in Dreq.cpp, the originTime of a request is the time when the MC gets a request + t_CMD.
		//scheduledAts[i] + SENDER_TO_MC is the time when request gets to the MC.
		if(scheduledAts[i] + SENDER_TO_MC + setting.t_CMD > available_time) {
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = setting.t_CMD+setting.t_CAS+setting.t_BURST;
		    else
			expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
		}
		else {//request's originTime is less than the bank's availableTime
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = available_time+setting.t_CAS+setting.t_BURST - (scheduledAts[i] + SENDER_TO_MC);
		    else
			expected_latency[i] = available_time+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST - (scheduledAts[i]+SENDER_TO_MC);
		}

	    }

	    if(random() / (RAND_MAX + 1.0) < 0.5) { //for certain probability, request sent before last one is finished.
		When += random() % 5 + 1; //a small interval
	    }
	    else {
	        When += BIG_INTERVAL;
	    }
	}


	Manifold::StopAt(scheduledAts[Nreqs-1] + expected_latency[Nreqs-1] + 100);
	Manifold::Run();

        //verify the responses and their timing
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
        map<int, Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(Nreqs, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(Nreqs, (int)ticks.size());

//for debug
#if 0
for(int i=0; i<Nreqs; i++)
//cout<<" req " << i << ": CRB= " << req_crbs[i].ch << "-" << req_crbs[i].rank << "-" << req_crbs[i].bank
//<< " last accessed= " << last_accessed[i] << " row= " << get_rowId(reqs_copy[i].addr, setting) << " addr= " << dec << reqs_copy[i].addr << dec
//<< "scheduled= " << scheduledAts[i] << " expected= " << expected_latency[i]
//<<endl;
}
#endif

        for(int i=0; i<Nreqs; i++) {
	    CPPUNIT_ASSERT_EQUAL(reqs[i]->addr, ((mem_req*)(mresps[i].data))->addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SENDER_TO_MC+expected_latency[i]+SENDER_TO_MC, ticks[i]);
	    CPPUNIT_ASSERT_EQUAL(nid, mresps[i].src);
	    CPPUNIT_ASSERT_EQUAL(m_senderp->get_nid(), mresps[i].dst);
	}
    }





    //======================================================================
    //======================================================================
    //! @brief Test handle_request with N random LOADs and STOREs.
    //!
    //! Same as test_handle_request_r3, except the MC is CLOSED_PAGE, and the
    //! timings are different.
    void test_handle_request_r3()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;
	setting.memPagePolicy = CLOSED_PAGE;

	//create Nreqs requests
	const int Nreqs = 10000;
        mySetup(nid, setting, Nreqs, true); //last parameter set to true, and the MC sends STORE responses.

	mem_req* reqs[Nreqs]; //the requests
	NetworkPacket* pkts[Nreqs]; //the NetworkPacket wrapper.


	//The following are data structures used to store the state of the banks.
	//When schedule request i, if the CRB of i hasn't been accessed before, accessed_map[CRB(i)]
	//would be false, then last_accessed[i] is set to -1, accessed_map[CRB(i)] is set to true, 
	//row_map[CRB(i)] is set to row(i), request_idx_map[CRB(i)] is set to i.
	//On the other hand, if the CRB of i has been accessed before, accessed_map[CRB(i)] would be
	//true, then last_accessed[i] is set to request_idx_map[CRB(i)], and request_idx_map[CRB(i)]
	//is updated to i, row_map[CRB(i)] is set to row(i). row_map is used so we can generate a
	//request with the same row as the last request to the same bank.
	CRB req_crbs[Nreqs]; //the request's channel-rank-bank
	int last_accessed[Nreqs]; //index of the request that last accessed the same
	                          //channel-rank-bank combo: -1 means the
	                          //bank hasn't been accessed before.
	map<CRB, bool, CRB_comp> accessed_map; //store whether a given channel-rank-bank triple has been accessed.
	map<CRB, unsigned, CRB_comp> row_map; //store row of the last request with sam channel-rank-bank triple.
	map<CRB, int, CRB_comp> request_idx_map; //store request index of the last request with sam channel-rank-bank triple.


        for(int i=0; i<Nreqs; i++) {
	    int req_id = random();
	    uint64_t addr = random();

	    //For each request, we need to know at the time of the request, whether the same channel-rank-bank
	    //has been accessed, and if so, the row id of the last access.
	    save_crb(&req_crbs[i], addr, setting); //save the request's channel, rank, bank
	    if(accessed_map[req_crbs[i]]) { //the same bank has been accessed.
	        last_accessed[i] = request_idx_map[req_crbs[i]]; //request_idx_map tells us the last request
		                                                 //with the same CRB
	    }
	    else {
	        last_accessed[i] = -1; //indicate at time of request i, the same bank hasn't been accessed
	    }
	    accessed_map[req_crbs[i]] = true; //update accessed_map.

	    if(last_accessed[i] != -1) { //for certain probablity, generate request with same rowId.
		if(random() / (RAND_MAX + 1.0) < 0.4) { // with certain probability, set rowId to the same
						        //as the previous access.
		    set_rowId(&addr, row_map[req_crbs[i]], setting); //row_map tells us the row of the last
		                                                     //access to the same bank.
		}
	    }
	    row_map[req_crbs[i]] = get_rowId(addr, setting); //update row_map
	    request_idx_map[req_crbs[i]] = i; //update reqeust_idx_map

	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(m_senderp->get_nid(), addr, mem_req::LOAD);
	    }
	    else {
		reqs[i] = new mem_req(m_senderp->get_nid(), addr, mem_req::STORE);
	    }
	    //In this program we use source port as a unique ID of the request, because responses
	    //can be in a different order than the requests.
	    reqs[i]->source_port = i;

	    pkts[i] = new NetworkPacket;
	    pkts[i]->type = MEM_MSG;
	    *((mem_req*)(pkts[i]->data)) = *(reqs[i]);

	    pkts[i]->src = m_senderp->get_nid();
	    pkts[i]->src_port = i;
	    pkts[i]->dst = nid;

	}

        //now schedule all requests.
	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs]; //remember the schedule time for verification later
	Ticks_t expected_latency[Nreqs]; //expected latency is (expected_finish_time_of_the_request - time_when_request_reaches MC)


	const Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time
	const int BIG_INTERVAL = rtt + 100;
	for(int i=0; i<Nreqs; i++) {
	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkts[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();

	    if(last_accessed[i] == -1) {//first access to the bank and this is CLOSED
		expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
	    }
	    else { //not first access
	        int last_request_idx = last_accessed[i];

		//Get the available_time of the bank.
		//Note available time is t_BURST before the last request finishes; and its an absolute time.
		uint64_t available_time = scheduledAts[last_request_idx] + SENDER_TO_MC + expected_latency[last_request_idx] - setting.t_BURST;

		//check if the request reaches the MC before or after its available_time
		//Note: in Dreq.cpp, the originTime of a request is the time when the MC gets a request + t_CMD.
		//scheduledAts[i] + SENDER_TO_MC is the time when request gets to the MC.
		if(scheduledAts[i] + SENDER_TO_MC + setting.t_CMD > available_time) {
		    expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
		}
		else {//request's originTime is less than the bank's availableTime
		    expected_latency[i] = available_time+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST - (scheduledAts[i]+SENDER_TO_MC);
		}

	    }

	    if(random() / (RAND_MAX + 1.0) < 0.5) { //for certain probability, request sent before last one is finished.
		When += random() % 5 + 1; //a small interval
	    }
	    else {
	        When += BIG_INTERVAL;
	    }
	}


	Manifold::StopAt(scheduledAts[Nreqs-1] + expected_latency[Nreqs-1] + 100);
	Manifold::Run();

        //verify the responses and their timing
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
        map<int, Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(Nreqs, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(Nreqs, (int)ticks.size());

//for debug
#if 0
for(int i=0; i<Nreqs; i++)
//cout<<" req " << i << ": CRB= " << req_crbs[i].ch << "-" << req_crbs[i].rank << "-" << req_crbs[i].bank
//<< " last accessed= " << last_accessed[i] << " row= " << get_rowId(reqs_copy[i].addr, setting) << " addr= " << dec << reqs_copy[i].addr << dec
//<< "scheduled= " << scheduledAts[i] << " expected= " << expected_latency[i]
//<<endl;
}
#endif

        for(int i=0; i<Nreqs; i++) {
	    CPPUNIT_ASSERT_EQUAL(reqs[i]->addr, ((mem_req*)(mresps[i].data))->addr);
	    CPPUNIT_ASSERT_EQUAL(scheduledAts[i]+SENDER_TO_MC+expected_latency[i]+SENDER_TO_MC, ticks[i]);
	    CPPUNIT_ASSERT_EQUAL(nid, mresps[i].src);
	    CPPUNIT_ASSERT_EQUAL(m_senderp->get_nid(), mresps[i].dst);
	}
    }





    //======================================================================
    //======================================================================
    //! @brief Test flow control with N random LOADs and STOREs: open page
    //! controller that sends NO response for stores. Credits <= number of loads;
    //!
    //! This function is almost identical to handle_request_2.
    //!
    void test_flow_control_0()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;

	const int Nreqs = 10000;

	mem_req* reqs[Nreqs]; //the requests
	NetworkPacket* pkts[Nreqs]; //the NetworkPacket wrapper.

	//The following are data structures used to store the state of the banks.
	//When schedule request i, if the CRB of i hasn't been accessed before, accessed_map[CRB(i)]
	//would be false, then last_accessed[i] is set to -1, accessed_map[CRB(i)] is set to true, 
	//row_map[CRB(i)] is set to row(i), request_idx_map[CRB(i)] is set to i.
	//On the other hand, if the CRB of i has been accessed before, accessed_map[CRB(i)] would be
	//true, then last_accessed[i] is set to request_idx_map[CRB(i)], and request_idx_map[CRB(i)]
	//is updated to i, row_map[CRB(i)] is set to row(i). row_map is used so we can generate a
	//request with the same row as the last request to the same bank.
	CRB req_crbs[Nreqs]; //the request's channel-rank-bank
	int last_accessed[Nreqs]; //index of the request that last accessed the same
	                          //channel-rank-bank combo: -1 means the
	                          //bank hasn't been accessed before.
	map<CRB, bool, CRB_comp> accessed_map; //store whether a given channel-rank-bank triple has been accessed.
	map<CRB, unsigned, CRB_comp> row_map; //store row of the last request with sam channel-rank-bank triple.
	map<CRB, int, CRB_comp> request_idx_map; //store request index of the last request with sam channel-rank-bank triple.

	int num_loads =0; //number of loads
        for(int i=0; i<Nreqs; i++) {
	    uint64_t addr = random();

	    //For each request, we need to know at the time of the request, whether the same channel-rank-bank
	    //has been accessed, and if so, the row id of the last access.
	    save_crb(&req_crbs[i], addr, setting); //save the request's channel, rank, bank
	    if(accessed_map[req_crbs[i]]) { //the same bank has been accessed.
	        last_accessed[i] = request_idx_map[req_crbs[i]]; //request_idx_map tells us the last request
		                                                 //with the same CRB
	    }
	    else {
	        last_accessed[i] = -1; //indicate at time of request i, the same bank hasn't been accessed
	    }
	    accessed_map[req_crbs[i]] = true; //update accessed_map.

	    if(last_accessed[i] != -1) { //for certain probablity, generate request with same rowId.
		if(random() / (RAND_MAX + 1.0) < 0.4) { // with certain probability, set rowId to the same
						        //as the previous access.
		    set_rowId(&addr, row_map[req_crbs[i]], setting); //row_map tells us the row of the last
		                                                     //access to the same bank.
		}
	    }
	    row_map[req_crbs[i]] = get_rowId(addr, setting); //update row_map
	    request_idx_map[req_crbs[i]] = i; //update reqeust_idx_map

	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(0, addr, mem_req::LOAD); //src is randomly set to 0 since m_senderp is not created yet.
		num_loads++;
	    }
	    else {
		reqs[i] = new mem_req(0, addr, mem_req::STORE);
	    }
	    //In this program we use source port as a unique ID of the request, because responses
	    //can be in a different order than the requests.
	    reqs[i]->source_port = i;

	    //reqs_copy[i] = *reqs[i];

	    pkts[i] = new NetworkPacket;
	    pkts[i]->type = MEM_MSG;
	    *((mem_req*)(pkts[i]->data)) = *(reqs[i]);

	    //pkts[i]->src = m_senderp->get_nid(); //m_senderp not created yet.
	    pkts[i]->src_port = i;
	    pkts[i]->dst = nid;
	}

        const int CREDITS = random() % num_loads + 1;
        mySetup(nid, setting, CREDITS, false); //last parameter set to false, and the MC sends NO STORE responses.
	for(int i=0; i<Nreqs; i++)
	    pkts[i]->src = m_senderp->get_nid();


        //now schedule all requests.
	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs]; //remember the schedule time for verification later
	Ticks_t expected_latency[Nreqs]; //expected latency is (expected_finish_time_of_the_request - time_when_request_reaches MC)


	const Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time
	const int BIG_INTERVAL = rtt + 100;
	for(int i=0; i<Nreqs; i++) {

	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkts[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();

	    if(last_accessed[i] == -1) {//first access to the bank and this is OPEN_PAGE
		expected_latency[i] = setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST;
	    }
	    else { //not first access
	        int last_request_idx = last_accessed[i];

		//Get the available_time of the bank.
		//Note available time is t_BURST before the last request finishes; and its an absolute time.
		uint64_t available_time = scheduledAts[last_request_idx] + SENDER_TO_MC + expected_latency[last_request_idx] - setting.t_BURST;

		//check if the request reaches the MC before or after its available_time
		//Note: in Dreq.cpp, the originTime of a request is the time when the MC gets a request + t_CMD.
		//scheduledAts[i] + SENDER_TO_MC is the time when request gets to the MC.
		if(scheduledAts[i] + SENDER_TO_MC + setting.t_CMD > available_time) {
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = setting.t_CMD+setting.t_CAS+setting.t_BURST;
		    else
			expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
		}
		else {//request's originTime is less than the bank's availableTime
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = available_time+setting.t_CAS+setting.t_BURST - (scheduledAts[i] + SENDER_TO_MC);
		    else
			expected_latency[i] = available_time+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST - (scheduledAts[i]+SENDER_TO_MC);
		}

	    }

	    if(random() / (RAND_MAX + 1.0) < 0.5) { //for certain probability, request sent before last one is finished.
		When += random() % 5 + 1; //a small interval
	    }
	    else {
	        When += BIG_INTERVAL;
	    }
	}


	Manifold::StopAt(scheduledAts[Nreqs-1] + expected_latency[Nreqs-1] + 100);
	Manifold::Run();

        //verify the number of load responses
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
        map<int, Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(CREDITS, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(CREDITS, (int)ticks.size());


        //verify the number of credit messages
        list<Ticks_t>& credit_ticks = m_senderp->get_credit_ticks();
	CPPUNIT_ASSERT_EQUAL(CREDITS + Nreqs-num_loads, (int)credit_ticks.size()); //There is one credit for each of the CREDITS
	                                                                           //load responses, and (Nreqs-num_loads) STOREs.

        //verify the timing of credit messages
	for(int i=0; i<Nreqs; i++) {
	    if(reqs[i]->rw == mem_req :: STORE || (mresps.find(i) != mresps.end())) {
		Ticks_t expected_tick = scheduledAts[i]+SENDER_TO_MC+expected_latency[i]+SENDER_TO_MC;
		list<Ticks_t>::iterator it = find(credit_ticks.begin(), credit_ticks.end(), expected_tick);
		CPPUNIT_ASSERT(it != credit_ticks.end());
		credit_ticks.erase(it);
	    }
	}

	for(int i=0; i<Nreqs; i++)
	    delete reqs[i];

    }




    //======================================================================
    //======================================================================
    //! @brief Test flow control with N random LOADs and STOREs: open page
    //! controller that sends NO response for stores. Credits > number of loads;
    //!
    //! This function is almost identical to handle_request_2.
    //!
    void test_flow_control_1()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;

	const int Nreqs = 10000;

	mem_req* reqs[Nreqs]; //the requests
	NetworkPacket* pkts[Nreqs]; //the NetworkPacket wrapper.

	//The following are data structures used to store the state of the banks.
	//When schedule request i, if the CRB of i hasn't been accessed before, accessed_map[CRB(i)]
	//would be false, then last_accessed[i] is set to -1, accessed_map[CRB(i)] is set to true, 
	//row_map[CRB(i)] is set to row(i), request_idx_map[CRB(i)] is set to i.
	//On the other hand, if the CRB of i has been accessed before, accessed_map[CRB(i)] would be
	//true, then last_accessed[i] is set to request_idx_map[CRB(i)], and request_idx_map[CRB(i)]
	//is updated to i, row_map[CRB(i)] is set to row(i). row_map is used so we can generate a
	//request with the same row as the last request to the same bank.
	CRB req_crbs[Nreqs]; //the request's channel-rank-bank
	int last_accessed[Nreqs]; //index of the request that last accessed the same
	                          //channel-rank-bank combo: -1 means the
	                          //bank hasn't been accessed before.
	map<CRB, bool, CRB_comp> accessed_map; //store whether a given channel-rank-bank triple has been accessed.
	map<CRB, unsigned, CRB_comp> row_map; //store row of the last request with sam channel-rank-bank triple.
	map<CRB, int, CRB_comp> request_idx_map; //store request index of the last request with sam channel-rank-bank triple.

	int num_loads =0; //number of loads
        for(int i=0; i<Nreqs; i++) {
	    uint64_t addr = random();

	    //For each request, we need to know at the time of the request, whether the same channel-rank-bank
	    //has been accessed, and if so, the row id of the last access.
	    save_crb(&req_crbs[i], addr, setting); //save the request's channel, rank, bank
	    if(accessed_map[req_crbs[i]]) { //the same bank has been accessed.
	        last_accessed[i] = request_idx_map[req_crbs[i]]; //request_idx_map tells us the last request
		                                                 //with the same CRB
	    }
	    else {
	        last_accessed[i] = -1; //indicate at time of request i, the same bank hasn't been accessed
	    }
	    accessed_map[req_crbs[i]] = true; //update accessed_map.

	    if(last_accessed[i] != -1) { //for certain probablity, generate request with same rowId.
		if(random() / (RAND_MAX + 1.0) < 0.4) { // with certain probability, set rowId to the same
						        //as the previous access.
		    set_rowId(&addr, row_map[req_crbs[i]], setting); //row_map tells us the row of the last
		                                                     //access to the same bank.
		}
	    }
	    row_map[req_crbs[i]] = get_rowId(addr, setting); //update row_map
	    request_idx_map[req_crbs[i]] = i; //update reqeust_idx_map

	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(0, addr, mem_req::LOAD); //src is randomly set to 0 since m_senderp is not created yet.
		num_loads++;
	    }
	    else {
		reqs[i] = new mem_req(0, addr, mem_req::STORE);
	    }
	    //In this program we use source port as a unique ID of the request, because responses
	    //can be in a different order than the requests.
	    reqs[i]->source_port = i;

	    //reqs_copy[i] = *reqs[i];

	    pkts[i] = new NetworkPacket;
	    pkts[i]->type = MEM_MSG;
	    *((mem_req*)(pkts[i]->data)) = *(reqs[i]);

	    //pkts[i]->src = m_senderp->get_nid(); //m_senderp not created yet.
	    pkts[i]->src_port = i;
	    pkts[i]->dst = nid;
	}

        const int CREDITS = random() % 50 + 1 + num_loads; // randomly pick from [num_loads+1, num_loads+50]
        mySetup(nid, setting, CREDITS, false); //last parameter set to false, and the MC sends NO STORE responses.
	for(int i=0; i<Nreqs; i++)
	    pkts[i]->src = m_senderp->get_nid();


        //now schedule all requests.
	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs]; //remember the schedule time for verification later
	Ticks_t expected_latency[Nreqs]; //expected latency is (expected_finish_time_of_the_request - time_when_request_reaches MC)


	const Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time
	const int BIG_INTERVAL = rtt + 100;
	for(int i=0; i<Nreqs; i++) {

	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkts[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();

	    if(last_accessed[i] == -1) {//first access to the bank and this is OPEN_PAGE
		expected_latency[i] = setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST;
	    }
	    else { //not first access
	        int last_request_idx = last_accessed[i];

		//Get the available_time of the bank.
		//Note available time is t_BURST before the last request finishes; and its an absolute time.
		uint64_t available_time = scheduledAts[last_request_idx] + SENDER_TO_MC + expected_latency[last_request_idx] - setting.t_BURST;

		//check if the request reaches the MC before or after its available_time
		//Note: in Dreq.cpp, the originTime of a request is the time when the MC gets a request + t_CMD.
		//scheduledAts[i] + SENDER_TO_MC is the time when request gets to the MC.
		if(scheduledAts[i] + SENDER_TO_MC + setting.t_CMD > available_time) {
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = setting.t_CMD+setting.t_CAS+setting.t_BURST;
		    else
			expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
		}
		else {//request's originTime is less than the bank's availableTime
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = available_time+setting.t_CAS+setting.t_BURST - (scheduledAts[i] + SENDER_TO_MC);
		    else
			expected_latency[i] = available_time+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST - (scheduledAts[i]+SENDER_TO_MC);
		}

	    }

	    if(random() / (RAND_MAX + 1.0) < 0.5) { //for certain probability, request sent before last one is finished.
		When += random() % 5 + 1; //a small interval
	    }
	    else {
	        When += BIG_INTERVAL;
	    }
	}


	Manifold::StopAt(scheduledAts[Nreqs-1] + expected_latency[Nreqs-1] + 100);
	Manifold::Run();

        //verify the number of load responses
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
        map<int, Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(num_loads, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(num_loads, (int)ticks.size());


        //verify the number of credit messages
        list<Ticks_t>& credit_ticks = m_senderp->get_credit_ticks();
	CPPUNIT_ASSERT_EQUAL(Nreqs, (int)credit_ticks.size());

        //verify the timing of credit messages
	for(int i=0; i<Nreqs; i++) {
	    //if(reqs[i]->rw == mem_req :: STORE || (mresps.find(i) != mresps.end())) {
		Ticks_t expected_tick = scheduledAts[i]+SENDER_TO_MC+expected_latency[i]+SENDER_TO_MC;
		list<Ticks_t>::iterator it = find(credit_ticks.begin(), credit_ticks.end(), expected_tick);
		CPPUNIT_ASSERT(it != credit_ticks.end());
		credit_ticks.erase(it);
	    //}
	}

	for(int i=0; i<Nreqs; i++)
	    delete reqs[i];

    }




    //======================================================================
    //======================================================================
    //! @brief Test flow control with N random LOADs and STOREs: open page
    //! controller that sends NO response for stores. Credits <= number of loads;
    //!
    //! Verify sending credits back to MC allow responses waiting for credits
    //! to be sent out. This function is almost identical to handle_request_2.
    //!
    void test_flow_control_2()
    {
        int nid = random() % 1024;  //node id for the mc
	Dsettings setting;

	const int Nreqs = 10000;

	mem_req* reqs[Nreqs]; //the requests
	NetworkPacket* pkts[Nreqs]; //the NetworkPacket wrapper.

	//The following are data structures used to store the state of the banks.
	//When schedule request i, if the CRB of i hasn't been accessed before, accessed_map[CRB(i)]
	//would be false, then last_accessed[i] is set to -1, accessed_map[CRB(i)] is set to true, 
	//row_map[CRB(i)] is set to row(i), request_idx_map[CRB(i)] is set to i.
	//On the other hand, if the CRB of i has been accessed before, accessed_map[CRB(i)] would be
	//true, then last_accessed[i] is set to request_idx_map[CRB(i)], and request_idx_map[CRB(i)]
	//is updated to i, row_map[CRB(i)] is set to row(i). row_map is used so we can generate a
	//request with the same row as the last request to the same bank.
	CRB req_crbs[Nreqs]; //the request's channel-rank-bank
	int last_accessed[Nreqs]; //index of the request that last accessed the same
	                          //channel-rank-bank combo: -1 means the
	                          //bank hasn't been accessed before.
	map<CRB, bool, CRB_comp> accessed_map; //store whether a given channel-rank-bank triple has been accessed.
	map<CRB, unsigned, CRB_comp> row_map; //store row of the last request with sam channel-rank-bank triple.
	map<CRB, int, CRB_comp> request_idx_map; //store request index of the last request with sam channel-rank-bank triple.

	int num_loads =0; //number of loads
        for(int i=0; i<Nreqs; i++) {
	    uint64_t addr = random();

	    //For each request, we need to know at the time of the request, whether the same channel-rank-bank
	    //has been accessed, and if so, the row id of the last access.
	    save_crb(&req_crbs[i], addr, setting); //save the request's channel, rank, bank
	    if(accessed_map[req_crbs[i]]) { //the same bank has been accessed.
	        last_accessed[i] = request_idx_map[req_crbs[i]]; //request_idx_map tells us the last request
		                                                 //with the same CRB
	    }
	    else {
	        last_accessed[i] = -1; //indicate at time of request i, the same bank hasn't been accessed
	    }
	    accessed_map[req_crbs[i]] = true; //update accessed_map.

	    if(last_accessed[i] != -1) { //for certain probablity, generate request with same rowId.
		if(random() / (RAND_MAX + 1.0) < 0.4) { // with certain probability, set rowId to the same
						        //as the previous access.
		    set_rowId(&addr, row_map[req_crbs[i]], setting); //row_map tells us the row of the last
		                                                     //access to the same bank.
		}
	    }
	    row_map[req_crbs[i]] = get_rowId(addr, setting); //update row_map
	    request_idx_map[req_crbs[i]] = i; //update reqeust_idx_map

	    if(random() / (RAND_MAX + 1.0) < 0.5) { // 50% LOADs
		reqs[i] = new mem_req(0, addr, mem_req::LOAD); //src is randomly set to 0 since m_senderp is not created yet.
		num_loads++;
	    }
	    else {
		reqs[i] = new mem_req(0, addr, mem_req::STORE);
	    }
	    //In this program we use source port as a unique ID of the request, because responses
	    //can be in a different order than the requests.
	    reqs[i]->source_port = i;

	    //reqs_copy[i] = *reqs[i];

	    pkts[i] = new NetworkPacket;
	    pkts[i]->type = MEM_MSG;
	    *((mem_req*)(pkts[i]->data)) = *(reqs[i]);

	    //pkts[i]->src = m_senderp->get_nid(); //m_senderp not created yet.
	    pkts[i]->src_port = i;
	    pkts[i]->dst = nid;
	}

        const int CREDITS = random() % num_loads - 10;
	assert(CREDITS > 0);

        mySetup(nid, setting, CREDITS, false); //last parameter set to false, and the MC sends NO STORE responses.
	for(int i=0; i<Nreqs; i++)
	    pkts[i]->src = m_senderp->get_nid();


        //now schedule all requests.
	Manifold::Reset(Manifold::TICKED);
	Ticks_t When = 1;

	//schedule for the MockSender to send the mem_reqs
	Ticks_t scheduledAts[Nreqs]; //remember the schedule time for verification later
	Ticks_t expected_latency[Nreqs]; //expected latency is (expected_finish_time_of_the_request - time_when_request_reaches MC)


	const Ticks_t rtt = SENDER_TO_MC + setting.t_CMD + setting.t_RP + setting.t_RCD + setting.t_CAS + setting.t_BURST; //round trip time
	const int BIG_INTERVAL = rtt + 100;
	for(int i=0; i<Nreqs; i++) {

	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkts[i]);
	    scheduledAts[i] = When + MasterClock.NowTicks();

	    if(last_accessed[i] == -1) {//first access to the bank and this is OPEN_PAGE
		expected_latency[i] = setting.t_CMD+setting.t_RCD+setting.t_CAS+setting.t_BURST;
	    }
	    else { //not first access
	        int last_request_idx = last_accessed[i];

		//Get the available_time of the bank.
		//Note available time is t_BURST before the last request finishes; and its an absolute time.
		uint64_t available_time = scheduledAts[last_request_idx] + SENDER_TO_MC + expected_latency[last_request_idx] - setting.t_BURST;

		//check if the request reaches the MC before or after its available_time
		//Note: in Dreq.cpp, the originTime of a request is the time when the MC gets a request + t_CMD.
		//scheduledAts[i] + SENDER_TO_MC is the time when request gets to the MC.
		if(scheduledAts[i] + SENDER_TO_MC + setting.t_CMD > available_time) {
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = setting.t_CMD+setting.t_CAS+setting.t_BURST;
		    else
			expected_latency[i] = setting.t_CMD+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST;
		}
		else {//request's originTime is less than the bank's availableTime
		    if(same_rowId(reqs[i]->addr, reqs[last_request_idx]->addr, setting)) //if same row is accessed
			expected_latency[i] = available_time+setting.t_CAS+setting.t_BURST - (scheduledAts[i] + SENDER_TO_MC);
		    else
			expected_latency[i] = available_time+setting.t_RP+setting.t_RCD+setting.t_CAS+setting.t_BURST - (scheduledAts[i]+SENDER_TO_MC);
		}

	    }

	    if(random() / (RAND_MAX + 1.0) < 0.5) { //for certain probability, request sent before last one is finished.
		When += random() % 5 + 1; //a small interval
	    }
	    else {
	        When += BIG_INTERVAL;
	    }
	}


        //send a few credits back.
	const int Ncredits = random() % (num_loads - CREDITS);
	for(int i=0; i<Ncredits; i++) {
	    NetworkPacket* pkt = new NetworkPacket;
	    pkt->type = CREDIT_MSG;
	    Manifold::Schedule(When, &MockSender::send_mreq, m_senderp, pkt);
	}

	Manifold::StopAt(scheduledAts[Nreqs-1] + expected_latency[Nreqs-1] + 100);
	Manifold::Run();

        //verify the number of load responses
	map<int, NetworkPacket>& mresps = m_senderp->get_mc_resps();
        map<int, Ticks_t>& ticks = m_senderp->get_ticks();

	CPPUNIT_ASSERT_EQUAL(CREDITS + Ncredits, (int)mresps.size());
	CPPUNIT_ASSERT_EQUAL(CREDITS + Ncredits, (int)ticks.size());


        //verify the number of credit messages
        list<Ticks_t>& credit_ticks = m_senderp->get_credit_ticks();
	CPPUNIT_ASSERT_EQUAL(CREDITS + Ncredits + Nreqs-num_loads, (int)credit_ticks.size()); //There is one credit for each of the CREDITS
	                                                                                      //load responses, and (Nreqs-num_loads) STOREs.

	for(int i=0; i<Nreqs; i++)
	    delete reqs[i];

    }















    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ControllerTest");

	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_handle_request_0", &ControllerTest::test_handle_request_0));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_handle_request_1", &ControllerTest::test_handle_request_1));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_handle_request_2", &ControllerTest::test_handle_request_2));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_handle_request_3", &ControllerTest::test_handle_request_3));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_handle_request_r0", &ControllerTest::test_handle_request_r0));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_handle_request_r1", &ControllerTest::test_handle_request_r1));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_handle_request_r2", &ControllerTest::test_handle_request_r2));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_handle_request_r3", &ControllerTest::test_handle_request_r3));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_flow_control_0", &ControllerTest::test_flow_control_0));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_flow_control_1", &ControllerTest::test_flow_control_1));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_flow_control_2", &ControllerTest::test_flow_control_2));

	return mySuite;
    }
};


Clock ControllerTest :: MasterClock(ControllerTest :: MASTER_CLOCK_HZ);

int main()
{
    Manifold :: Init();
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ControllerTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


