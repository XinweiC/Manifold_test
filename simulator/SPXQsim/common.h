#ifndef COMMON_H
#define COMMON_H

#include "uarch/networkPacket.h"
#include "iris/interfaces/simulatedLen.h"
#include "iris/interfaces/vnetAssign.h"

//Object of this class is passed to Iris to help determine simulated length of a packet.
class MySimLen : public manifold::iris::SimulatedLen<manifold::uarch::NetworkPacket> {
public:
    MySimLen(int cacheLineSize, int coh_msg_type, int mem_msg_type, int credit_msg_type) :
          m_cacheLineSize(cacheLineSize), m_COH(coh_msg_type), m_MEM(mem_msg_type), m_CREDIT(credit_msg_type)
    {}

    int get_simulated_len(manifold::uarch::NetworkPacket* pkt);
private:
    const int m_cacheLineSize;
    const int m_COH; //coherence message type
    const int m_MEM; //memory message type
    const int m_CREDIT; //credit message type
};




//Object of this class is passed to Iris to help determine virtual network of a packet.
class MyVnet : public manifold::iris::VnetAssign<manifold::uarch::NetworkPacket> {
public:
    MyVnet(int coh_msg_type, int mem_msg_type, int credit_msg_type) :
          m_COH(coh_msg_type), m_MEM(mem_msg_type), m_CREDIT(credit_msg_type)
    {}

    int get_virtual_net(manifold::uarch::NetworkPacket* pkt);

private:
    const int m_COH; //coherence message type
    const int m_MEM; //memory message type
    const int m_CREDIT; //credit message type
};

#endif
