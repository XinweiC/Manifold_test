//! @file spx_llp.cc
//! @brief This program builds a simple simulator of multicore systems. Its purpose is to
//! demonstrate how to build such a simulator using the Manifold framework.
//! The multicore system is illustrated as follows:
//!
//! @verbatim
//!                                              ------------      ------------
//!      -----------       -----------          |   memory   |    |   memory   |
//!     | processor |     | processor |         | controller |    | controller |
//!      -----------       -----------           ------------      ------------
//!           |                 |                     |                  |
//!        ----------           ----------            |                  |
//!       | L1 cache |         | L1 cache |           |                  |
//!        ----------           ----------            |                  |
//!         |     |              |     |              |                  |
//!  ----------   |       ----------   |              |                  |
//! | L2 cache |  |      | L2 cache |  |              |                  |
//!  ----------   |       ----------   |              |                  |
//!       |       |            |       |              |                  |
//!        ----   |             ----   |              |                  |
//!            |  |                 |  |              |                  |
//!           -----                -----              |                  |
//!          | mux |              | mux |             |                  |
//!             |                    |                |                  |
//!  ---------------------------------------------------------------------------
//!       ------------         ------------       ------------      ------------ 
//!      | NetIntface |       | NetIntface |     | NetIntface |    | NetIntface |  
//!       ------------         ------------       ------------      ------------ 
//!  ---------------------------------------------------------------------------
//! @endverbatim
//!
//!
//! The components used in this program are:
//! - spx_core_t: a processor model of spx.
//!                  It is built with the QSim library and uses QSim to get instructions.
//! - CaffDRAM: a memory controller model.
//! - mcp-cache: a coherence cache model.
//! - Iris: a cycle level interconnection network model.
//!
//! NOTE:
//! Since the processor model is built with the Qsim library, and all processors should
//! share the same Qsim OS domain object, in this program all the processors must be
//! in the same LP. In fact, we only allow this program to be run with 1 LP.
//!
//! In this program the configuration for the components is provided by a configuration
//! file which is parsed with libconfig++. An example can be found in conf4x1.cfg.
//! 
//! To run this program, type:
//! @code
//! mpirun -np 1 spx_llp <conf_file>  <spx_conf_file>  <state_file>  <benchmark_tar_file>
//! @endcode
//! <conf_file>: the name of the configuration file.
//! <sepx_conf_file>: the name of the spx configuration file.
//! <state_file>: the name of the QSim state file.
//! <benchmark_tar_file>: the name of the benchmark tar file.
//!
//!
//!
#include "sysBuilder_llp.h"
#include "common.h"
#include "ei_wrapper.h"
#include "kernel/clock.h"
#include "kernel/component.h"
#include "kernel/manifold.h"
#include "spx/core.h"
#include "spx/interrupt_handler.h"
//#include "iris/genericTopology/genericTopoCreator.h"
#include "mcp-cache/lp_lls_unit.h"
#include "CaffDRAM/Controller.h"
#include "CaffDRAM/McMap.h"
#include "qsim.h"
#include "qsim-load.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <libconfig.h++>
#include <string>
#include <assert.h>
#include "mpi.h"

#define USE_EI


using namespace std;
using namespace manifold::kernel;
using namespace manifold::uarch;
using namespace manifold::spx;
using namespace manifold::mcp_cache_namespace;
using namespace manifold::iris;
using namespace manifold::caffdram;
using namespace manifold::ei_wrapper;
using namespace libconfig;
using namespace EI;

EI::energy_introspector_t *energy_introspector;
double sampling_period = 1e-5;

int main(int argc, char** argv)
{
    if(argc != 7) {
        cerr << "Usage: mpirun -np <NP> " << argv[0] << " <config_file>  <spx_config_file> <qsim_type> <state_file> <EI_config_file> <benchmark_tar_file>" << endl;
        exit(1);
    }


    //==========================================================================
    // Read and parse libconfig input.
    //==========================================================================
    Config config;
    try {
	config.readFile(argv[1]);
        config.setAutoConvert(true);
    }
    catch (FileIOException e) {
        cerr << "Cannot read configuration file " << argv[1] << endl;
	exit(1);
    }
    catch (ParseException e) {
        cerr << "Cannot parse configuration file " << argv[1] << endl;
	exit(1);
    }


    // LLP System Builder
    SysBuilder_llp sysBuilder;

    // Init EI
#ifdef USE_EI
    energy_introspector = new energy_introspector_t(argv[5]);
#endif

    //==========================================================================
    // Determine network topology
    //==========================================================================
    sysBuilder.config_network_topology(config);



    //==========================================================================
    // Configuration parameters
    //==========================================================================
    sysBuilder.config_components(config);


    //==========================================================================
    // Manifold init
    //==========================================================================
    Manifold::Init(argc, argv);

    //==========================================================================
    // Initialize Qsim library.
    //==========================================================================
    Qsim::OSDomain* qsim_osd = NULL;
    if(!strcasecmp(argv[3],"qsim_lib"))
    {
        std::cout << "Qsim_lib Loading state..." << endl;
        qsim_osd = new Qsim::OSDomain(argv[4]);
        qsim_osd->connect_console(std::cout);
        std::cout << "        Loading app " << argv[6] << " ..." << endl;
        Qsim::load_file(*qsim_osd, argv[6]);
        std::cout << "        Finished loading app" << endl;

    }
    else
    {
      std::cout << "unknown qsim_type " << argv[3] << "(qsim_lib or qsim_client)" << endl;
      exit(1);
    }

#if 1
    {
      int fastfwd_cores = 0;
      uint64_t fastfwd_count = 0;
      uint64_t interrupt_count = 0;
      float utilization = 0.99;
      float utilization_progress = 0.0;
      int fastfwd_step = 100;
         
      while(utilization_progress < utilization) {
        fastfwd_count++; interrupt_count++;
        fastfwd_cores = 0;
        for(int j = 0; j < sysBuilder.MAX_NODES; j++) {
          qsim_osd->run(j,fastfwd_step);
          //qsim_osd->idle(j) && (qsim_osd->get_prot(j) != Qsim::OSDomain::PROT_KERN)) { fastfwd_cores++; }
          if(!qsim_osd->idle(j)) { fastfwd_cores++; }
          if((j == 0)&&(interrupt_count*fastfwd_step == 100000)) {
            qsim_osd->timer_interrupt();
            interrupt_count = 0;
	        }

          if((j == (sysBuilder.MAX_NODES - 1)) && (fastfwd_count > 5000000)) {
            fprintf(stderr,"problem app\n");
            exit(1);
          }
        } 

        if((float)fastfwd_cores/sysBuilder.MAX_NODES > utilization_progress) {
         utilization_progress = (float)fastfwd_cores/sysBuilder.MAX_NODES;
         fprintf(stderr,"\t\t%3.1lf%% cores are utilized ...\n",utilization_progress*100.0);
        }

        if((fastfwd_count*fastfwd_step) % 1000000 == 0) {
          fprintf(stderr,"\t\t%3.0lfM instructions are fastforwarded (%3.1lf%%) ...\n",(double)(fastfwd_count*fastfwd_step)/1000000.0,utilization_progress*100.0);
        }

      }

      if(fastfwd_count)
        fprintf(stderr,"\t\t%3.1lf%% utilization fast forwarding is done after running %lu instructions\n",utilization*100.0,(fastfwd_count-1)*fastfwd_step);
	    
      fprintf(stderr,"\n");      
    }   
#endif

    //==========================================================================
    // Configure distributed clock network.
    //==========================================================================


    vector<Clock*> node_clock;
    node_clock.reserve(sysBuilder.MAX_NODES);

    Clock network_clock((double)config.lookup("network_clock_frequency"));
    cout << "Network: clock_frequency = " << network_clock.freq << endl;

    try {

      Setting &clock_setting = config.lookup("distributed_clock_frequency");
      assert(clock_setting.getLength() == sysBuilder.MAX_NODES);

      for(int c = 0; c < clock_setting.getLength(); c++) 
      {
        node_clock[c] = new Clock((double)clock_setting[c]);
        cout << "Node " << c << ": clock_frequency = " << node_clock[c]->freq << endl;
      }
    }
    catch (SettingNotFoundException e) {
      cout << e.getPath() << " not set." << endl;
      exit(1);
    }
    catch (SettingTypeException e) {
      cout << e.getPath() << " has incorrect type." << endl;
      exit(1);
    }

    Clock *interrupt_handler_clock = new Clock(1000.0);
    interrupt_handler_t *interrupt_handler = Component :: GetComponent<interrupt_handler_t>(Component::Create<interrupt_handler_t>(0, interrupt_handler_clock, qsim_osd));
    //interrupt_handler_clock->Register<interrupt_handler_t>(*interrupt_handler_clock,interrupt_handler,&interrupt_handler_t::tick,(void(interrupt_handler_t::*)(void))0);

    // need storage for component IDs for connecting components
    Node_cid_llp node_cids[sysBuilder.MAX_NODES];

    //==========================================================================
    // Create network.
    //==========================================================================
    //Terminal address to network address mapping
    //Using Simple_terminal_to_net_mapping means node ids must be 0 to MAX_NODES-1.
    Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
    MySimLen* simLen = new MySimLen(sysBuilder.l1_cache_parameters.block_size, sysBuilder.COH_MSG_TYPE, sysBuilder.MEM_MSG_TYPE, sysBuilder.CREDIT_MSG_TYPE);
    MyVnet* vnet = new MyVnet(sysBuilder.COH_MSG_TYPE, sysBuilder.MEM_MSG_TYPE, sysBuilder.CREDIT_MSG_TYPE);

    sysBuilder.create_network(network_clock, mapping, simLen, vnet);


    CaffDramMcMap* mc_map = new CaffDramMcMap(sysBuilder.mc_node_idx_vec, sysBuilder.dram_settings);
    PageBasedMap* l2_map = new PageBasedMap(sysBuilder.mc_node_idx_vec, 12); //page size = 2^12
    sysBuilder.config_cache_settings(l2_map, mc_map);

//#define REDIRECT_COUT_

#ifdef REDIRECT_COUT
    // create a file into which to write debug/stats info.
    int Mytid;
    MPI_Comm_rank(MPI_COMM_WORLD, &Mytid);
    char buf[10];
    sprintf(buf, "DBG_LOG%d", Mytid);
    ofstream DBG_LOG(buf);

    //redirect cout to file.
    std::streambuf* cout_sbuf = std::cout.rdbuf(); // save original sbuf
    std::cout.rdbuf(DBG_LOG.rdbuf()); // redirect cout
#endif



    //==========================================================================
    // Create manifold components.
    //==========================================================================
    //Node ID is the same as its node index: between 0 and MAX_NODES-1
    list<LP_LLS_unit*> lp_lls_units;

    Clock dram_clock(3000000000);	        
    
    LpId_t node_lp = 0;

    int cpuid=0;
    for(int i=0; i<sysBuilder.MAX_NODES; i++) {
      if(sysBuilder.proc_node_idx_set.find(i) != sysBuilder.proc_node_idx_set.end()) { //proc & MC node
        node_cids[i].type = CORE_MC_NODE;
        node_cids[i].proc_cid = Component :: Create<spx_core_t>(node_lp, node_clock[i], i, qsim_osd, argv[2], cpuid++);

        //LP_LLS_unit* unit = new LP_LLS_unit(node_lp, i, sysBuilder.l1_cache_parameters, sysBuilder.l2_cache_parameters, sysBuilder.l1_settings, sysBuilder.l2_settings, *(node_clock[i]), sysBuilder.CREDIT_MSG_TYPE); 
        LP_LLS_unit* unit = new LP_LLS_unit(node_lp, i, sysBuilder.l1_cache_parameters, sysBuilder.l2_cache_parameters, sysBuilder.l1_settings, sysBuilder.l2_settings, *(node_clock[i]), sysBuilder.CREDIT_MSG_TYPE); 

        node_cids[i].l1_cache_cid = unit->get_llp_cid();
        node_cids[i].l2_cache_cid = unit->get_lls_cid();
        node_cids[i].mux_cid = unit->get_mux_cid();
        lp_lls_units.push_back(unit);
        node_cids[i].mc_cid = Component :: Create<Controller>(node_lp, &dram_clock, i, sysBuilder.dram_settings, sysBuilder.MC_DOWNSTREAM_CREDITS);
        
      } else if(sysBuilder.proc_node_idx_set.find(i) != sysBuilder.proc_node_idx_set.end()) { //proc node

        node_cids[i].type = CORE_NODE;
        node_cids[i].proc_cid = Component :: Create<spx_core_t>(node_lp, node_clock[i], i, qsim_osd, argv[2], cpuid++);

        //LP_LLS_unit* unit = new LP_LLS_unit(node_lp, i, sysBuilder.l1_cache_parameters, sysBuilder.l2_cache_parameters, sysBuilder.l1_settings, sysBuilder.l2_settings, *(node_clock[i]), sysBuilder.CREDIT_MSG_TYPE); 
        LP_LLS_unit* unit = new LP_LLS_unit(node_lp, i, sysBuilder.l1_cache_parameters, sysBuilder.l2_cache_parameters, sysBuilder.l1_settings, sysBuilder.l2_settings, *(node_clock[i]), sysBuilder.CREDIT_MSG_TYPE); 

        node_cids[i].l1_cache_cid = unit->get_llp_cid();
        node_cids[i].l2_cache_cid = unit->get_lls_cid();
        node_cids[i].mux_cid = unit->get_mux_cid();
        lp_lls_units.push_back(unit);
      }
      else {
        node_cids[i].type = EMPTY_NODE;
      }
    }

    for(int i=0; i<sysBuilder.MAX_NODES; i++) {
	if(node_cids[i].type == CORE_MC_NODE)
	    cout << i << "  core mc node" << endl;
	else
	    cout << i << "  empty node" << endl;
    }

    //==========================================================================
    // Connect the components.
    //==========================================================================

    sysBuilder.connect_components(node_cids);

#ifdef USE_EI
    vector<ei_wrapper_t*> ei_device;
    ei_device.reserve(sysBuilder.MAX_NODES);
    for(int i=0; i<sysBuilder.MAX_NODES; i++) 
        if((node_cids[i].type == CORE_MC_NODE) || (node_cids[i].type == CORE_NODE)) {
	    manifold::spx::spx_core_t* proc_global = (spx_core_t*) Component :: GetComponent<spx_core_t>(node_cids[i].proc_cid);
	    //manifold::mcp_cache_namespace::MESI_LLP_cache* l1_global = (MESI_LLP_cache*) Component :: GetComponent<MESI_LLP_cache>(node_cids[i].l1_cache_cid);
	    //manifold::mcp_cache_namespace::MESI_LLS_cache* l2_global = (MESI_LLS_cache*) Component :: GetComponent<MESI_LLS_cache>(node_cids[i].l2_cache_cid);
	    if(proc_global && l1_global && l2_global) {
                ei_device[i] = new ei_wrapper_t(node_clock[i], energy_introspector, proc_global->pipeline->counters, proc_global->ipa, l1_global->cache_counter, l2_global->cache_counter, sampling_period, sysBuilder.MAX_NODES, i); 
            } else {
                cerr << "parsing sim hierachy fails" << endl;
                exit(1);
            }


	}
#endif     

    //==========================================================================
    // Manifold runs.
    //==========================================================================
    Manifold::StopAt(sysBuilder.STOP);
    Manifold::Run();

#ifdef REDIRECT_COUT
    sysBuilder.print_stats(node_cids);
    std::cout.rdbuf(cout_sbuf);
#endif

    sysBuilder.print_stats(node_cids);

    delete interrupt_handler_clock;

    //==========================================================================
    // Manifold finishes.
    //==========================================================================
    Manifold :: Finalize();

    while(node_clock.size())
    {
      delete *node_clock.begin();
      node_clock.erase(node_clock.begin());
    }
}
