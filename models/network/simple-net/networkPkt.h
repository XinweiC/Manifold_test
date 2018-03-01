#ifndef __MANIFOLD_SIMPLENET_NETWORKPKT_H
#define __MANIFOLD_SIMPLENET_NETWORKPKT_H

#include <stdint.h>

class NetworkPkt {
public:
    enum { DATA=0, CREDIT};

    static const int MAX_DATA_SIZE = 512;

    int type;
    int src; //source network interface
    int dst; //destination network interface
    int payload_size;
    unsigned long tick_enter_network;
    uint8_t data[MAX_DATA_SIZE]; //payload data
};

#endif

