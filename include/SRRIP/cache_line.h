/*
@Author: Lzww
@LastEditTime: 2025-8-13 21:54:18
@Description: SRRIP缓存行实现
@Language: C++17
*/

#ifndef CACHE_LINE_H
#define CACHE_LINE_H

#include <cstdint>

namespace SRRIP {

enum class MESIState {
    Invalid,
    Shared,
    Exclusive,
    Modified,
};

struct CacheLine {
    bool valid{false};                          // Valid Bit
    uint64_t tag{0};                            // Tag
    uint8_t rrpv{2};                            // Re-Reference Prediction Value
    bool dirty{false};                          // Dirty Bit：Marks whether the line has been modified
    uint8_t data[64];                           // Data Block：Holds the actual cached data（a 64-byte cache line）
    MESIState mesi_state{MESIState::Invalid};   // Coherence State
};

} // namespace SRRIP


#endif // CACHE_LINE_H
