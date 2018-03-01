#include "mapping.h"

namespace manifold {
namespace simple_net {

//! This simple mapping simply maps the terminal id to itself.
//! It is based on the assumption that the terminal's ID is
//! the same as the id of the networkinterface to which it is connected.
//! If this assumptions is false, then this mapping should not be used.
int Simple_terminal_to_net_mapping :: terminal_to_net(int aid)
{
    return aid;
}

} //namespace simple_net
} //namespace manifold
