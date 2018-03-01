#ifndef MANIDOLD_SIMPLE_MC_SIMPLE_MC_H
#define MANIDOLD_SIMPLE_MC_SIMPLE_MC_H

#include "kernel/component-decl.h"
#include "simple-cache/cache_messages.h"
#include "simple-cache/MemoryControllerMap.h"
#include <map>
#include <vector>
#include <iostream>

namespace manifold {
namespace simple_mc {


//! This class emulates a MC. Upon receiving a request it simply returns a response
//! after certain delay.
class SimpleMC : public manifold::kernel::Component {
public:
    enum {OUT=0};
    enum {IN=0};

    SimpleMC(int nid, manifold::kernel::Ticks_t latency, bool st_resp=false) : m_nid(nid), m_latency(latency), m_send_st_response(st_resp) {}

    int get_nid() { return m_nid; }

    void handle_incoming(int, manifold::simple_cache::mem_req* req);

    void print_config(std::ostream&);
    void print_stats(std::ostream&);
private:
    int m_nid;  //node id
    manifold::kernel::Ticks_t m_latency;
    bool m_send_st_response;  //send response for stores

    //for stats
    struct Req_info {
        Req_info(int t, int oid, int sid, manifold::simple_cache::paddr_t a) : type(t), org_id(oid), src_id(sid), addr(a) {}
        int type; //ld or st
        int org_id; //originator id
        int src_id; //source id; could be different from originator id, e.g., when it's from an LLS cache
	manifold::simple_cache::paddr_t addr;
    };

    std::map<int, int> m_ld_misses; //number of ld misses per source
    std::map<int, int> m_stores;
    std::multimap<manifold::kernel::Ticks_t, Req_info> m_req_info; //request info in time order
};


//! This memory controller map assumes there is only 1 memory controller, whose ID
//! is given in the constructor.
class SimpleMcMap1 : public manifold::simple_cache::MemoryControllerMap {
public:
    SimpleMcMap1(int nodeId) : m_nodeId(nodeId)
    {}

    int lookup(manifold::simple_cache::paddr_t addr)
    {
        return m_nodeId;
    }
private:
    int m_nodeId;
};


//! This memory controller map distributes the memory lines (based on cache line)
//! among the controllers.
class SimpleMcMap : public manifold::simple_cache::MemoryControllerMap {
public:
    SimpleMcMap(std::vector<int>& nodeIds, int line_size);

    int lookup(manifold::simple_cache::paddr_t addr);

private:
    std::vector<int> m_nodeIds;
    int m_offset_bits;
    manifold::simple_cache::paddr_t  m_mc_selector_mask;
};


} // namespace simple_mc
} // namespace manifold

#endif //MANIDOLD_SIMPLE_MC_SIMPLE_MC_H
