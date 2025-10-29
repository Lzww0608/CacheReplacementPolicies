/*
@Author: Lzww
@LastEditTime: 2025-10-29 20:28:08
@Description: BRRIP主缓存
@Language: C++17
*/

#ifndef BRRIP_CACHE_H
#define BRRIP_CACHE_H

#include "cache_set.h"

#include <atomic>
#include <vector>

namespace BRRIP {

template <uint8_t RRPV_M_BITS>
class BRRIPCache {
public:
    // ctor，传入缓存大小（KB），块大小（字节），相联度
    BRRIPCache(size_t cache_size_kb, size_t block_size_bytes, size_t associativity);

    // core interface，访问一个内存地址，返回 true 表示命中， false 表示未命中
    bool access(uint64_t address);

    // get stats
    [[nodiscard]] uint64_t getHitCount() const noexcept {
        return hit_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t getMissCount() const noexcept {
        return miss_count_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] uint64_t getReplaceCount() const noexcept {
        return replace_count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t getHitRate() const noexcept;

    void parseAddress(uint64_t address, uint64_t& tag, size_t& set_index) const;

private:
    const size_t associativity_;
    const size_t num_sets_;
    const int offset_bits_;
    const int set_index_bits_;

    std::vector<CacheSet<RRPV_M_BITS>> sets_;
    std::atomic<uint64_t> hit_count_{0};
    std::atomic<uint64_t> miss_count_{0};
    mutable std::atomic<size_t> replace_count_{0};
};

} // namespace BRRIP

#endif // BRRIP_CACHE_H
