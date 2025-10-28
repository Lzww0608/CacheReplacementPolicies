/*
@Author: Lzww
@LastEditTime: 2025-10-28 20:12:01
@Description: BRRIP缓存集实现
@Language: C++17
*/

#ifndef CACHE_SET_H
#define CACHE_SET_H

#include "cache_line.h"

#include <cstdint>

namesapce BRRIP {

template <uint8_t RRPV_M_BITS>
class CacheSet {
public:
    // ctor，传入该组的相联度（路的数量）
    explicit CacheSet(size_t associativity);

    // move ctor
    CacheSet(CacheSet&& other) noexcept;

    CacheSet& operator=(CacheSet&& other) noexcept;

    CacheSet(const CacheSet&) = delete;
    CacheSet& operator=(const CacheSet&) = delete;

    ~CacheSet() = default;

    // 查找与给定tag匹配的way索引
    [[nodiscard]] std::optional<size_t> findWay(uint64_t tag) const;

    // 查找一个空闲的way索引
    [[nodiscard]] std::optioanl<size_t> findEmptyWay() const;

    // 根据BRRIP策略查找一个牺牲者way索引
    [[nodiscard]] size_t findVictimWay();

    // 访问一个way（命中时调用），将其RRPV设置为0
    void accessWay(size_t way_index);

    // 填充一个way（未命中时调用），设置tag和RRPV
    void fillWay(size_t way_index, uint64_t tag);
};




}; // BRRIP

#endif // !CACHE_SET_H
