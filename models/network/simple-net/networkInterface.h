#ifndef __MANIFOLD_SIMPLENET_NETWORKINTERFACE_H
#define __MANIFOLD_SIMPLENET_NETWORKINTERFACE_H

#include "kernel/component-decl.h"
#include "networkPkt.h"
#include "network.h"
#include "mapping.h"
#include <assert.h>

#ifdef DBG_SIMPLENET_INTERFACE
#include <iostream>
//#include "kernel/manifold.h"
#endif

#include "kernel/manifold.h"



namespace manifold {
namespace simple_net {



//! A base class for network interface. It has no event handlers.
class NetworkInterfaceBase : public manifold::kernel::Component {
public:
    NetworkInterfaceBase(int id) : m_id(id) {}
    virtual ~NetworkInterfaceBase() {}

    int get_id() { return m_id; }
    void set_network(SimpleNetwork* n) { m_network = n; }

    virtual int get_output_port_for_router() = 0;
    virtual int get_input_port_for_router() = 0;

    virtual void handle_router(int, NetworkPkt*) = 0;
    virtual void rising_edge() = 0;

    //flow control
    virtual int get_bufsize_router() { return 0; }
    virtual int get_credit_router() { return 0; }

    virtual void print_stats(std::ostream&) {}

#ifdef SIMPLE_NET_UTEST
    SimpleNetwork* get_network() { return m_network; }
#endif

protected:
    int m_id; //interface id
    SimpleNetwork* m_network;
};


/* deprecated
class SimpleNetworkInterface : public NetworkInterfaceBase {
public:
    enum {IN_FROM_TERMINAL = 0};
    enum {OUT_TO_TERMINAL = 0};

    SimpleNetworkInterface(int id) : NetworkInterfaceBase(id) {}

    void handle_new_packet(int, NetworkPkt* pkt);

private:
    void handle_dst_packet(NetworkPkt* pkt, manifold::kernel::Ticks_t delay);
};
*/






struct GenNI_flow_control_setting
{
    //router side
    int credits;
    //terminal side: TBD
};



//! This templated class allows it to accept any data type from the terminal,
//! as long as the data type doesn't contain pointers. Therefore, it doesn't
//! require an adapter to be placed between the terminal and the network interface.
template<typename T>
class GenNetworkInterface : public NetworkInterfaceBase {
public:
    enum {IN_FROM_TERMINAL = 0, IN_FROM_ROUTER};
    enum {OUT_TO_TERMINAL = 0, OUT_TO_ROUTER};

    GenNetworkInterface(int id, GenNI_flow_control_setting s) : NetworkInterfaceBase(id),
	m_CREDITS(s.credits)
    {
	m_credits_router = s.credits; //init credits

	//stats
	m_stats_packets_from_terminal = 0;
	m_stats_packets_to_terminal = 0;
	m_stats_packets_total_delay = 0;
    }
    ~GenNetworkInterface();


    int get_output_port_for_router() { return OUT_TO_ROUTER; }
    int get_input_port_for_router() { return IN_FROM_ROUTER; }

    void handle_terminal(int, T* data);
    void handle_router(int, NetworkPkt*);

    virtual void rising_edge();

    int get_credit_router() { return m_credits_router; }

    virtual void print_stats(std::ostream&);

#ifndef SIMPLE_NET_UTEST
private:
#endif
    int m_credits_router; //credits for output channel to router
    const int m_CREDITS; // the initial value of credits for output channel to router.

    std::list<NetworkPkt*> m_input_from_terminal;
    std::list<NetworkPkt*> m_input_from_router;

    //stats
    unsigned long m_stats_packets_from_terminal;
    unsigned long m_stats_packets_to_terminal;
    double m_stats_packets_total_delay;
};


template<typename T>
GenNetworkInterface<T> :: ~GenNetworkInterface<T>()
{
    for(std::list<NetworkPkt*>::iterator it=m_input_from_terminal.begin();
        it != m_input_from_terminal.end(); ++it) {
        delete(*it);
    }

    while(m_input_from_router.size() > 0) {
        NetworkPkt* tp = m_input_from_router.front();
        delete tp;
	m_input_from_router.pop_front();
    }
}


//! Process input from terminal; put it in the input buffer.
template<typename T>
void GenNetworkInterface<T> :: handle_terminal(int, T* cdata)
{
    assert(sizeof(T) <= (unsigned)NetworkPkt::MAX_DATA_SIZE);

    NetworkPkt* pkt = new NetworkPkt(); //This pkt is not really necessary, but in case we decide to add
                                        //routers to the network.
    pkt->type = NetworkPkt::DATA;
    pkt->src = m_id;
    assert(m_network->get_mapping());
    pkt->dst = m_network->get_mapping()->terminal_to_net(cdata->get_dst()); //Client data must support get_dst()
    pkt->payload_size = sizeof(T);
    pkt->tick_enter_network = manifold::kernel::Manifold::NowTicks();

#ifdef DBG_SIMPLENET_INTERFACE
std::cout << "@@ " << manifold::kernel::Manifold::NowTicks() << " Interface (" << m_id << "): (Terminal: " << cdata->get_src() << " => " << cdata->get_dst() << ") (Net addr: " << pkt->src << " -> " << pkt->dst<< ")" << std::endl;
#endif

    uint8_t* ptr = (uint8_t*)cdata;
    for(unsigned i=0; i<sizeof(T); i++) {
	pkt->data[i] = *ptr;
	ptr++;
    }

    delete cdata;

    //Send(OUT_TO_ROUTER, pkt);
    m_input_from_terminal.push_back(pkt);

    m_stats_packets_from_terminal++;
}



//! Process input from router; put it in the input buffer.
template<typename T>
void GenNetworkInterface<T> :: handle_router(int, NetworkPkt* pkt)
{
    if(pkt->type == NetworkPkt :: DATA) {
	assert(pkt->dst == m_id);
	#ifdef DBG_SIMPLENET_INTERFACE
	std::cout << "simple net: destination NI got packet from router  " << pkt->src << " => " << pkt->dst << std::endl;
	#endif

	assert((int)m_input_from_router.size() < m_CREDITS);

	m_input_from_router.push_back(pkt);
    }
    else if(pkt->type == NetworkPkt :: CREDIT) {
	#ifdef DBG_SIMPLENET_INTERFACE
	std::cout << "simple net: NI got credit from router.  " << std::endl;
	#endif
        m_credits_router++;
        assert(m_credits_router <= m_CREDITS);
    }
    else {
        assert(0);
    }

}


template<typename T>
void GenNetworkInterface<T> :: rising_edge()
{
    if(m_input_from_terminal.size() > 0 && m_credits_router > 0) {
        NetworkPkt* pkt = m_input_from_terminal.front();
	m_input_from_terminal.pop_front();
	Send(OUT_TO_ROUTER, pkt);
	m_credits_router--;
    }

    if(m_input_from_router.size() > 0) {
	NetworkPkt* pkt = m_input_from_router.front();
	m_input_from_router.pop_front();

	m_stats_packets_to_terminal++;
	m_stats_packets_total_delay += (unsigned long) (manifold::kernel::Manifold::NowTicks() - pkt->tick_enter_network);

	T* cdp = new T(*((T*)(pkt->data)));

	Send(OUT_TO_TERMINAL, cdp);


	//delete pkt;
	//Reuse pkt

	//send a credit back to router
	NetworkPkt* credit = pkt;
	credit->type = NetworkPkt::CREDIT;
	Send(OUT_TO_ROUTER, credit);
    }

}



template<typename T>
void GenNetworkInterface<T> :: print_stats(std::ostream& out)
{
    out << "NetworkInterface " << m_id << std::endl
        << "    packets from terminal = " << m_stats_packets_from_terminal << std::endl
        << "    packets to terminal = " << m_stats_packets_to_terminal << std::endl;
    if(m_stats_packets_to_terminal != 0)
        out << "    average packet delay = " << m_stats_packets_total_delay / m_stats_packets_to_terminal << std::endl;
    else
        out << "    average packet delay = N/A" << std::endl;
}




} //namespace simple_net
} //namespace manifold

#endif
