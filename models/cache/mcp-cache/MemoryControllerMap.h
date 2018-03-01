#ifndef MANIFOLD_MCP_CACHE_MEMORYCONTROLLERMAP_H
#define MANIFOLD_MCP_CACHE_MEMORYCONTROLLERMAP_H

#include "uarch/DestMap.h"

#include <assert.h>
#include <vector>

namespace manifold {
namespace mcp_cache_namespace {


class PageBasedMap : public manifold::uarch::DestMap {
public:
    PageBasedMap(std::vector<int>& nodes, int page_offset_bits) : m_page_offset_bits(page_offset_bits)
    {
	assert(nodes.size() > 0);

	for(unsigned i=0; i<nodes.size(); i++)
	    m_nodes.push_back(nodes[i]);

	int bits_for_mask = myLog2(nodes.size());

        m_selector_mask = (0x1 << bits_for_mask) - 1;
    }

    int lookup(uint64_t addr)
    {
        return m_nodes[m_selector_mask & (addr >> m_page_offset_bits)];
    }

private:

    static int myLog2(unsigned num)
    {
	assert(num > 0);

	int bits = 0;
	while(((unsigned)0x1 << bits) < num) {
	    bits++;
	}
	return bits;
    }

    std::vector<int> m_nodes; //target node ids
    const int m_page_offset_bits;
    uint64_t m_selector_mask;
};


} //namespace mcp_cache_namespace
} //namespace manifold

#endif //MANIFOLD_MCP_CACHE_MEMORYCONTROLLERMAP_H
