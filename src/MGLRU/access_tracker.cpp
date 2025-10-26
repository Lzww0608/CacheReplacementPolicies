/*
@Author: Lzww
@LastEditTime: 2025-10-26 21:57:41
@Description: MGLRU Access Tracker Implementation
@Language: C++17
*/

#include "access_tracker.h"

AccessTracker::AccessTracker(size_t size) 
    : bitset_(size), size_mask_(size - 1) {
    // size must be power of 2
    // size_mask is used for quick modulo operation
}

void AccessTracker::set(PageFrameId page_id) {
    size_t idx = (page_id >> 6) & size_mask_;  // divide by 64 to get bitset index
    size_t bit = page_id & 0x3F;                // mod 64 to get bit position
    bitset_[idx].fetch_or(1ULL << bit, std::memory_order_relaxed);
}

bool AccessTracker::check_and_clear(PageFrameId page_id) {
    size_t idx = (page_id >> 6) & size_mask_;
    size_t bit = page_id & 0x3F;
    uint64_t mask = 1ULL << bit;
    
    // atomically check and clear the bit
    uint64_t old_val = bitset_[idx].fetch_and(~mask, std::memory_order_acq_rel);
    return (old_val & mask) != 0;
}

void AccessTracker::clear() {
    for (auto& bits : bitset_) {
        bits.store(0, std::memory_order_relaxed);
    }
}

