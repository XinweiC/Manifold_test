#include "terminal.h"
#include "client.h"
#include "simple-net/networkPkt.h"
#include "kernel/component.h"

#include <iostream>
using namespace std;

using namespace manifold::kernel;

void Terminal :: handle_incoming_from_net(int, NetworkPkt* pkt)
{
    ClientData* cdp = new ClientData(*((ClientData*)(pkt->data)));
    delete pkt;
    Send(OUT_TO_CLIENT, cdp);
}



void Terminal :: handle_incoming_from_client(int, ClientData* cdata)
{
    assert(sizeof(ClientData) <= (unsigned)NetworkPkt::MAX_DATA_SIZE);

    NetworkPkt* pkt = new NetworkPkt();
    pkt->src = m_nid;
    pkt->dst = client_to_net(cdata->dst_nid);
    pkt->payload_size = sizeof(ClientData);

    uint8_t* ptr = (uint8_t*)cdata;
    for(unsigned i=0; i<sizeof(ClientData); i++) {
    //cout << hex << (unsigned)*ptr << " ";
	pkt->data[i] = *ptr;
	ptr++;
    }
    //cout << dec << endl;

    delete cdata;
    Send(OUT_TO_NET, pkt);
}

int Terminal :: client_to_net(int client_addr)
{
    return client_addr;
}
