#ifndef MANIFOLD_MCP_CACHE_L2_CACHE_H
#define MANIFOLD_MCP_CACHE_L2_CACHE_H

#include <vector>
#include "cache_types.h"
#include "coherence/ManagerInterface.h"
#include "kernel/component.h"
#include "hash_table.h"
#include "MemoryControllerMap.h"
#include "coh_mem_req.h"
#include "uarch/networkPacket.h"
#include "uarch/DestMap.h"

#include "cache-counters.h"

using namespace std;

namespace manifold {
namespace mcp_cache_namespace {


struct L2_cache_settings {
public:
    manifold::uarch::DestMap* mc_map;
    unsigned mshr_sz; //mshr size
    int downstream_credits;
};



class L2_cache : public manifold::kernel::Component {
public:
    enum {PORT_L1=0};

    hash_table *my_table;

#ifdef MCP_CACHE_COUNTERS
    L2_counter_t *cache_counter;
#endif

    L2_cache (int nid, const cache_settings&, const L2_cache_settings&);
    virtual ~L2_cache (void);

    int get_node_id() { return node_id; }

    //void handle_incoming (int, Coh_mem_req* request);

    template<typename T>
    void handle_incoming (int, manifold::uarch::NetworkPacket*);

    void send_msg_to_l1(Coh_msg* msg);
    void client_writeback(ManagerInterface*);
    void invalidate(ManagerInterface*);
    void ignore(ManagerInterface*);

    hash_entry* get_hash_entry_by_idx(unsigned idx) { return hash_entries[idx]; }

    void print_stats(std::ostream&);

    static void Set_msg_types(int coh, int mem, int credit)
    {
        COH_MSG = coh;
	MEM_MSG = mem;
	CREDIT_MSG = credit;
    }
private:
    void process_client_request (Coh_msg* request, bool first);
    void process_client_reply (Coh_msg* reply);
    void process_mem_resp(Mem_msg*);

    void L2_algorithm (hash_entry *e, Coh_msg *request);
    void start_eviction(ManagerInterface* manager, Coh_msg* request);

    //! called when manager enters stable state while processing lower client request.
    void m_notify(ManagerInterface*);

    //! called when manager enters stable state while evicting an L2 line.
    void m_evict_notify(ManagerInterface* manager);

    void stall (Coh_msg *req, stall_type_t stall);
    bool stall_buffer_has_match(paddr_t addr);

    static void update_hash_entry(hash_entry* e1, hash_entry* e2);

    void release_mshr_entry(hash_entry* mshr_entry);

    //debug
    void print_mshr();
    void print_stall_buffer();

#ifdef MCP_CACHE_UTEST
public:
#else
protected:
#endif

    void process_incoming_coh(Coh_msg*);

    static int COH_MSG;
    static int MEM_MSG;
    static int CREDIT_MSG;

    int node_id;
    manifold::uarch::DestMap* mc_map;

    hash_table *mshr; /** Can view MSHRs as temporary blocks for use while an eviction is pending. Thus, a fully associative hash_table works great. */
     
    std::vector<ManagerInterface*> managers;
    std::vector<hash_entry*> hash_entries; //allows mapping from manager to hash_entry


    std::map<int, hash_entry*> mshr_map; //map an mshr entry id to an entry in the hash table

    struct Stall_buffer_entry {
        Coh_msg* req;
	stall_type_t type;
	manifold::kernel::Ticks_t time; //when it was stalled.
    };

    //std::list<std::pair <Coh_mem_req *, stall_type_t> > stalled_client_req_buffer;    
    std::list<Stall_buffer_entry> stalled_client_req_buffer;    

    std::vector<Coh_msg*> mcp_stalled_req; //stalled request for each manager; with this, after a manager finishes a request, we can
                                                   //easily identify the stalled request in the stall buffer.

    virtual void get_from_memory (Coh_msg *request);
    virtual void dirty_to_memory (paddr_t);

    //flow control
    int m_downstream_credits;
    const int DOWNSTREAM_FULL_CREDITS; //for debug purpose only
    std::list<manifold::uarch::NetworkPacket*> m_downstream_output_buffer; //buffer holding output msg to L1
    void send_msg_after_lookup_time(manifold::uarch::NetworkPacket* pkt);
    virtual void try_send();
    void schedule_send_credit();
    virtual void send_credit_downstream();

    //output prediction
    std::list<manifold::kernel::Ticks_t> m_credit_out_ticks;
    std::list<manifold::kernel::Ticks_t> m_msg_out_ticks;

    //stats
    unsigned long stats_cycles;
    unsigned stats_num_reqs; //number of L1 requests
    unsigned stats_miss; //number of misses
    unsigned stats_MSHR_STALLs;
    unsigned stats_PREV_PEND_STALLs;
    unsigned stats_LRU_BUSY_STALLs;
    unsigned stats_TRANS_STALLs;
    unsigned stats_stall_buffer_max_size;
    unsigned long stats_table_occupancy; //accumulated hash table occupancy
    unsigned stats_table_empty_cycles;
    unsigned long stats_mshr_occupancy; //accumulated mshr occupancy
    unsigned stats_mshr_empty_cycles;
};



template<typename T>
void L2_cache :: handle_incoming (int, manifold::uarch::NetworkPacket* pkt)
{
    if(pkt->type == CREDIT_MSG) {
        delete pkt;
	m_downstream_credits++;
	assert(m_downstream_credits <= DOWNSTREAM_FULL_CREDITS);
	try_send();
	return;
    }

    if(pkt->type == COH_MSG) {
        Coh_msg* coh = new Coh_msg;
	*coh = *((Coh_msg*)pkt->data);
	process_incoming_coh(coh);
#ifdef MCP_CACHE_COUNTERS
        cache_counter->DL2.read_tag += 1;
        cache_counter->DL2.search += 1;
        if (coh->rw == 1) { //write
            cache_counter->DL2.write_tag += 1;
            cache_counter->DL2.write += 1;
        } else
            cache_counter->DL2.read += 1;
#endif
    }
    else if(pkt->type == MEM_MSG) {
        T objT = *((T*)pkt->data);
	Mem_msg* mem = new Mem_msg;
	mem->type = Mem_msg :: MEM_RPLY;
	mem->addr = objT.get_addr();
	if(objT.is_read())
	    mem->op_type = OpMemLd;
	else
	    mem->op_type = OpMemSt;
#ifdef MCP_CACHE_COUNTERS
        cache_counter->DL2.read_tag += 1;
        cache_counter->DL2.search += 1;
        if (mem->op_type == OpMemSt) { //read
            cache_counter->DL2.write_tag += 1;
            cache_counter->DL2.write += 1;
        } else
            cache_counter->DL2.read += 1;
#endif

        process_mem_resp(mem);
    }
    else {
        assert(0);
    }

    schedule_send_credit();
    delete pkt;
}





} //namespace mcp_cache
} //namespace manifold


#endif // MANIFOLD_MCP_CACHE_L2_CACHE_H
