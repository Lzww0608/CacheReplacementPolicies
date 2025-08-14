/*
@Author: Lzww
@LastEditTime: 2025-8-14 17:19:14
@Description: SRRIP主缓存实现
@Language: C++17
*/

#include "../../include/SRRIP/srrip_cache.h"

#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace SRRIP {

template <uint8_t RRPV_M_BITS>
SRRIPCache<RRPV_M_BITS>::SRRIPCache(size_t cache_size_kb, size_t block_size_bytes, size_t associativity)
    : associativity_(associativity) {
    if (cache_size_kb == 0 || block_size_bytes == 0 || associativity == 0) {
        throw std::invalid_argument("Cache paramenters must b positive");
    }

    // block_size must b power of 2
    if (block_size_bytes & (block_size_bytes - 1) != 0) {
        throw std::invalid_argument("Block size must be a power of 2");
    }

    // calculate total block numbers
    size_t total_bytes = cache_size_kb * 1024;
    size_t total_blocks = total_bytes / block_size_bytes;
    if (total_bytes % block_size_bytes != 0) {
        throw std::invalid_argument("Cache size must b divisible by block size");
    }

    // total blocks must be divisible by associativity
    if (total_blocks % associativity != 0) {
        throw std::invalid_argument("Total blocks must b divisible by associativity");
    }

    // number of sets must b a power of 2
    num_sets_ = total_blocks / associativity;
    if (num_sets_ & (num_sets_ - 1) != 0) {
        throw std::invalid_argument("Number of sets must b a power of 2");
    }
      
    // calculate offset and index 
    offset_bits_ = static_cast<int> (std::log2(block_size_bytes));
    set_index_bits_ = static_cast<int> (std::log2(num_sets_));
      
    // initialize all the sets
    sets_.resize(num_sets_, CacheSet<RRPV_M_BITS>(associativity));
}

template <uint8_t RRPV_M_BITS>
void SRRIPCache<RRPV_M_BITS>::parseAddress(uint64_t address, uint64_t& tag, size_t& set_index) const {
    set_index = (address >> offset_bits_) & ((1ULL << set_index_bits_) - 1);
    tag = address >> (offset_bits_ + set_index_bits_);
}

template <uint8_t RRPV_M_BITS>
bool SRRIPCache<RRPV_M_BITS>::access(uint64_t address) {
    uint64_t tag;
    size_t set_index;
    parseAddress(address, tag, set_index);

    assert(set_index < num_sets_ && "Invalid set index");

    auto& target_set = sets_[set_index];
    auto way = target_set.findWay(tag);

    if (way.has_value()) {
        target_set.accessWay(way.value());
        hits_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    } else {
        miss_count_.fetch_add(1, std::memory_order_relaxed);

        // 1. find empty block
        auto empty_way = target_set.findEmptyWay();
        if (empty_way.has_value()) {
            target_set.fillWay(empty_way.value(), tag);
            return false;
        }

        // 2. find victim and replace it 
        size_t victim_way = target_set.findVictimWay();
        target_set.fillWay(victim_way, tag);
        return false;
    }
}


} // namespace SRRIP

