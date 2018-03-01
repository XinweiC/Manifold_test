#ifndef __MANIFOLD_SIMPLENET_NETWORK_H
#define __MANIFOLD_SIMPLENET_NETWORK_H

#include <vector>

#include "kernel/common-defs.h"
#include "kernel/component.h"
#include "kernel/clock.h"
//#include "networkInterface.h"
//#include "interfaceCreator.h"

//using manifold::kernel::LpId_t;
//using manifold::kernel::CompId_t;

namespace manifold {
namespace simple_net {

class Terminal_to_net_mapping;
class NetworkInterfaceBase;
class InterfaceCreator;


/*
class InterfaceCreator {
public:
    virtual ~InterfaceCreator() {}
    virtual NetworkInterfaceBase* create(manifold::kernel::LpId_t lp, int id, manifold::kernel::CompId_t&) = 0;
};

template<class T>
class GenInterfaceCreator :  public InterfaceCreator {
public:
    GenInterfaceCreator(GenNI_flow_control_setting& s) : m_flow_control_setting(s) {}

    virtual NetworkInterfaceBase* create(manifold::kernel::LpId_t lp, int id, manifold::kernel::CompId_t& cid)
    {
        cid = manifold::kernel::Component :: Create<T>(lp, id, m_flow_control_setting);
        NetworkInterfaceBase* nip =  manifold::kernel::Component :: GetComponent <T>(cid);
	return nip;
    }
private:
    GenNI_flow_control_setting m_flow_control_setting;
};
*/


//#########################################################################
//#########################################################################
//! An abstract network that is simply a container of network interfaces.
//! The simple network only contains network interfaces, all of which are
//! in the same logic process (LP).
class SimpleNetwork {
public:
    SimpleNetwork(int n, manifold::kernel::LpId_t, InterfaceCreator*, Terminal_to_net_mapping*);
    virtual ~SimpleNetwork();

    std::vector<manifold::kernel::CompId_t>& get_interfaceCids();
    std::vector<NetworkInterfaceBase*>& get_interfaces();
    NetworkInterfaceBase* get_interface(int id);
    Terminal_to_net_mapping* get_mapping() { return m_mapping; }

    virtual void print_stats(std::ostream&) {}


protected:

    //keep the component IDs because connecting components requires component IDs.
    std::vector<manifold::kernel::CompId_t> m_interfaceCids;
    std::vector<NetworkInterfaceBase*> m_interfaces;

    //keep a terminal address to network address mapping
    Terminal_to_net_mapping* m_mapping;
};


//#########################################################################
//#########################################################################
class RingRouter;

//! A ring network.
class RingNetwork : public SimpleNetwork {
public:
    RingNetwork(int n, int credits, manifold::kernel::LpId_t, manifold::kernel::Clock&, InterfaceCreator*, Terminal_to_net_mapping*);
    ~RingNetwork();

    virtual void print_stats(std::ostream&);

#ifdef SIMPLE_NET_UTEST
    int get_ni_router_delay() { return DELAY_NI_ROUTER; }
    int get_router_router_delay() { return DELAY_ROUTER_ROUTER; }
    std::vector<RingRouter*>& get_routers() { return m_routers; }
    std::vector<manifold::kernel::CompId_t>& get_routerCids() { return m_routerCids; }
#endif

private:
    void register_with_clock(manifold::kernel::Clock&);

    static const int DELAY_NI_ROUTER = 1; //delay between NI and router
    static const int DELAY_ROUTER_ROUTER = 1; //inter-router delay

    std::vector<manifold::kernel::CompId_t> m_routerCids;
    std::vector<RingRouter*> m_routers;
};



//#########################################################################
//#########################################################################
class MeshRouter;

//! A mesh network.
class MeshNetwork : public SimpleNetwork {
public:
    MeshNetwork(int x, int y, int credits, manifold::kernel::LpId_t, manifold::kernel::Clock&, InterfaceCreator*, Terminal_to_net_mapping*);
    ~MeshNetwork();

    virtual void print_stats(std::ostream&);

#ifdef SIMPLE_NET_UTEST
    int get_ni_router_delay() { return DELAY_NI_ROUTER; }
    int get_router_router_delay() { return DELAY_ROUTER_ROUTER; }
    std::vector<std::vector<MeshRouter*> >& get_routers() { return  m_routers; }
    std::vector<std::vector<manifold::kernel::CompId_t> >& get_routerCids() { return  m_routerCids; }
#endif

private:
    void register_with_clock(manifold::kernel::Clock&);

    static const int DELAY_NI_ROUTER = 1; //delay between NI and router
    static const int DELAY_ROUTER_ROUTER = 1; //inter-router delay


    const int m_xdimension;
    const int m_ydimension;
    std::vector<std::vector<manifold::kernel::CompId_t> > m_routerCids; //2-D vector
    std::vector<std::vector<MeshRouter*> > m_routers; //2-D vector
};








} //namespace simple_net
} //namespace manifold

#endif
