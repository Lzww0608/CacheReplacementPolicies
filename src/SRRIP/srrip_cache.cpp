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
SRRIPCache::SRRIPCache(size_t cache_size_kb, size_t block_size_bytes, size_t associativity)
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


} // namespace SRRIP
