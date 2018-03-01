#include <assert.h>

#include "kernel/component.h"
#include "kernel/manifold.h"

#include "interfaceCreator.h"
#include "network.h"
#include "networkInterface.h"
#include "router.h"

using namespace manifold::kernel;

#include <iostream>
using namespace std;


namespace manifold {
namespace simple_net {


//! @param n The number of network interfaces.
//! @param lp The logic process to which all the network interfaces are assigned.
//! All the network interfaces are on the same LP.
SimpleNetwork :: SimpleNetwork(int n, LpId_t lp, InterfaceCreator* ifcreator, Terminal_to_net_mapping* mapping)
{
    assert(n > 1);

    m_interfaces.resize(n); // Do not use reserve()! Otherwise size() would be 0.
    m_interfaceCids.resize(n);

    for(int i=0; i<n; i++) {
    /*
        m_interfaceCids[i] = Component :: Create<NetworkInterface>(lp, i);

        m_interfaces[i] = Component :: GetComponent <NetworkInterface>(m_interfaceCids[i]);
	*/
        m_interfaces[i] = ifcreator->create(lp, i, m_interfaceCids[i]);
	

	if(m_interfaces[i]) //Remeber the object is only created in the specified LP
	    m_interfaces[i]->set_network(this);
    }

    m_mapping = mapping;
}


SimpleNetwork :: ~SimpleNetwork()
{
    for(unsigned i=0; i<m_interfaces.size(); i++)
        delete m_interfaces[i];
}


std::vector<CompId_t>& SimpleNetwork :: get_interfaceCids()
{
    return m_interfaceCids;
}


std::vector<NetworkInterfaceBase*>& SimpleNetwork :: get_interfaces()
{
    return m_interfaces;
}


//! Returns the network interface with the given id.
NetworkInterfaceBase* SimpleNetwork :: get_interface(int id)
{
    assert(id >=0 && id < (int)m_interfaces.size());
    return m_interfaces[id];
}





//! @param n \c   Number of routers in the ring.
//! @param lp \c   The logic process to which all the network interfaces are assigned.
//! All the network interfaces are on the same LP.
RingNetwork :: RingNetwork(int n, int credits, LpId_t lp, Clock& clk, InterfaceCreator* ifcreator, Terminal_to_net_mapping* mapping) : SimpleNetwork(n, lp, ifcreator, mapping)
{
    //Base class has already created the network interfaces.
    assert((int)m_interfaceCids.size() == n);

    m_routers.resize(n); // Do not use reserve()! Otherwise size() would be 0.
    m_routerCids.resize(n);

    //create routers
    for(int i=0; i<n; i++) {
        m_routerCids[i] = Component :: Create<RingRouter>(lp, i, credits);
        m_routers[i] = Component :: GetComponent <RingRouter>(m_routerCids[i]);

        if(m_interfaces[i]) {  // the pointer is only valid in one LP
	    assert(m_interfaces[i]->get_id() == i);
	}
    }


    if(m_interfaces[0] == 0) //no need to connect if the network is not in this LP
        return;

    //connect routers and interfaces, and between routers
    for(int i=0; i<n; i++) {

        //NI to router
        Manifold :: Connect(m_interfaceCids[i], m_interfaces[i]->get_output_port_for_router(),
	                    m_routerCids[i], RingRouter::IN_NI, &RingRouter :: handle_interface, DELAY_NI_ROUTER);
        //router to NI
        Manifold :: Connect(m_routerCids[i], RingRouter::OUT_NI,
	                    m_interfaceCids[i], m_interfaces[i]->get_input_port_for_router(),
			    &NetworkInterfaceBase :: handle_router, DELAY_NI_ROUTER);
        //router to successor
        Manifold :: Connect(m_routerCids[i], RingRouter::OUT_SUCC,
	                    m_routerCids[(i+1)%n], RingRouter::IN_PRED,
			    &RingRouter :: handle_pred, DELAY_ROUTER_ROUTER);
        //successor to router
        Manifold :: Connect(m_routerCids[(i+1)%n], RingRouter::OUT_PRED,
	                    m_routerCids[i], RingRouter::IN_SUCC,
			    &RingRouter :: handle_succ, DELAY_ROUTER_ROUTER);
    }

    register_with_clock(clk);
}


RingNetwork :: ~RingNetwork()
{
    for(unsigned i=0; i<m_routers.size(); i++)
        delete m_routers[i];
}


void RingNetwork :: register_with_clock(Clock& c)
{
    for(unsigned i=0; i<m_interfaces.size(); i++) {
        if(m_interfaces[i] != 0) {
	    Clock::Register(c, m_interfaces[i], &NetworkInterfaceBase::rising_edge, (void(NetworkInterfaceBase::*)(void))0);
	}
    }

    for(unsigned i=0; i<m_routers.size(); i++) {
	if(m_routers[i] != 0) {
	    Clock::Register(c, m_routers[i], &RingRouter::rising_edge,
	    (void(RingRouter::*)(void))0);
	}
    }
}



void RingNetwork :: print_stats(ostream& out)
{
    for(unsigned i=0; i<m_interfaces.size(); i++) {
        if(m_interfaces[i] != 0) {
	    m_interfaces[i]->print_stats(out);
	}
    }

}


//! @param x The x dimension of the network.
//! @param y The y dimension of the network.
//! @param lp The logic process to which all the network interfaces are assigned.
//! All the network interfaces are on the same LP.
MeshNetwork :: MeshNetwork(int x, int y, int credits, LpId_t lp, Clock& clk, InterfaceCreator* ifcreator, Terminal_to_net_mapping* mapping) : SimpleNetwork(x*y, lp, ifcreator, mapping),
    m_xdimension(x), m_ydimension(y)
{
    assert(x > 0 && y > 0);

    const int N = x*y;

    //Base class has already created the network interfaces.
    assert((int)m_interfaceCids.size() == N);

    m_routers.resize(y); // Do not use reserve()! Otherwise size() would be 0.
    m_routerCids.resize(y);


    //create routers
    for(int i=0; i<y; i++) { //y rows
        vector<manifold::kernel::CompId_t> cid_row(x);
        vector<MeshRouter*> obj_row(x);

        for(int j=0; j<x; j++) { // x columns

	    cid_row[j] = Component :: Create<MeshRouter>(lp, i*x + j, x, y, credits);
	    obj_row[j] = Component :: GetComponent <MeshRouter>(cid_row[j]);

	    if(m_interfaces[i*x + j]) { // interface only valid in its own LP
	        assert(obj_row[j]);
		assert(m_interfaces[i*x + j]->get_id() == i*x + j);
	    }

	}
	m_routerCids[i] = cid_row;
	m_routers[i] = obj_row;
    }


    if(m_interfaces[0] == 0) //no need to connect if the network is not in this LP
        return;

    //connect routers and interfaces, and between routers
    for(int i=0; i<y; i++) { // y rows
	for(int j=0; j<x; j++) { // x columns
	    int idx = i*x + j; //one dimension index

	    //NI to router
	    Manifold :: Connect(m_interfaceCids[idx], m_interfaces[idx]->get_output_port_for_router(),
				m_routerCids[i][j], MeshRouter::IN_NI, &MeshRouter :: handle_interface, DELAY_NI_ROUTER);
	    //router to NI
	    Manifold :: Connect(m_routerCids[i][j], RingRouter::OUT_NI,
				m_interfaceCids[idx], m_interfaces[idx]->get_input_port_for_router(),
				&NetworkInterfaceBase :: handle_router, DELAY_NI_ROUTER);
	    //router to router

	    //Eastward
	    if(j < x-1) {
		Manifold :: Connect(m_routerCids[i][j], MeshRouter::OUT_EAST,
				    m_routerCids[i][j+1], MeshRouter::IN_WEST,
				    &MeshRouter :: handle_router, DELAY_ROUTER_ROUTER);
	    }
	    //Westward
	    if(j > 0) {
		Manifold :: Connect(m_routerCids[i][j], MeshRouter::OUT_WEST,
				    m_routerCids[i][j-1], MeshRouter::IN_EAST,
				    &MeshRouter :: handle_router, DELAY_ROUTER_ROUTER);
	    }
	    //Northward
	    if(i > 0) {
		Manifold :: Connect(m_routerCids[i][j], MeshRouter::OUT_NORTH,
				    m_routerCids[i-1][j], MeshRouter::IN_SOUTH,
				    &MeshRouter :: handle_router, DELAY_ROUTER_ROUTER);
	    }
	    //Southward
	    if(i < y-1) {
		Manifold :: Connect(m_routerCids[i][j], MeshRouter::OUT_SOUTH,
				    m_routerCids[i+1][j], MeshRouter::IN_NORTH,
				    &MeshRouter :: handle_router, DELAY_ROUTER_ROUTER);
	    }
        }
    }

    register_with_clock(clk);
}


MeshNetwork :: ~MeshNetwork()
{
    for(unsigned i=0; i<m_routers.size(); i++) {
	for(unsigned j=0; j<m_routers[i].size(); j++)
	    delete m_routers[i][j];
    }
}


void MeshNetwork :: register_with_clock(Clock& c)
{
    for(unsigned i=0; i<m_interfaces.size(); i++) {
        if(m_interfaces[i] != 0) {
	    Clock::Register(c, m_interfaces[i], &NetworkInterfaceBase::rising_edge, (void(NetworkInterfaceBase::*)(void))0);
	}
    }

    for(unsigned i=0; i<m_routers.size(); i++) {
	for(unsigned j=0; j<m_routers[i].size(); j++)
	    if(m_routers[i][j] != 0) {
	        Clock::Register(c, m_routers[i][j], &MeshRouter::rising_edge, (void(MeshRouter::*)(void))0);
	    }
    }
}


void MeshNetwork :: print_stats(ostream& out)
{
    for(unsigned i=0; i<m_interfaces.size(); i++) {
        if(m_interfaces[i] != 0) {
	    m_interfaces[i]->print_stats(out);
	}
    }

}

/*

//! @param id1 ID of first network interface.
//! @param id2 ID of second network interface.
//! Return the Manhattan distance of two network interfaces.
Ticks_t MeshNetwork :: get_delay(int id1, int id2)
{
    int x1 = id1 % m_xdimension;
    int y1 = id1 / m_xdimension;

    int x2 = id2 % m_xdimension;
    int y2 = id2 / m_xdimension;

    int xdiff = x1 - x2;
    if(xdiff < 0)
        xdiff = -xdiff;
    int ydiff = y1 - y2;
    if(ydiff < 0)
        ydiff = -ydiff;

    return xdiff + ydiff;
}
*/



} //namespace simple_net
} //namespace manifold

