#include "router.h"
#include <assert.h>
#include <vector>
#include <stdlib.h>

#include "kernel/component.h"

//#include "kernel/manifold.h"

using namespace std;


namespace manifold {
namespace simple_net {

//! @param \c credits  Credits for each channel.
RingRouter :: RingRouter(int id, int credits) : m_id(id), m_CREDITS(credits)
{
    m_credits[0] = m_CREDITS;
    m_credits[1] = m_CREDITS;

    m_priority[0] = IN_BUF_NI; //input from NI has higher priority
    m_priority[1] = IN_BUF_PRED;

    m_OUTPUT_BUFSIZE = 1;
}


//! Handle incoming data from the network interface.
void RingRouter :: handle_interface(int port, NetworkPkt* pkt)
{
    assert(port == IN_NI);

    if(pkt->type == NetworkPkt :: DATA) {
	assert(m_id != pkt->dst);
	assert((int)m_in_buffers[IN_BUF_NI].size() < m_CREDITS);

	//put the pkt in the buffer
	m_in_buffers[IN_BUF_NI].push_back(pkt);
    }
    else if(pkt->type == NetworkPkt :: CREDIT) {
        m_credits[OUT_BUF_NI]++;
	assert(m_credits[OUT_BUF_NI] <= m_CREDITS);
    }
    else {
        assert(0);
    }
}


//! Handle incoming data from the predecessor.
void RingRouter :: handle_pred(int port, NetworkPkt* pkt)
{
    assert(port == IN_PRED);
    assert(pkt->type == NetworkPkt :: DATA);

    assert((int)m_in_buffers[IN_BUF_PRED].size() < m_CREDITS);

    //put the pkt in the buffer
    m_in_buffers[IN_BUF_PRED].push_back(pkt);
}



//! Handle incoming data from the predecessor.
void RingRouter :: handle_succ(int port, NetworkPkt* pkt)
{

    assert(port == IN_SUCC);
    assert(pkt->type == NetworkPkt :: CREDIT);

    m_credits[OUT_BUF_SUCC]++;
    assert(m_credits[OUT_BUF_SUCC] <= m_CREDITS);

}




//! Move packets from input buffer to output buffer if the output buffer has space,
//! then for each output channel, send out 1 packet, if there are any.
//!
//! When a packet is moved from the input buffer to output buffer, a credit is sent
//! back to the sender.
//!
//! The routing scheme has 2 features:
//! 1. Packets headed for different outputs can be sent out at the same time.
//! 2. Input buffers are given round-robin priority to prevent starvation.
void RingRouter :: rising_edge()
{
    vector<int> not_served_inputs;
    vector<int> served_inputs;

    //move (at most) 1 packet from each input buffer to the output buffer.
    for(int i=0; i<2; i++) { //check all input buffers
        int input = m_priority[i];

        if(m_in_buffers[input].size() > 0) {
	    NetworkPkt* pkt = m_in_buffers[input].front();
	    assert(pkt->type == NetworkPkt::DATA);

	    //Find out the output channel for the packet.
	    int output = OUT_BUF_NI; //If I'm the destination, go to NI.

	    if(m_id != pkt->dst) { //I'm not the destination
		output = OUT_BUF_SUCC;
	    }

            if((int)m_outputs[output].size() < m_OUTPUT_BUFSIZE) {
	        //append to output channel
		m_outputs[output].push_back(pkt);
		m_in_buffers[input].pop_front();

                //put in served_inputs so we can update input priority
	        served_inputs.push_back(input);

		//send a credit back
		NetworkPkt* credit = new NetworkPkt(*pkt);
		credit->type = NetworkPkt::CREDIT;
		if(input == IN_BUF_NI)
		    Send(OUT_NI, credit);
		else {
		    assert(input == IN_BUF_PRED);
		    Send(OUT_PRED, credit);
		}
	    }
	    else {//output buffer has no space
	        not_served_inputs.push_back(input);
	    }
	}
	else {//no input
	    not_served_inputs.push_back(input);
	}
    }

    //update priority
    int idx=0;
    for(unsigned i=0; i<not_served_inputs.size(); i++)
	m_priority[idx++] = not_served_inputs[i];
    for(unsigned i=0; i<served_inputs.size(); i++)
	m_priority[idx++] = served_inputs[i];

    //now process the output buffers
    for(int i=0; i<2; i++) {
        if(m_outputs[i].size() > 0 && m_credits[i] > 0) {
	    NetworkPkt* pkt = m_outputs[i].front();
	    m_outputs[i].pop_front();
	    if(i == OUT_BUF_NI) {
		Send(OUT_NI, pkt);
	    }
            else {
	        assert(i == OUT_BUF_SUCC);
		Send(OUT_SUCC, pkt);
	    }
	    m_credits[i]--;
	}
    }

}















//! @param \c id  ID of the router.
//! @param \c x    x dimension of the mesh network.
//! @param \c y    y dimension of the mesh network.
//! @param \c credits    Credits for each channel.
MeshRouter :: MeshRouter(int id, int x, int y, int credits) : m_id(id), m_x_dimension(x), m_y_dimension(y), m_CREDITS(credits)
{
    //credits must be <= input buffer size
    for(int i=0; i<5; i++)
        m_credits[i] = m_CREDITS;

    for(int i=0; i<5; i++)
	m_priority[i] = i;

    m_OUTPUT_BUFSIZE = 1; //no buffer
}




//! Handle incoming data from the network interface.
void MeshRouter :: handle_interface(int port, NetworkPkt* pkt)
{
    assert(port == IN_NI);

    if(pkt->type == NetworkPkt :: DATA) {
	assert(m_id != pkt->dst);
	assert((int)m_in_buffers[port].size() < m_CREDITS);

	//put the pkt in the buffer
	m_in_buffers[port].push_back(pkt);
    }
    else if(pkt->type == NetworkPkt :: CREDIT) {
        m_credits[port]++;
	assert(m_credits[port] <= m_CREDITS);
    }
    else {
        assert(0);
    }
}


//! Handle incoming data from another router.
void MeshRouter :: handle_router(int port, NetworkPkt* pkt)
{
    assert(port > 0 && port < 5);

    if(pkt->type == NetworkPkt :: DATA) {
	assert((int)m_in_buffers[port].size() < m_CREDITS);

	//put the pkt in the buffer
	m_in_buffers[port].push_back(pkt);
    }
    else if(pkt->type == NetworkPkt :: CREDIT) {
        m_credits[port]++;
	assert(m_credits[port] <= m_CREDITS);
	delete pkt;
    }
    else {
        assert(0);
    }
}



//! Move packets from input buffer to output buffer if the output buffer has space,
//! then for each output channel, send out 1 packet, if there are any.
//!
//! When a packet is moved from the input buffer to output buffer, a credit is sent
//! back to the sender.
//!
//! The routing scheme has 2 features:
//! 1. Packets headed for different outputs can be sent out at the same time.
//! 2. Input buffers are given round-robin priority to prevent starvation.
void MeshRouter :: rising_edge()
{
    vector<int> not_served_inputs;
    vector<int> served_inputs;

    //move (at most) 1 packet from each input buffer to the output buffer.
    for(int i=0; i<5; i++) { //check all input buffers
        int input = m_priority[i];

        if(m_in_buffers[input].size() > 0) {
	    NetworkPkt* pkt = m_in_buffers[input].front();

	    //Find out the output channel for the packet.
	    int output = OUT_NI; //If I'm the destination, go to NI.

	    if(m_id != pkt->dst) { //I'm not the destination
		//packet is forwarded based on destination; x first
		int dest_x = pkt->dst % m_x_dimension;
		int dest_y = pkt->dst / m_x_dimension;
		if(dest_x < m_id % m_x_dimension) {
		    output = OUT_WEST;
		}
		else if(dest_x > m_id % m_x_dimension) {
		    output = OUT_EAST;
		}
		else { //same column
		    if(dest_y < m_id / m_x_dimension)
			output = OUT_NORTH;
		    else
			output = OUT_SOUTH;
		}
	    }

            if((int)m_outputs[output].size() < m_OUTPUT_BUFSIZE) {
	        //append to output channel
		m_outputs[output].push_back(pkt);
		m_in_buffers[input].pop_front();

                //put in served_inputs so we can update input priority
	        served_inputs.push_back(input);

		//send a credit back
		NetworkPkt* credit = new NetworkPkt(*pkt);
		credit->type = NetworkPkt::CREDIT;
		Send(input, credit);
	    }
	    else {
	        not_served_inputs.push_back(input);
	    }
	}
	else { //input buffer is empty
	    not_served_inputs.push_back(input);
	}
    }//for

    assert(served_inputs.size() + not_served_inputs.size() == 5);

    //update priority
    int idx=0;
    for(unsigned i=0; i<not_served_inputs.size(); i++)
	m_priority[idx++] = not_served_inputs[i];
    for(unsigned i=0; i<served_inputs.size(); i++)
	m_priority[idx++] = served_inputs[i];


    //now process the output buffers
    for(int i=0; i<5; i++) {
        if(m_outputs[i].size() > 0 && m_credits[i] > 0) {
	    NetworkPkt* pkt = m_outputs[i].front();
	    m_outputs[i].pop_front();
	    Send(i, pkt);
	    m_credits[i]--;
	}
    }


}





} //namespace simple_net
} //namespace manifold

