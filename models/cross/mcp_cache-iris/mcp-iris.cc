#include "mcp-iris.h"

#include "mcp-cache/coh_mem_req.h"
#include "mcp-cache/coherence/MESI_enum.h"

using namespace manifold::uarch;
using namespace manifold::mcp_cache_namespace;

int MCPSimLen :: get_simulated_len(NetworkPacket* pkt)
{
    if(pkt->type == m_COH) { //coherence msg between MCP caches
	Coh_msg& msg = *((Coh_msg*)pkt->data);
	const int SZ_NO_DATA = 34; //32B addr + 2B overhead.
	const int SZ_WITH_DATA = 34 + m_cacheLineSize; //32B addr + 2B overhead + line

		switch(msg.msg) {
		    case MESI_CM_I_to_E:
		    case MESI_CM_I_to_S:
		    case MESI_CM_E_to_I:
		    case MESI_CM_M_to_I:
		    case MESI_CM_UNBLOCK_I:
			return SZ_NO_DATA;

		    case MESI_CM_UNBLOCK_I_DIRTY:
			return SZ_WITH_DATA;

		    case MESI_CM_UNBLOCK_E:
		    case MESI_CM_UNBLOCK_S:
		    case MESI_CM_CLEAN:
			return SZ_NO_DATA;

		    case MESI_CM_WRITEBACK:
		    case MESI_MC_GRANT_E_DATA:
		    case MESI_MC_GRANT_S_DATA:
			return SZ_WITH_DATA;

		    case MESI_MC_GRANT_I:
		    case MESI_MC_FWD_E:
		    case MESI_MC_FWD_S:
		    case MESI_MC_DEMAND_I:
			return SZ_NO_DATA;

		    case MESI_CC_E_DATA:
		    case MESI_CC_M_DATA:
		    case MESI_CC_S_DATA:
			return SZ_WITH_DATA;
		    default:
			assert(0);
			break;
		}
    }
    else if(pkt->type == m_MEM) { //memory msg between cache and MC
		const int SZ_NO_DATA = 36; //32B addr + 4B overhead.
		const int SZ_WITH_DATA = 36 + m_cacheLineSize; //32B addr + 4B overhead + line

		Mem_msg& msg = *((Mem_msg*)pkt->data);

		if(msg.type == Mem_msg :: MEM_REQ) {
		    if(msg.is_read()) //LOAD request
			return SZ_NO_DATA;
		    else
			return SZ_WITH_DATA;
		}
		else if(msg.type == Mem_msg :: MEM_RPLY) {
		    return SZ_WITH_DATA;
		}
		else {
		    assert(0);
		}
    }
    else {
	assert(0);
    }

}


//Requests are assigned to virtual net 0; replies virtual net 1.
int MCPVnet :: get_virtual_net(NetworkPacket* pkt)
{
    if(pkt->type == m_COH) { //coherence msg between MCP caches
	Coh_msg& msg = *((Coh_msg*)pkt->data);
        if(msg.type == Coh_msg :: COH_REQ)
            return 0;
        else if(msg.type == Coh_msg :: COH_RPLY)
            return 1;
        else {
            assert(0);
        }
    }
    else if(pkt->type == m_MEM) { //memory msg between cache and MC
	Mem_msg& msg = *((Mem_msg*)pkt->data);

	if(msg.type == Mem_msg :: MEM_REQ) {
            return 0;
	}
	else if(msg.type == Mem_msg :: MEM_RPLY) {
            return 1;
	}
	else {
	    assert(0);
	}
    }
    else {
	assert(0);
    }
}
