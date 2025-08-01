#ifndef W_TINYLFU_CACHE_H
#define W_TINYLFU_CACHE_H

#include <cstdint>

namespace CRP {
namespace w_tineylfu {

template <typename K, typename V, typename Hash = std::hash<std::string>>
class Cache {
public:

private:
    uint64_t capacity_;
    SLRU<K, V, Hash> slru_;
};

}
}

#endif
