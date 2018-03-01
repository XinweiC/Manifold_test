#include <iostream>
#include <list>
#include <stdlib.h>
#include "iris/genericTopology/genericTopoCreator.h"
#include "iris/interfaces/genericIrisInterface.h"
#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace std;

using namespace manifold::kernel;
using namespace manifold::iris;


// This class defines data sent between terminals.
const unsigned DATASIZE = 8;

class TerminalData {
public:
    enum { REQ, REPLY};

    int type;
    int src;
    int dst;
    unsigned data[DATASIZE];  //do not use containers
    
    int get_type() { return type; }
    void set_type(int t) { type = t; }
    int get_src() { return src; }
    int get_dst() { return dst; }
    int get_src_port() { return 0; }
    int get_dst_port() { return 0; }
    void set_dst_port(int dp) {} //do nothing
};


// This class calculates simulated length of packets.
class MySimLen : public manifold::iris::SimulatedLen<TerminalData> {
public:
    int get_simulated_len(TerminalData* pkt)
    {
        if(pkt->type == TerminalData::REQ)
            return 18;
        else
            return 4;
    }
};


//This class assigns packets to virtual networks based on packet type.
class MyVnet : public manifold::iris::VnetAssign<TerminalData> {
public:
    int get_virtual_net(TerminalData* pkt)
    {
        if(pkt->type == TerminalData::REQ)
            return 0;
        else
            return 1;
    }
};


const int NI_CREDIT_TYPE = 123; //msg type for network interface upstream credits.


//This class emulates a terminal.
class Mockterminal : public manifold::kernel::Component {
public:
    enum {PORT_NI=0};

    Mockterminal(unsigned id, unsigned n_nodes, unsigned total_pkts) :
        m_id(id), m_NODES(n_nodes), m_TOTAL_PKTS(total_pkts)
    {   
        m_pkt_count = 0;
    }
    
    void tick ()
    {  
        if(m_pkt_count < m_TOTAL_PKTS) {
        //cout << "Terminal " << m_id << " sending\n";
            TerminalData* td = new TerminalData;
            td->type = TerminalData::REQ;
            td->src = m_id;

            while((td->dst = random() % m_NODES) == m_id); //randomly pick a destination, but not self.

            Send(PORT_NI , td);
            m_pkt_count++;   
        } 
    }
    
    
    void handle_incoming_pkt(int, TerminalData* td)
    {
        if(td->type == TerminalData::REQ) {
            m_pkts.push_back (*td);
            //send a credit back; reuse td
            td->type = NI_CREDIT_TYPE;
            Send(PORT_NI , td);
        }
        else if(td->type == NI_CREDIT_TYPE) {
            //do nothing
            delete td;
        }
        else {
            assert(0);
        }
    }    

private:
    unsigned m_id;
    unsigned m_pkt_count; //number of pkts that have been sent
    const unsigned m_NODES; //number of nodes in the network
    const unsigned m_TOTAL_PKTS; //pkts to send to each of the other nodes

    list<TerminalData> m_pkts; //received pkts

};



const int MASTER_CLOCK_HZ = 1000;
Clock MasterClock(MASTER_CLOCK_HZ);


int main(int argc, char** argv)
{

    //check command-line arguments
    if(argc != 3) {
        cerr << "Usage: " << argv[0] << " <num_nodes>  <num_packets>" << endl
             << "   num_nodes: number of nodes in the network." << endl
             << "   num_packets: max number of packets a node can send." << endl;
        exit(1);
    }

    const unsigned NUM_NODES = atoi(argv[1]);
    if(NUM_NODES < 2 || NUM_NODES > 256) {
        cerr << "Number of nodes must be between 2 and 256." << endl;
        exit(1);
    }

    const unsigned NUM_PKTS = atoi(argv[2]);
    if(NUM_PKTS < 1) {
        cerr << "Number of packets cannot be less than 1." << endl;
        exit(1);
    }


    Manifold :: Init(argc, argv);

    //the paramters for ring network
    ring_init_params rp;
    rp.no_nodes = NUM_NODES;
    rp.no_vcs = 4; //number of virtual channels
    rp.credits = 20; //number of credits for each virtual channel
    rp.rc_method = RING_ROUTING;
    rp.link_width = 128;
    rp.ni_up_credits = 10;
    rp.ni_upstream_buffer_size = 5;
        
    Clock& clk = MasterClock;
        
    //creat the ring network
    Simple_terminal_to_net_mapping* mapping = new Simple_terminal_to_net_mapping();
    MySimLen* simLen = new MySimLen();
    MyVnet* vnet = new MyVnet();

    Ring<TerminalData>* myRing = topoCreator<TerminalData>::create_ring(clk, &rp, mapping, simLen, vnet, NI_CREDIT_TYPE, 0, 0);

        
    //creat the terminals
    vector<CompId_t> term_ids(NUM_NODES);        
    vector<Mockterminal*> terms(NUM_NODES); //pointer to terminal objects

    for (unsigned i=0; i<NUM_NODES; i++) {
        term_ids[i] = Component :: Create<Mockterminal> (0, i, NUM_NODES, NUM_PKTS);
        terms[i] = Component :: GetComponent<Mockterminal>(term_ids[i]);
        if (terms[i])
            Clock::Register(clk, terms[i], &Mockterminal::tick, (void (Mockterminal::*)()) 0);
    }
        
    //get the network interfaces' component IDs for connection.
    const vector <manifold::kernel::CompId_t>& inf_ids = myRing->get_interface_id();

    //connect terminal and interface
    const Ticks_t LATENCY = 1;
    for (unsigned i=0; i<NUM_NODES; i++) {
        Manifold::Connect(term_ids[i], Mockterminal::PORT_NI, inf_ids[i], 
                      GenNetworkInterface<TerminalData>::DATAFROMTERMINAL,
                      &GenNetworkInterface<TerminalData>::handle_new_packet_event, LATENCY);
        Manifold::Connect(inf_ids[i], GenNetworkInterface<TerminalData>::DATATOTERMINAL, term_ids[i], 
                      Mockterminal::PORT_NI, &Mockterminal::handle_incoming_pkt, LATENCY);               
    }
        
    Manifold::StopAt(10000);
    Manifold::Run();
        
    myRing->print_stats(cout);

    Manifold :: Finalize();

}
