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
    
struct CacheLine {
    bool valid{false};      // 有效位
    uint64_t tag{0};        // 存储地址标签部分
    uint8_t rrpv{0};        // Re-Reference Prediction Value
};

} // namespace SRRIP


#endif // CACHE_LINE_H