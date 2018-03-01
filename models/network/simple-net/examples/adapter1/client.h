#ifndef __MANIFOLDREFNET_CLIENT_H
#define __MANIFOLDREFNET_CLIENT_H

#include "kernel/component-decl.h"

#include <iostream>

struct ClientData {
    static const int DATA_LEN = 10;

    int src_nid; //! source node id
    int dst_nid; //! destination node id
    uint8_t data[DATA_LEN];
    int get_dst() { return dst_nid; }

    friend std::ostream& operator<<(std::ostream& stream, const ClientData& cd);
};

class Client : public manifold::kernel::Component {
public:
    enum { IN_FROM_NET = 0 };
    enum { OUT_TO_NET = 0 };

    Client(int nid) : m_nid(nid) {}

    int get_nid() { return m_nid; }
    void tick(); //! called on clock rising edge
    void tock(); //! called on clock falling edge
    void handle_incoming_data(int, ClientData*);

    static void Set_numNodes(int n) { Num_nodes = n; }
private:
    int m_nid;
    static int Num_nodes; //! number of nodes
};

#endif
