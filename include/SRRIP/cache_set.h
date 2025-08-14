/*
@Author: Lzww
@LastEditTime: 2025-8-13 22:04:00
@Description: SRRIP缓存集实现
@Language: C++17
*/

#ifndef CACHE_SET_H
#define CACHE_SET_H

#include "cache_line.h"

#include <vector>
#include <cstdint>
#include <optional>
#include <mutex>
#include <memory>
#include <shared_mutex>

namespace SRRIP {

template <uint8_t RRPV_M_BITS>
class CacheSet {
public:
    // 构造函数，传入该组的相联度（路的数量）
    explicit CacheSet(size_t associativity);

    // 移动构造函数
    CacheSet(CacheSet&& other) noexcept;

    // 移动赋值运算符
    CacheSet& operator=(CacheSet&& other) noexcept;

    // 删除拷贝构造函数和赋值运算符
    CacheSet(const CacheSet&) = delete;
    CacheSet& operator=(const CacheSet&) = delete;

    // 析构函数
    ~CacheSet() = default;

    // 查找与给定 tag 匹配的 way 索引
    [[nodiscard]] std::optional<size_t> findWay(uint64_t tag) const;

    // 查找一个空闲的 way 索引
    [[nodiscard]] std::optional<size_t> findEmptyWay() const;

    // 根据 SRRIP 策略查找一个牺牲者 way 索引
    [[nodiscard]] size_t findVictimWay();

    // 访问一个 way（命中时调用），将其RRPV置为0
    void accessWay(size_t way_index);

    // 填充一个 way（未命中时调用），设置 tag 和 RRPV
    void fillWay(size_t way_index, uint64_t tag);

private:
    static constexpr uint8_t RRPV_MAX = (1 << RRPV_M_BITS) - 1;
    std::vector<CacheLine> ways_;
    // buckets[r] stores the index of the cache line "RRPV = r"
    std::vector<std::vector<size_t>> buckets_;
    // State Bit: if rth bit is 1, there is a cache line "RRPV = r"
    uint32_t rrpv_presence;
    // max RRPV currently
    uint8_t max_rrpv;

    mutable std::unique_ptr<std::shared_mutex> mtx_;

    struct Stats {
        size_t hits = 0;
        size_t misses = 0;
        size_t replacements = 0;
    } stats_;
};

}

#endif // CACHE_SET_H
