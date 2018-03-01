#ifndef SYSBUILDER_LLP_H
#define SYSBUILDER_LLP_H

#include <libconfig.h++>
#include "kernel/manifold.h"
#include "kernel/component.h"
#include "iris/interfaces/simulatedLen.h"
#include "iris/interfaces/vnetAssign.h"
#include "iris/interfaces/genericIrisInterface.h"
#include "iris/genericTopology/ring.h"
#include "iris/genericTopology/torus.h"
#include "iris/genericTopology/torus6p.h"
#include "uarch/networkPacket.h"
#include "mcp-cache/cache_types.h"
#include "mcp-cache/MemoryControllerMap.h"
#include "mcp-cache/L1_cache.h"
#include "mcp-cache/L2_cache.h"
#include "mcp-cache/MESI_LLP_cache.h"
#include "mcp-cache/MESI_LLS_cache.h"
#include "CaffDRAM/Controller.h"
#include "spx/core.h"



// use a data structure to hold the component IDs of either a core node or a MC node.
enum { EMPTY_NODE=0, CORE_NODE, MC_NODE, CORE_MC_NODE };

struct Node_cid_llp {
    int type; //either a CORE_NODE or a MC_NODE
    manifold::kernel::CompId_t proc_cid;
    manifold::kernel::CompId_t l1_cache_cid;
    manifold::kernel::CompId_t l2_cache_cid;
    manifold::kernel::CompId_t mux_cid;
    manifold::kernel::CompId_t mc_cid;
};



class MySimLen;
class MyVnet;

//Objec of this class holds system configuration parameters.
class SysBuilder_llp {
public:
    //network topology
    enum { TOPO_RING=1, TOPO_TORUS, TOPO_TORUS6P}; //network topology
    enum { PART_1, PART_2, PART_Y}; //torus partitioning

    void config_network_topology(libconfig::Config& config);
    void config_components(libconfig::Config& config);
    void create_network(manifold::kernel::Clock& clock, manifold::iris::Terminal_to_net_mapping* mapping,
                        MySimLen* simLen, MyVnet* vn, int p=PART_1);
    void connect_components(const Node_cid_llp cids[]);
    void config_cache_settings(manifold::uarch::DestMap* l2_map,
                               manifold::uarch::DestMap* mc_map);
    //template<typename CORE>
    //void register_components_with_clock(const Node_cid_llp node_cids[]);
    //template<typename CORE>
    void print_stats(const Node_cid_llp node_cids[]);


    int net_topology;

    int x_dimension;
    int y_dimension;
    int MAX_NODES;

    //kernel
    int FREQ; //clock frequency
    manifold::kernel::Ticks_t STOP; //simulation stop time

    //proc
    vector<int> proc_node_idx_vec;
    set<int> proc_node_idx_set; //set is used to ensure each index is unique

    //cache
    manifold::mcp_cache_namespace::cache_settings l1_cache_parameters;
    manifold::mcp_cache_namespace::cache_settings l2_cache_parameters;
    unsigned L1_MSHR_SIZE;
    unsigned L2_MSHR_SIZE;
    int L1_downstream_credits;
    int L2_downstream_credits;
    manifold::mcp_cache_namespace::L1_cache_settings l1_settings;
    manifold::mcp_cache_namespace::L2_cache_settings l2_settings;

    //network
    int COH_MSG_TYPE;
    int MEM_MSG_TYPE;
    int CREDIT_MSG_TYPE;

    manifold::iris::ring_init_params ring_params;
    manifold::iris::torus_init_params torus_params;
    manifold::iris::torus6p_init_params torus6p_params;

    manifold::iris::Ring<manifold::uarch::NetworkPacket>* myRing;
    manifold::iris::Torus<manifold::uarch::NetworkPacket>* myTorus;
    manifold::iris::Torus6p<manifold::uarch::NetworkPacket>* myTorus6p;

    //mc
    int MC_DOWNSTREAM_CREDITS;
    manifold::caffdram::Dsettings dram_settings;  //use default values
    vector<int> mc_node_idx_vec;
    set<int> mc_node_idx_set; //set is used to ensure each index is unique
};

#if 0
//====================================================================
//====================================================================
template<typename CORE>
void SysBuilder_llp :: register_components_with_clock(const Node_cid_llp node_cids[])
{
    for(int i=0; i<this->MAX_NODES; i++) {
        if(node_cids[i].type == CORE_MC_NODE) {
	    CORE* proc = manifold::kernel::Component :: GetComponent<CORE>(node_cids[i].proc_cid);
	    if(proc) {
		manifold::kernel::Clock :: Register((core_t*)proc, &CORE::tick, (void(core_t::*)(void))0);
		//manifold::mcp_cache_namespace::LLP_cache* llp = manifold::kernel::Component :: GetComponent<manifold::mcp_cache_namespace::LLP_cache>(node_cids[i].l1_cache_cid);
		//manifold::mcp_cache_namespace::LLS_cache* lls = manifold::kernel::Component :: GetComponent<manifold::mcp_cache_namespace::LLS_cache>(node_cids[i].l2_cache_cid);
		//manifold::kernel::Clock :: Register(llp, &manifold::mcp_cache_namespace::LLP_cache::tick, (void(manifold::mcp_cache_namespace::LLP_cache::*)(void))0);
		//manifold::kernel::Clock :: Register(lls, &manifold::mcp_cache_namespace::LLS_cache::tick, (void(manifold::mcp_cache_namespace::LLS_cache::*)(void))0);
	    }

	    manifold::caffdram::Controller* mc = manifold::kernel::Component :: GetComponent<manifold::caffdram::Controller>(node_cids[i].mc_cid);
	    if(mc)
		mc->print_config(cout);	
	}
    }
#if 0
    for(int i=0; i<this->MAX_NODES; i++) {
        if(node_cids[i].type == CORE_NODE) {
	}
	else if(node_cids[i].type == MC_NODE) {
	    manifold::caffdram::Controller* mc = manifold::kernel::Component :: GetComponent<manifold::caffdram::Controller>(node_cids[i].mc_cid);
	    if(mc)
		mc->print_config(cout);
	}
    }
#endif
}
#endif


#if 0
//====================================================================
//====================================================================
template<typename CORE>
void SysBuilder_llp :: print_stats(const Node_cid_llp node_cids[])
{
    for(int i=0; i<this->MAX_NODES; i++) {
        if(node_cids[i].type == CORE_NODE) {
	    CORE* proc = manifold::kernel::Component :: GetComponent<CORE>(node_cids[i].proc_cid);
	    if(proc) {
		proc->print_stats();
		proc->print_stats(cout);
	    }

            manifold::mcp_cache_namespace::MESI_LLP_cache* l1 = manifold::kernel::Component :: GetComponent<manifold::mcp_cache_namespace::MESI_LLP_cache>(node_cids[i].l1_cache_cid);
	    manifold::mcp_cache_namespace::MESI_LLS_cache* l2 = manifold::kernel::Component :: GetComponent<manifold::mcp_cache_namespace::MESI_LLS_cache>(node_cids[i].l2_cache_cid);
	    if(l1) {
		l1->print_stats(cout);
		l2->print_stats(cout);
	    }
	}
	else if(node_cids[i].type == MC_NODE) {
	    manifold::caffdram::Controller* mc = manifold::kernel::Component :: GetComponent<manifold::caffdram::Controller>(node_cids[i].mc_cid);
	    if(mc)
		mc->print_stats(cout);
	}
    }

    if(myRing)
	myRing->print_stats(cout);
    else
	myTorus->print_stats(cout);

    manifold::kernel::Manifold :: print_stats(cout);
}
#endif

#endif // #ifndef SYSBUILDER_LLP_H
