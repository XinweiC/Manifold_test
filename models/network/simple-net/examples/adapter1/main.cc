//! This program demonstrates how to use the simple-net network model.
//! It creates a network of N nodes, and N clients and adapters. Each client
//! is connected to its adapter which is connected to a network interface.
//! The simple client address to network interface address mapping is used, which
//! means the client must have the same ID as its network interface.
//! All the clients are registered with the clock. At each falling edge, a client
//! randomly picks a destination and sends out a packet.
//!
//! To run the program, type
//! mpirun -np <NP> adapter1 
//!    where <NP> is the number of MPI tasks, and must be 1, 2, or 5 (= number of nodes + 1).
//!
#include "kernel/clock.h"
#include "kernel/component.h"
#include "kernel/manifold.h"
#include "client.h"
#include "simple-net/simpleNetAdapter.h"
#include "simple-net/network.h"
#include "simple-net/networkInterface.h"
#include <stdlib.h>
#include <iostream>

using namespace std;
using namespace manifold::kernel;
using namespace manifold::simple_net;

#define X_DIM 2
#define Y_DIM 2


int main(int argc, char** argv)
{
    Clock myClock(1000);

    Manifold::Init(argc, argv);

    const int NUM_NODES = X_DIM * Y_DIM;

    int N_LPs = 1; //number of LPs
    MPI_Comm_size(MPI_COMM_WORLD, &N_LPs);
    cout << "Number of LPs = " << N_LPs << endl;
    if(N_LPs != 1 && N_LPs !=2 && N_LPs != NUM_NODES + 1) {
        cerr << "Number of LPs must be 1, 2, or " << NUM_NODES + 1 << "!" << endl;
	exit(1);
    }

    // create all the components.
    CompId_t* client_cids = new CompId_t[NUM_NODES]; //required for connecting components
    CompId_t* adapter_cids = new CompId_t[NUM_NODES]; //required for connecting components

    MeshNetwork* myNetwork = new MeshNetwork(X_DIM, Y_DIM, 0); //network creates all the network interfaces.

    Simple_adapter_to_net_mapping* mapping = new Simple_adapter_to_net_mapping();

    if(N_LPs <= 2) {
	LpId_t node_lp = (N_LPs == 1) ? 0 : 1;
	for(int i=0; i<NUM_NODES; i++) {
	    client_cids[i] = Component :: Create<Client>(node_lp, i);
	    adapter_cids[i] = Component :: Create<SimpleNetAdapter<ClientData> >(node_lp, i, mapping);
	}
    }
    else {
	for(int i=0; i<NUM_NODES; i++) {
	    client_cids[i] = Component :: Create<Client>(i+1, i);
	    adapter_cids[i] = Component :: Create<SimpleNetAdapter<ClientData> >(i+1, i, mapping);
	}
    }

    Client :: Set_numNodes(NUM_NODES);//need to know the number of nodes in order to randomly select a destination

    std::vector<CompId_t>& ni_cids = myNetwork->get_interfaceCids();

    cout << "Clients: ";
    for(int i=0; i<NUM_NODES; i++) {
        cout << i << " (LP " << Component :: GetComponentLP(client_cids[i]) << ")  ";
    }
    cout << endl;

    //register clients with clock.
    for(int i=0; i<NUM_NODES; i++) {
        Client* c = Component :: GetComponent<Client>(client_cids[i]);
        if(c)
	    Clock::Register(c, &Client::tick, &Client::tock);
    }

    //The following variables are used to verify some basic assumptions.
    // 1. client and the terminal it connects to have the same node id.
    // 2. terminal's node id and the id of the network interface to which
    //    it is connected are the same. (However, this can only be verified
    //    if the terminal and the NI are in the same LP)
    Client** clients = new Client*[NUM_NODES];
    SimpleNetAdapter<ClientData>** adapters = new SimpleNetAdapter<ClientData>*[NUM_NODES];
    std::vector<NetworkInterface*>& nis = myNetwork->get_interfaces();
    for(int i=0; i<NUM_NODES; i++) {
        clients[i] = Component :: GetComponent<Client>(client_cids[i]);
        adapters[i] = Component :: GetComponent<SimpleNetAdapter<ClientData> >(adapter_cids[i]);
    }

    //Now connect the components
    for(int i=0; i<NUM_NODES; i++) {
        if(clients[i]) {
	    assert(adapters[i] != 0);
	    assert(clients[i]->get_nid() == adapters[i]->get_aid());
	}
	
        //client to adapter
        Manifold :: Connect(client_cids[i], Client::OUT_TO_NET,
	                    adapter_cids[i], SimpleNetAdapter<ClientData>::IN_FROM_CLIENT,
			    &SimpleNetAdapter<ClientData>::handle_incoming_from_client, 1);
        //adapter to client
        Manifold :: Connect(adapter_cids[i], SimpleNetAdapter<ClientData>::OUT_TO_CLIENT,
	                    client_cids[i], Client::IN_FROM_NET,
			    &Client::handle_incoming_data, 1);
        //adapter to interface
        Manifold :: Connect(adapter_cids[i], SimpleNetAdapter<ClientData>::OUT_TO_NET,
	                    ni_cids[i], NetworkInterface::IN_FROM_TERMINAL,
			    &NetworkInterface::handle_new_packet, 1);
        //interface to adapter
        Manifold :: Connect(ni_cids[i], NetworkInterface::OUT_TO_TERMINAL,
	                    adapter_cids[i], SimpleNetAdapter<ClientData>::IN_FROM_NET,
			    &SimpleNetAdapter<ClientData>::handle_incoming_from_net, 1);

	//Inside the network, routing is based on the interface IDs. The terminals have
	//their own IDs. When one terminal (or its client to be more specific) sends
	//data to another terminal (client), it uses terminal IDs. But it needs to know
	//the destination's network interface ID so the network can deliver the packet.
	//Therefore, the terminal layer should maintain a mapping between terminal IDs
	//and network interface IDs.
	//This is similar to the mapping between IP addresses and MAC addresses, except
	//here the mapping is static and nothing like ARP is involved.

	//In this simple network, we simplify things further such that the network interface
	//IDs are the same as the terminal IDs, therefore the mapping becomes M(X)=X. So
	//we don't need to keep any mapping. Sending to terminal X would be sending to
	//network interface X.
	if(adapters[i] != 0 && nis[i] != 0) { //only true when both are in the same LP
	    assert(adapters[i]->get_aid() == nis[i]->get_id());
	}
    }

    Manifold::StopAt(10);
    Manifold::Run();
    Manifold :: Finalize();
}
