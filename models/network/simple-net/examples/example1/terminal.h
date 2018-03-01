#ifndef __MANIFOLDREFNET_TERMINAL_H
#define __MANIFOLDREFNET_TERMINAL_H

#include "kernel/component-decl.h"

#include "simple-net/networkPkt.h"

//class NetworkPkt;
class ClientData;

class Terminal : public manifold::kernel::Component {
public:
    enum { IN_FROM_NET = 0, IN_FROM_CLIENT = 1 };
    enum { OUT_TO_NET = 0, OUT_TO_CLIENT = 1 };

    Terminal(int nid) : m_nid(nid) {}

    int get_nid() { return m_nid; }
    void handle_incoming_from_net(int, NetworkPkt*);
    void handle_incoming_from_client(int, ClientData*);
private:
    int m_nid; //! node id
    int client_to_net(int); //! convert client address to network address
};

#endif

