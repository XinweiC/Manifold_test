#ifndef __MANIFOLD_SIMPLENET_ROUTER_H
#define __MANIFOLD_SIMPLENET_ROUTER_H

#include "kernel/component-decl.h"
#include "networkPkt.h"

#include <list>


namespace manifold {
namespace simple_net {


//############################################################################
//############################################################################
//! This router is used in a ring network. It has 2 input ports: one from the
//! network interface, and another from its predecessor. It also has 2 output
//! ports: one to the interface, and another to the successor.
//! To support flow control, we add an output port to the predecessor and an
//! input port from successor.
class RingRouter : public manifold::kernel::Component {
public:
    enum { IN_NI=0, IN_PRED, IN_SUCC};
    enum { OUT_NI=0, OUT_PRED, OUT_SUCC};

    RingRouter(int id, int credits);

    int get_id() { return m_id; }

    void handle_interface(int, NetworkPkt*);
    void handle_pred(int, NetworkPkt*);
    void handle_succ(int, NetworkPkt*);
    void rising_edge();

    //int get_ni_credit() { return m_credits[OUT_BUF_NI]; }

#ifndef SIMPLE_NET_UTEST
private:
#endif
    const int m_id;
    const int m_CREDITS; //credits for each channel
    int m_OUTPUT_BUFSIZE; //output buffer size

    //There is no input from the successor and no output to predecessor.
    enum {IN_BUF_NI=0, IN_BUF_PRED=1}; //define enums to avoid hard-coding
    enum {OUT_BUF_NI=0, OUT_BUF_SUCC=1};

    std::list<NetworkPkt*> m_in_buffers[2]; //input buffers: 0 for NI, 1 for predecessor
    std::list<NetworkPkt*> m_outputs[2]; //output buffers: 0 for NI, 1 for successor

    int m_credits[2]; //credits for output channels: 0 for NI, 1 for successor
    int m_priority[2]; //the input buffer priority. Index 0 has the highest priority.
                       //We implement round-robin priority, i.e.,
                       //a served input buffer is moved to the lowest priority.
};



//############################################################################
//############################################################################
class MeshRouter : public manifold::kernel::Component {
public:
    enum { IN_NI=0, IN_EAST, IN_WEST, IN_NORTH, IN_SOUTH};
    enum { OUT_NI=0, OUT_EAST, OUT_WEST, OUT_NORTH, OUT_SOUTH};

    MeshRouter(int id, int x, int y, int credits);

    int get_id() { return m_id; }

    void handle_interface(int, NetworkPkt*);
    void handle_router(int, NetworkPkt*);
    void rising_edge();

    //int get_ni_credit() { return m_credits[0]; }

#ifndef SIMPLE_NET_UTEST
private:
#endif
    const int m_id;
    const int m_x_dimension;
    const int m_y_dimension;
    const int m_CREDITS; //credits for each channel.
    std::list<NetworkPkt*> m_in_buffers[5]; //input buffers
    std::list<NetworkPkt*> m_outputs[5]; //output buffers
    int m_credits[5]; //credits for the 5 output channels
    int m_OUTPUT_BUFSIZE; //size of the output buffers
    int m_priority[5]; //the input buffer priority. Index 0 has the highest priority.
                       //We implement round-robin priority, i.e.,
                       //a served input buffer is moved to the lowest priority.
};





} //namespace simple_net
} //namespace manifold

#endif
