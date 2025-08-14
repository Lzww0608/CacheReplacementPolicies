/*
@Author: Lzww
@LastEditTime: 2025-8-14 14:10:09
@Description: SRRIP缓存集实现
@Language: C++17
*/

#include "../../include/SRRIP/cache_set.h"

namespace SRRIP {

// ctor
template <uint8_t RRPV_M_BITS>
explicit CacheSet(size_t associativity)
    : RRPV_MAX((1 << RRPV_M_BITS) - 1) {
    ways_.resize(associativity);
    buckets_.resize(RRPV_MAX + 1);
    rrpv_presence = 0;
    max_rrpv = 0;
}

[[nodiscard]] std::optional<size_t> findWay(uint64_t tag) const {
    // read lock  
    std::shared_lock readLock(mtx_);
    for (size_t i = 0; i < ways_.size(); i++) {
        if (ways_[i].valid && ways_[i].tag == tag) {
            return i;
        }
    }

    return std::nullopt;
}

} // namespace SRRIP
