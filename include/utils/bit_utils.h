/*
@Author: Lzww
@LastEditTime: 2025-7-11 21:36:47
@Description: 位运算辅助函数
@Language: C++17
*/

#ifndef BIT_UTILS_H
#define BIT_UTILS_H

#include <cstdint>
#include <type_traits>

constexpr uint64_t nextPowerOf2(uint64_t value) {
    if (value <= 1) return 1;
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return value + 1;
}




#endif