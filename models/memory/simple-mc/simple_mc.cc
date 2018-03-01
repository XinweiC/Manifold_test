#include "kernel/component.h"
#include "kernel/manifold.h"
#include "simple_mc.h"

#include <iostream>
using namespace std;
using namespace manifold::kernel;
using namespace manifold::simple_cache;

namespace manifold {
namespace simple_mc {


//! Event handler for incoming memory request.
void SimpleMC :: handle_incoming(int, mem_req* req)
{
    if(req->op_type == OpMemLd) {
#ifdef DBG_SIMPLE_MC
cout << "mc received LD, src= " << req->source_id << " port= " <<req->source_port << " addr= " <<hex<< req->addr <<dec<<endl;
#endif
	//stats
	m_ld_misses[req->source_id]++;
	m_req_info.insert(pair<Ticks_t, Req_info>(Manifold::NowTicks(),
	                                          Req_info(OpMemLd, req->originator_id, req->source_id, req->addr)));

	req->msg = LD_RESPONSE;
	req->req_id = req->req_id;
	req->dest_id = req->source_id;
	req->dest_port = req->source_port;
	req->source_id = m_nid;
	req->source_port = 0;
	SendTick(OUT, req, m_latency);
    }
    else {
#ifdef DBG_SIMPLE_MC
cout << "mc received ST, src= " << req->source_id << " port= " <<req->source_port << " addr= " <<hex<< req->addr <<dec<<endl;
#endif
	//stats
	m_stores[req->source_id]++;
	m_req_info.insert(pair<Ticks_t, Req_info>(Manifold::NowTicks(),
	                                          Req_info(OpMemSt, req->originator_id, req->source_id, req->addr)));
        if(m_send_st_response) {
	    req->msg = ST_COMPLETE;
	    req->req_id = req->req_id;
	    req->dest_id = req->source_id;
	    req->dest_port = req->source_port;
	    req->source_id = m_nid;
	    req->source_port = 0;
	    SendTick(OUT, req, m_latency);
	}
    }
}


void SimpleMC :: print_config(ostream& out)
{
    out << "********** SimpleMC " << m_nid << " config **********" << endl;
    out << "    Latency: " << m_latency << endl;
    out << "    Send STORE response: " << (m_send_st_response ? "yes" : "no") << endl;
}


void SimpleMC :: print_stats(ostream& out)
{
    out << "********** SimpleMC " << m_nid << " stats **********" << endl;
    out << "=== Load misses ===" << endl;
    for(map<int, int>::iterator it = m_ld_misses.begin(); it != m_ld_misses.end(); ++it) {
        out << "    " << (*it).first << ": " << (*it).second << endl;
    }
    out << "=== stores ===" << endl;
    for(map<int, int>::iterator it = m_stores.begin(); it != m_stores.end(); ++it) {
        out << "    " << (*it).first << ": " << (*it).second << endl;
    }

    for(multimap<Ticks_t, Req_info>::iterator it = m_req_info.begin(); it != m_req_info.end(); ++it) {
        Req_info& req = (*it).second;
        out << "MC received: " << (*it).first << " " << ((req.type == OpMemLd) ? "LD" : "ST")
	    << " N_" << req.org_id << "(" << req.src_id << ") " << hex << "0x" << req.addr << dec << endl;
    }
}




//! @param \c nodeIds    A vector of node IDs of memory controllers. Obviously, multiple MCs are supported.
//! @param \c line_size    Size of cache line. Memory addresses are split among the MCs based on lines. Therefore
//! we need to know the line size in order to determine the MC for a given address.
SimpleMcMap :: SimpleMcMap(std::vector<int>& nodeIds, int line_size) : m_nodeIds(nodeIds)
{
    assert(nodeIds.size() > 0);

    if(nodeIds.size() > 1) {
        //determine the number of bits in the offset portion of an address.
	m_offset_bits = 0;
	while((0x1 << m_offset_bits) < line_size) {
	    m_offset_bits++;
	}

	assert((0x1 << m_offset_bits) == line_size); //line_size must be a power of 2

        //determine the number of bits required to select the MC. Assuming there are N MCs,
	//the bits required, B,  is  2^(B-1) < N <= 2^B
	int mc_selector_bits = 0;
	while((0x1 << mc_selector_bits) < (int)nodeIds.size()) {
	    mc_selector_bits++;
	}
	m_mc_selector_mask = (0x1 << mc_selector_bits) - 1;
    }

}

//! Return the node ID of the MC for a given address.
int SimpleMcMap :: lookup(manifold::simple_cache::paddr_t addr)
{
    if(m_nodeIds.size() == 1) { //if there is only one MC
	return m_nodeIds[0];
    }
    else {
	int idx = ((addr >> m_offset_bits) & m_mc_selector_mask) % m_nodeIds.size();
	return m_nodeIds[idx];
    }
}


} // namespace simple_mc
} // namespace manifold


