/*
@Author: Lzww
@LastEditTime: 2025-8-14 14:10:09
@Description: SRRIP缓存集实现
@Language: C++17
*/

#include "../../include/SRRIP/cache_set.h"

#include <stdexcept>
#include <random>
#include <algorithm>

namespace SRRIP {

// ctor
template <uint8_t RRPV_M_BITS>
CacheSet<RRPV_M_BITS>::CacheSet(size_t associativity) {
    if (associativity == 0) {
        throw std::invalid_argument("associativity must be positive");
    }
    ways_.resize(associativity);
    buckets_.resize(RRPV_MAX + 1);
    rrpv_presence = 0;
    max_rrpv = 0;
    mtx_ = std::make_unique<std::shared_mutex>();
}

// 移动构造函数
template <uint8_t RRPV_M_BITS>
CacheSet<RRPV_M_BITS>::CacheSet(CacheSet&& other) noexcept
    : ways_(std::move(other.ways_))
    , buckets_(std::move(other.buckets_))
    , rrpv_presence(other.rrpv_presence)
    , max_rrpv(other.max_rrpv)
    , mtx_(std::move(other.mtx_)) {
}

// 移动赋值运算符
template <uint8_t RRPV_M_BITS>
CacheSet<RRPV_M_BITS>& CacheSet<RRPV_M_BITS>::operator=(CacheSet&& other) noexcept {
    if (this != &other) {
        ways_ = std::move(other.ways_);
        buckets_ = std::move(other.buckets_);
        rrpv_presence = other.rrpv_presence;
        max_rrpv = other.max_rrpv;
        mtx_ = std::move(other.mtx_);
    }
    return *this;
}

template <uint8_t RRPV_M_BITS>
[[nodiscard]] std::optional<size_t> CacheSet<RRPV_M_BITS>::findWay(uint64_t tag) const {
    // read lock  
    std::shared_lock<std::shared_mutex> readLock(*mtx_);
    for (size_t i = 0; i < ways_.size(); i++) {
        if (ways_[i].valid && ways_[i].tag == tag) {
            return i;
        }
    }

    return std::nullopt;
}

// 查找一个空闲的 way 索引
template <uint8_t RRPV_M_BITS>
[[nodiscard]] std::optional<size_t> CacheSet<RRPV_M_BITS>::findEmptyWay() const {
    // read lock  
    std::shared_lock<std::shared_mutex> readLock(*mtx_);
    for (size_t i = 0; i < ways_.size(); i++) {
        if (!ways_[i].valid) {
            return i;
        }
    }

    return std::nullopt;
}

// 根据 SRRIP 策略查找一个牺牲者 way 索引
template <uint8_t RRPV_M_BITS>
[[nodiscard]] size_t CacheSet<RRPV_M_BITS>::findVictimWay() {
    // write lock
    std::unique_lock<std::shared_mutex> writeLock(*mtx_);
    // randomly select from max_rrpv buckets
    auto& candidates = buckets_[max_rrpv];
    size_t victim_idx = candidates[rand() % candidates.size()];

    // if the bucket is empty after eviction, update `max_rrpv` by find the next largest value via `rrpv_presence`
    candidates.erase(std::remove(candidates.begin(), candidates.end(), victim_idx), candidates.end());
    if (candidates.empty()) {
        rrpv_presence &= ~(1 << max_rrpv);
        // find the next largest value
        while (max_rrpv > 0 && !(rrpv_presence & (1 << max_rrpv))) {
            max_rrpv--;
        }
    }

    return victim_idx;
}

// 访问一个 way（命中时调用），将其RRPV置为0
template <uint8_t RRPV_M_BITS>
void CacheSet<RRPV_M_BITS>::accessWay(size_t way_index) {
    // write lock
    std::unique_lock<std::shared_mutex> writeLock(*mtx_);
    auto& line = ways_[way_index];
    uint8_t old_rrpv = line.rrpv;
    line.rrpv = 0; // reset to 0
    line.valid = true;

    // remove from old bucket, add to bucket '0'
    auto& old_bucket = buckets_[old_rrpv];
    old_bucket.erase(std::remove(old_bucket.begin(), old_bucket.end(), way_index), old_bucket.end());
    if (old_bucket.empty()) {
        rrpv_presence &= ~(1 << old_rrpv);
        // if old_rrpv == max_rrpv, update
        if (old_rrpv == max_rrpv) {
            while (max_rrpv > 0 && !(rrpv_presence & (1 << max_rrpv))) {
                max_rrpv--;
            }
        }
    }

    buckets_[0].push_back(way_index);
    rrpv_presence |= 1;
    max_rrpv = std::max<uint8_t>(0, max_rrpv);
}

// 填充一个 way（未命中时调用），设置 tag 和 RRPV
template <uint8_t RRPV_M_BITS>
void CacheSet<RRPV_M_BITS>::fillWay(size_t way_index, uint64_t tag) {
    // write lock
    std::unique_lock<std::shared_mutex> writeLock(*mtx_);

    auto& line = ways_[way_index];
    line.valid = true;
    line.tag = tag;
    line.rrpv = 2;

    buckets_[2].push_back(way_index);
    rrpv_presence |= (1 << 2);
    max_rrpv = std::max<uint8_t>(max_rrpv, 2);
}

} // namespace SRRIP

