/*
@Author: Lzww
@LastEditTime: 2025-10-29 20:32:42
@Description: BRRIP缓存集实现
@Language: C++17
*/

#include "cache_set.h"

#include <stdexcept>
#include <random>
#include <algorithm>

namespace BRRIP {

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

// 根据 BRRIP 策略查找一个牺牲者 way 索引
template <uint8_t RRPV_M_BITS>
[[nodiscard]] size_t CacheSet<RRPV_M_BITS>::findVictimWay() {
    // write lock
    std::unique_lock<std::shared_mutex> writeLock(*mtx_);
    
    // RRPV Age-Up机制：如果max_rrpv < RRPV_MAX，递增所有行的RRPV直到有行达到RRPV_MAX
    while (max_rrpv < RRPV_MAX) {
        // 遍历所有当前max_rrpv的行，将它们的RRPV递增
        for (uint8_t rrpv = 0; rrpv <= max_rrpv; ++rrpv) {
            auto& bucket = buckets_[rrpv];
            if (!bucket.empty()) {
                // 将这个bucket中的所有way移动到rrpv+1
                auto& next_bucket = buckets_[rrpv + 1];
                for (size_t way_idx : bucket) {
                    ways_[way_idx].rrpv++;
                    next_bucket.push_back(way_idx);
                }
                // 更新位图
                rrpv_presence |= (1 << (rrpv + 1));
                // 清空当前bucket
                bucket.clear();
                rrpv_presence &= ~(1 << rrpv);
            }
        }
        max_rrpv++;
    }
    
    // 现在max_rrpv == RRPV_MAX，从max_rrpv bucket中随机选择victim
    auto& candidates = buckets_[max_rrpv];
    
    // 使用C++11随机数生成器
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    size_t victim_idx = candidates[dist(gen)];

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
    
    // BRRIP策略：双模随机选择
    // 以epsilon（通常是1/32 = 3.125%）的概率插入RRPV_MAX（远期预测）
    // 以1-epsilon的概率插入RRPV_MAX-1（近期预测）
    static thread_local std::mt19937 gen(std::random_device{}());
    static thread_local std::bernoulli_distribution dist(1.0 / 32.0); // epsilon = 1/32
    
    uint8_t insert_rrpv = dist(gen) ? RRPV_MAX : (RRPV_MAX - 1);
    line.rrpv = insert_rrpv;

    buckets_[insert_rrpv].push_back(way_index);
    rrpv_presence |= (1 << insert_rrpv);
    max_rrpv = std::max<uint8_t>(max_rrpv, insert_rrpv);
}

} // namespace BRRIP

// 模板显式实例化，避免链接错误
template class BRRIP::CacheSet<2>;  // 2位RRPV (0-3)
template class BRRIP::CacheSet<3>;  // 3位RRPV (0-7)
