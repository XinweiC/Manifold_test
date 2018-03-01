#ifndef MANIFOLD_UARCH_DESTMAP_H
#define MANIFOLD_UARCH_DESTMAP_H

#include <stdint.h>

namespace manifold {
namespace uarch {

class DestMap {
public:
    virtual ~DestMap() {}
    virtual int lookup(uint64_t) = 0;
};

} //namespace uarch
} //namespace manifold

#endif //MANIFOLD_UARCH_DESTMAP_H
