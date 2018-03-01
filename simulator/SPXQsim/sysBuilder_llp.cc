//!
//!

#include "sysBuilder_llp.h"
#include "common.h"

#include "kernel/clock.h"
#include "kernel/component.h"
#include "kernel/manifold.h"
#include "mcp-cache/mux_demux.h"
#include "CaffDRAM/Controller.h"
#include "CaffDRAM/McMap.h"
#include "iris/genericTopology/genericTopoCreator.h"

using namespace manifold::kernel;
using namespace manifold::uarch;
using namespace manifold::spx;
using namespace manifold::mcp_cache_namespace;
using namespace manifold::caffdram;
using namespace manifold::iris;
using namespace libconfig;


//====================================================================
//====================================================================
void SysBuilder_llp :: config_network_topology(Config& config)
{
    //==========================================================================
    // Determine network topology
    //==========================================================================
    string topo;

    try {
	const char* topo1 = config.lookup("network.topology");
	topo = topo1;
	this->x_dimension = config.lookup("network.x_dimension");
	this->y_dimension = config.lookup("network.y_dimension");
    }
    catch (SettingNotFoundException e) {
	cout << e.getPath() << " not set." << endl;
	exit(1);
    }
    catch (SettingTypeException e) {
	cout << e.getPath() << " has incorrect type." << endl;
	exit(1);
    }

    if(topo == "RING") {
        this->net_topology = TOPO_RING;
    }
    else if(topo == "TORUS") {
        this->net_topology = TOPO_TORUS;
    }
    else if(topo == "TORUS6P" ) {
        this->net_topology = TOPO_TORUS6P;
    }
    else {
        cout << "Unknown network topology: " << topo << endl;
	exit(1);
    }


    assert(this->x_dimension > 0 && this->y_dimension > 0);
    assert(this->x_dimension * this->y_dimension >= 1);

    if(this->net_topology == (int)TOPO_RING)
        assert(this->y_dimension == 1);

    MAX_NODES = this->x_dimension * this->y_dimension; //max no. of nodes
}



//====================================================================
//====================================================================
void SysBuilder_llp :: config_components(Config& config)
{
    //==========================================================================
    // Configuration parameters
    //==========================================================================
    try {
	//FREQ = config.lookup("clock_frequency");
	//assert(FREQ > 0);
	STOP = config.lookup("simulation_stop");

	// No configuration for Processor

	//cache parameters
	l1_cache_parameters.name = config.lookup("llp_cache.name");
	l1_cache_parameters.size = config.lookup("llp_cache.size");
	l1_cache_parameters.assoc = config.lookup("llp_cache.assoc");
	l1_cache_parameters.block_size = config.lookup("llp_cache.block_size");
	l1_cache_parameters.hit_time = config.lookup("llp_cache.hit_time");
	l1_cache_parameters.lookup_time = config.lookup("llp_cache.lookup_time");
	l1_cache_parameters.replacement_policy = RP_LRU;
	L1_MSHR_SIZE = config.lookup("llp_cache.mshr_size");

	L1_downstream_credits = config.lookup("llp_cache.downstream_credits");

	l2_cache_parameters.name = config.lookup("lls_cache.name");
	l2_cache_parameters.size = config.lookup("lls_cache.size");
	l2_cache_parameters.assoc = config.lookup("lls_cache.assoc");
	l2_cache_parameters.block_size = config.lookup("lls_cache.block_size");
	l2_cache_parameters.hit_time = config.lookup("lls_cache.hit_time");
	l2_cache_parameters.lookup_time = config.lookup("lls_cache.lookup_time");
	l2_cache_parameters.replacement_policy = RP_LRU;
	L2_MSHR_SIZE = config.lookup("lls_cache.mshr_size");

	L2_downstream_credits = config.lookup("lls_cache.downstream_credits");



	//network parameters
	// x and y dimensions already specified
	if(this->net_topology == SysBuilder_llp::TOPO_RING) {
	    ring_params.no_nodes = MAX_NODES;
	    ring_params.no_vcs = config.lookup("network.num_vcs");
	    ring_params.credits = config.lookup("network.credits");
	    ring_params.link_width = config.lookup("network.link_width");
	    ring_params.rc_method = RING_ROUTING;
	    ring_params.ni_up_credits = config.lookup("network.ni_up_credits");
	    ring_params.ni_upstream_buffer_size = config.lookup("network.ni_up_buffer");
	}
	else if(this->net_topology == SysBuilder_llp::TOPO_TORUS) {
	    torus_params.x_dim = this->x_dimension;
	    torus_params.y_dim = this->y_dimension;
	    torus_params.no_vcs = config.lookup("network.num_vcs");
	    torus_params.credits = config.lookup("network.credits");
	    torus_params.link_width = config.lookup("network.link_width");
	    torus_params.ni_up_credits = config.lookup("network.ni_up_credits");
	    torus_params.ni_upstream_buffer_size = config.lookup("network.ni_up_buffer");
	}
	else if(this->net_topology == SysBuilder_llp::TOPO_TORUS6P) {
	    torus6p_params.x_dim = this->x_dimension;
	    torus6p_params.y_dim = this->y_dimension;
	    torus6p_params.no_vcs = config.lookup("network.num_vcs");
	    torus6p_params.credits = config.lookup("network.credits");
	    torus6p_params.link_width = config.lookup("network.link_width");
	    torus6p_params.ni_up_credits = config.lookup("network.ni_up_credits");
	    torus6p_params.ni_upstream_buffer_size = config.lookup("network.ni_up_buffer");
	}

	COH_MSG_TYPE = config.lookup("network.coh_msg_type");
	MEM_MSG_TYPE = config.lookup("network.mem_msg_type");
	CREDIT_MSG_TYPE = config.lookup("network.credit_msg_type");


	//processor configuration
	//the node indices of processors are in an array, each value between 0 and MAX_NODES-1
	Setting& setting_proc = config.lookup("processor.node_idx");
	int num_proc = setting_proc.getLength(); //number of processors
	assert(num_proc >=1 && num_proc <= MAX_NODES);

	this->proc_node_idx_vec.resize(num_proc);
	//this->mc_node_idx_vec.resize(num_proc);

	for(int i=0; i<num_proc; i++) {
	    assert((int)setting_proc[i] >=0 && (int)setting_proc[i] < MAX_NODES);

	    proc_node_idx_set.insert((int)setting_proc[i]);
	    this->proc_node_idx_vec[i] = (int)setting_proc[i];

      //mc_node_idx_set.insert((int)setting_proc[i]);
	    //this->mc_node_idx_vec[i] = (int)setting_proc[i];
	}
	assert(proc_node_idx_set.size() == (unsigned)num_proc); //verify no 2 indices are the same

#if 1
	//memory controller configuration
	//the node indices of MC are in an array, each value between 0 and MAX_NODES-1
	Setting& setting_mc = config.lookup("mc.node_idx");
	int num_mc = setting_mc.getLength(); //number of mem controllers
	assert(num_mc >=1 && num_mc < MAX_NODES);

	this->mc_node_idx_vec.resize(num_mc);

	for(int i=0; i<num_mc; i++) {
	    assert((int)setting_mc[i] >=0 && (int)setting_mc[i] < MAX_NODES);
	    mc_node_idx_set.insert((int)setting_mc[i]);
	    this->mc_node_idx_vec[i] = (int)setting_mc[i];
	}
  cerr << mc_node_idx_set.size() << ", " << num_mc << endl;
	assert(mc_node_idx_set.size() == (unsigned)num_mc); //verify no 2 indices are the same

#if 0
	//verify MC indices are not used by processors
	for(int i=0; i<num_mc; i++) {
	    for(int j=0; j<num_proc; j++) {
	        assert(this->mc_node_idx_vec[i] != this->proc_node_idx_vec[j]);
	    }
	}
#endif
#endif
	MC_DOWNSTREAM_CREDITS = config.lookup("mc.downstream_credits");

    }
    catch (SettingNotFoundException e) {
	cout << e.getPath() << " not set." << endl;
	exit(1);
    }
    catch (SettingTypeException e) {
	cout << e.getPath() << " has incorrect type." << endl;
	exit(1);
    }

}


//====================================================================
//====================================================================
void SysBuilder_llp :: create_network(Clock& clock, Terminal_to_net_mapping* mapping, MySimLen* simLen, MyVnet* vn, int part)
{
    myRing = 0;
    myTorus = 0;
    myTorus6p = 0;

    if(this->net_topology == TOPO_RING) {
	myRing = topoCreator<NetworkPacket>::create_ring(clock, &(this->ring_params), mapping, simLen, vn, this->CREDIT_MSG_TYPE, 0, 0); //network on LP 0
    } else if (this->net_topology == TOPO_TORUS) {
	vector<int> node_lp;
	node_lp.resize(x_dimension*y_dimension);
	switch(part) {
	    case PART_1:
		for (int i = 0; i < x_dimension*y_dimension; i++) {
		    node_lp[i] = 0;
		}
		break;
	    case PART_2:
		for (int i = 0; i < x_dimension*y_dimension; i++) {
		    if(i < x_dimension*y_dimension/2)
			node_lp[i] = 0;
		    else
			node_lp[i] = 1;
		}
		break;
	    case PART_Y:
		for (int i = 0; i < x_dimension*y_dimension; i++) {
		    node_lp[i] = i / x_dimension;
		}
		break;
	    default:
		assert(0);
	}//switch
	myTorus = topoCreator<NetworkPacket>::create_torus(clock, &(this->torus_params), mapping, simLen, vn, this->CREDIT_MSG_TYPE, &node_lp); //network on LP 0
    } else {
	vector<int> node_lp;
	node_lp.resize(x_dimension*y_dimension);
	switch(part) {
	    case PART_1:
		for (int i = 0; i < x_dimension*y_dimension; i++) {
		    node_lp[i] = 0;
		}
		break;
	    case PART_2:
		for (int i = 0; i < x_dimension*y_dimension; i++) {
		    if(i < x_dimension*y_dimension/2)
			node_lp[i] = 0;
		    else
			node_lp[i] = 1;
		}
		break;
	    case PART_Y:
		for (int i = 0; i < x_dimension*y_dimension; i++) {
		    node_lp[i] = i / x_dimension;
		}
		break;
	    default:
		assert(0);
	}//switch
	myTorus6p = topoCreator<NetworkPacket>::create_torus6p(clock, &(this->torus6p_params), mapping, simLen, vn, this->CREDIT_MSG_TYPE, &node_lp); //network on LP 0
    }

}



//====================================================================
//====================================================================
void SysBuilder_llp :: connect_components(const Node_cid_llp node_cids[])
{
    const std::vector<CompId_t>& ni_cids = (this->myRing != 0) ? this->myRing->get_interface_id() : ((this->myTorus != 0) ? this->myTorus->get_interface_id() : this->myTorus6p->get_interface_id());

    //==========================================================================
    //Now connect the components
    //==========================================================================
    const std::vector<GenNetworkInterface<NetworkPacket>*>& nis = (myRing != 0) ? myRing->get_interfaces() : ((myTorus != 0) ? myTorus->get_interfaces() : myTorus6p->get_interfaces());
    const std::vector<SimpleRouter*>& routers = (myRing != 0) ? myRing->get_routers() : ((myTorus != 0) ? myTorus->get_routers() : myTorus6p->get_routers());

    for(int i=0; i<this->MAX_NODES; i++) {
#ifdef FORECAST_NULL 
if(nis[i] != 0) {
    manifold::kernel::Clock::Master().register_output_predictor(nis[i], &GenNetworkInterface<NetworkPacket>::do_output_to_terminal_prediction);
}

if(routers[i] != 0) {
    if (routers[i]->get_cross_lp_flag())
        manifold::kernel::Clock::Master().register_output_predictor(routers[i], &SimpleRouter::do_output_to_router_prediction);
}
#endif

        if(node_cids[i].type == CORE_MC_NODE) {
	    #if 1
	    //some sanity check
	    //qsimlib_core_t* proc = Component :: GetComponent<qsimlib_core_t>(node_cids[i].proc_cid);
	    spx_core_t* proc = Component :: GetComponent<spx_core_t>(node_cids[i].proc_cid);
	    if(proc != 0) {
		MESI_LLP_cache* llp_cache = Component :: GetComponent<MESI_LLP_cache>(node_cids[i].l1_cache_cid);
		MESI_LLS_cache* lls_cache = Component :: GetComponent<MESI_LLS_cache>(node_cids[i].l2_cache_cid);
		assert(proc->core_id == llp_cache->get_node_id());
		assert(proc->core_id == lls_cache->get_node_id());

		if(nis[i] != 0) { //only true when there is only 1 LP
		    assert(llp_cache->get_node_id() == (int)nis[i]->get_id());
		}
	    }
	    #endif
	
	    //proc to L1 cache
	    Manifold :: Connect(node_cids[i].proc_cid, spx_core_t::OUT_TO_CACHE,
				node_cids[i].l1_cache_cid, MESI_LLP_cache::PORT_PROC,
				&MESI_LLP_cache::handle_processor_request<cache_request_t>, 1);
	    //L1 cache to proc
	    Manifold :: Connect(node_cids[i].l1_cache_cid, MESI_LLP_cache::PORT_PROC,
				node_cids[i].proc_cid, spx_core_t::IN_FROM_CACHE,
				&spx_core_t::handle_cache_response, 1);

	    //Mux to network interface
	    Manifold :: Connect(node_cids[i].mux_cid, MuxDemux::PORT_NET,
				ni_cids[i*2], GenNetworkInterface<NetworkPacket>::TERMINAL_PORT,
				&GenNetworkInterface<NetworkPacket>::handle_new_packet_event, 1);
	    //Network interface to Mux
	    Manifold :: Connect(ni_cids[i*2], GenNetworkInterface<NetworkPacket>::TERMINAL_PORT,
				node_cids[i].mux_cid, MuxDemux::PORT_NET,
				&MuxDemux::handle_net<Mem_msg>, 1);

	    //Inside the network, routing is based on the interface IDs. The adapters have
	    //their own IDs. When one adapter (or its client to be more specific) sends
	    //data to another adapter (client), it uses adapter IDs. But it needs to know
	    //the destination's network interface ID so the network can deliver the packet.
	    //Therefore, the adapter layer should maintain a mapping between adapter IDs
	    //and network interface IDs.
	    //This is similar to the mapping between IP addresses and MAC addresses, except
	    //here the mapping is static and nothing like ARP is involved.

	    //In this simple network, we simplify things further such that the network interface
	    //IDs are the same as the adapter IDs, therefore the mapping becomes M(X)=X. So
	    //we don't need to keep any mapping. Sending to adapter X would be sending to
	    //network interface X.

	    #if 1
	    Controller* mc = Component :: GetComponent<Controller>(node_cids[i].mc_cid);
	    if(mc != 0 ) {
		if(nis[i] != 0) { //only true when both are in the same LP
		    assert(mc->get_nid() == (int)nis[i]->get_id());
		}
	    }
	    #endif
	    //mc to interface
	    Manifold :: Connect(node_cids[i].mc_cid, Controller::PORT0,
				ni_cids[i*2+1], GenNetworkInterface<NetworkPacket>::TERMINAL_PORT,
				&GenNetworkInterface<NetworkPacket>::handle_new_packet_event, 1);
	    //interface to mc
	    Manifold :: Connect(ni_cids[i*2+1], GenNetworkInterface<NetworkPacket>::TERMINAL_PORT,
				node_cids[i].mc_cid, Controller::PORT0,
				&Controller::handle_request<Mem_msg>, 1);

	} else if (node_cids[i].type == CORE_NODE) {
	    #if 1
	    //some sanity check
	    //qsimlib_core_t* proc = Component :: GetComponent<qsimlib_core_t>(node_cids[i].proc_cid);
	    spx_core_t* proc = Component :: GetComponent<spx_core_t>(node_cids[i].proc_cid);
	    if(proc != 0) {
		MESI_LLP_cache* llp_cache = Component :: GetComponent<MESI_LLP_cache>(node_cids[i].l1_cache_cid);
		MESI_LLS_cache* lls_cache = Component :: GetComponent<MESI_LLS_cache>(node_cids[i].l2_cache_cid);
		assert(proc->core_id == llp_cache->get_node_id());
		assert(proc->core_id == lls_cache->get_node_id());

		if(nis[i] != 0) { //only true when there is only 1 LP
		    assert(llp_cache->get_node_id() == (int)nis[i]->get_id());
		}
	    }
	    #endif
	
	    //proc to L1 cache
	    Manifold :: Connect(node_cids[i].proc_cid, spx_core_t::OUT_TO_CACHE,
				node_cids[i].l1_cache_cid, MESI_LLP_cache::PORT_PROC,
				&MESI_LLP_cache::handle_processor_request<cache_request_t>, 1);
	    //L1 cache to proc
	    Manifold :: Connect(node_cids[i].l1_cache_cid, MESI_LLP_cache::PORT_PROC,
				node_cids[i].proc_cid, spx_core_t::IN_FROM_CACHE,
				&spx_core_t::handle_cache_response, 1);

	    //Mux to network interface
	    Manifold :: Connect(node_cids[i].mux_cid, MuxDemux::PORT_NET,
				ni_cids[i*2], GenNetworkInterface<NetworkPacket>::TERMINAL_PORT,
				&GenNetworkInterface<NetworkPacket>::handle_new_packet_event, 1);
	    //Network interface to Mux
	    Manifold :: Connect(ni_cids[i*2], GenNetworkInterface<NetworkPacket>::TERMINAL_PORT,
				node_cids[i].mux_cid, MuxDemux::PORT_NET,
				&MuxDemux::handle_net<Mem_msg>, 1);

	} else { //Empty node
	    //do nothing
	}

    }
}




//====================================================================
//====================================================================
void SysBuilder_llp :: config_cache_settings(manifold::uarch::DestMap* l2_map, manifold::uarch::DestMap* mc_map)
{
    assert(L1_downstream_credits > 0 && L2_downstream_credits > 0);

    l1_settings.l2_map = l2_map;
    l1_settings.mshr_sz = L1_MSHR_SIZE;
    l1_settings.downstream_credits = L1_downstream_credits;

    l2_settings.mc_map = mc_map;
    l2_settings.mshr_sz = L2_MSHR_SIZE;
    l2_settings.downstream_credits = L2_downstream_credits;

    L1_cache :: Set_msg_types(COH_MSG_TYPE, CREDIT_MSG_TYPE);
    L2_cache :: Set_msg_types(COH_MSG_TYPE, MEM_MSG_TYPE, CREDIT_MSG_TYPE);

    Controller :: Set_msg_types(MEM_MSG_TYPE, CREDIT_MSG_TYPE);

}



#if 0
//====================================================================
//====================================================================
void SysBuilder_llp :: register_components_with_clock(const Node_cid_llp node_cids[])
{
    for(int i=0; i<this->MAX_NODES; i++) {
        if(node_cids[i].type == CORE_NODE) {
	    qsimlib_core_t* proc = Component :: GetComponent<qsimlib_core_t>(node_cids[i].proc_cid);
	    assert(proc);
	    Clock :: Register((core_t*)proc, &qsimlib_core_t::tick, (void(core_t::*)(void))0);
	}
    }

    for(int i=0; i<this->MAX_NODES; i++) {
        if(node_cids[i].type == CORE_NODE) {
	}
	else if(node_cids[i].type == MC_NODE) {
	    Controller* mc = Component :: GetComponent<Controller>(node_cids[i].mc_cid);
	    if(mc)
		mc->print_config(cout);
	}
    }
}


#endif

//====================================================================
//====================================================================
void SysBuilder_llp :: print_stats(const Node_cid_llp node_cids[])
{
    for(int i=0; i<this->MAX_NODES; i++) {
        if(node_cids[i].type == CORE_MC_NODE) {
	    spx_core_t* proc = Component :: GetComponent<spx_core_t>(node_cids[i].proc_cid);
	    if(proc) {
		proc->print_stats();
		//proc->print_stats(cout);
	    }
#if 1
            MESI_LLP_cache* l1 = Component :: GetComponent<MESI_LLP_cache>(node_cids[i].l1_cache_cid);
	    MESI_LLS_cache* l2 = Component :: GetComponent<MESI_LLS_cache>(node_cids[i].l2_cache_cid);
	    if(l1) {
		l1->print_stats(cerr);
		l2->print_stats(cerr);
	    }

	    Controller* mc = Component :: GetComponent<Controller>(node_cids[i].mc_cid);
	    if(mc)
		mc->print_stats(cerr);
#endif
	}
    }

#if 1
    if(myRing)
	myRing->print_stats(cout);
    else if(myTorus)
	myTorus->print_stats(cout);
    else
	myTorus6p->print_stats(cerr);
#endif
    Manifold :: print_stats(cout);
}


