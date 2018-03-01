#include "client.h"

#include "kernel/component.h"

#include <stdlib.h>
#include <assert.h>
#include <iostream>

using namespace std;
using namespace manifold::kernel;

int Client :: Num_nodes = 0;


ostream& operator<<(ostream& stream, const ClientData& cd)
{
    for(int i=0; i<ClientData::DATA_LEN; i++) {
      stream << hex << (unsigned)cd.data[i] << " ";
    }
    stream << dec;
    return stream;
}


//! This function is registered with the rising edge of the clock.
void Client :: tick()
{
}

//! This function is registered with the falling edge of the clock.
//! It generates a data packet and sends it to the network.
void Client :: tock()
{
    ClientData* d = new ClientData();

    d->src_nid = m_nid;
    //randomly pick a destination; mustn't be self
    srandom(m_nid + Manifold::NowTicks() + 1023); //This is to ensure results can be re-produced with
                                                  //different numbers of LPs.
    while((d->dst_nid = random() % Num_nodes) == m_nid);

    for(int i=0; i<ClientData::DATA_LEN; i++) {
        d->data[i] = static_cast<uint8_t>(random());
    }

    cout << "Sending " << Manifold::NowTicks() << " (" << d->src_nid << " => " << d->dst_nid << ") : " << "   ";
    cout << *d << endl;
    Send(OUT_TO_NET, d);
}

//! Event handler for data coming in from the network
void Client :: handle_incoming_data(int, ClientData* data)
{
    assert(data->dst_nid == m_nid);
    cout << "Received " << Manifold::NowTicks() << " (" << data->src_nid << " => " << data->dst_nid << ") : " << "   ";
    cout << *data << endl;
    delete data;
}
