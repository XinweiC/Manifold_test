#ifndef __MANIFOLD_SIMPLENET_INTERFACECREATOR_H
#define __MANIFOLD_SIMPLENET_INTERFACECREATOR_H

#include "networkInterface.h"

//#include <iostream>

namespace manifold {
namespace simple_net {

class NetworkInterfaceBase;


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


} //namespace simple_net
} //namespace manifold


#endif //__MANIFOLD_SIMPLENET_INTERFACECREATOR_H


