/*
@Author: Lzww  
@LastEditTime: 2025-7-14 20:53:43
@Description: ARC算法实现 
@Language: C++17
*/

#ifndef ARC_CACHE_H
#define ARC_CACHE_H

#include "../lru/lru_shard.h"
#include "../fifo/fifo_cache.h"
#include "../utils/bit_utils.h"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <algorithm>

constexpr size_t DEFAULT_SHARD_COUNT = 16;

template <typename K, typename V, typename Hash = std::hash<std::string>>
class ARCCache {
private:
    // T1: 最近访问一次的页面 (LRU)
    std::vector<std::unique_ptr<LRUShard<K, V, Hash>>> t1_;
    // T2: 频繁访问的页面 (LRU)  
    std::vector<std::unique_ptr<LRUShard<K, V, Hash>>> t2_;
    // B1: 最近被从T1中驱逐的页面元数据 (FIFO淘汰)
    std::vector<std::unique_ptr<FIFOCache<K, V, Hash>>> b1_;
    // B2: 最近被从T2中驱逐的页面元数据 (FIFO淘汰)
    std::vector<std::unique_ptr<FIFOCache<K, V, Hash>>> b2_;
    
    std::atomic<size_t> p_;  // T1的目标大小
    size_t c_;               // 总缓存大小 (T1 + T2 ≤ c)
    size_t shard_count_;
    Hash hasher_;
    mutable std::shared_mutex mtx_;
    
    // 辅助函数
    size_t getShard(const K& key) const;
    void replace(size_t shard_index, const K& key);
    void adaptP(size_t shard_index, bool hit_b1);
    void adjustCacheSize();  // 根据p值调整B1和B2大小
    size_t getCurrentT1Size(size_t shard_index) const;
    size_t getCurrentT2Size(size_t shard_index) const;
    
public:
    ARCCache(size_t p, size_t c, size_t shard_count = DEFAULT_SHARD_COUNT);
    ~ARCCache();

    bool get(const K& key, V& out_value);
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    bool remove(const K& key);
    bool contains(const K& key) const;
    
    // 统计信息
    struct CacheStats {
        size_t t1_size = 0;
        size_t t2_size = 0;
        size_t b1_size = 0;
        size_t b2_size = 0;
        size_t target_p = 0;
        size_t total_capacity = 0;
    };
    CacheStats getStats() const;
};

template <typename K, typename V, typename Hash>
ARCCache<K, V, Hash>::ARCCache(size_t p, size_t c, size_t shard_count) 
    : p_(p), c_(c), shard_count_(shard_count) {
    
    // 参数验证
    if (c == 0) {
        throw std::invalid_argument("Cache capacity must be greater than 0");
    }
    if (p > c) {
        throw std::invalid_argument("Target size p cannot exceed total capacity c");
    }
    if (shard_count == 0) {
        throw std::invalid_argument("Shard count must be greater than 0");
    }
    
    // 初始化分片
    t1_.reserve(shard_count_);
    t2_.reserve(shard_count_);
    b1_.reserve(shard_count_);
    b2_.reserve(shard_count_);
    
    // 计算每个分片的容量
    size_t t1_capacity = std::max(1UL, p / shard_count_);
    size_t t2_capacity = std::max(1UL, (c - p) / shard_count_);
    size_t b1_capacity = std::max(1UL, (c - p) / shard_count_);  // B1大小与T2目标大小相同
    size_t b2_capacity = std::max(1UL, p / shard_count_);        // B2大小与T1目标大小相同
    
    for (size_t i = 0; i < shard_count_; i++) {
        t1_.emplace_back(std::make_unique<LRUShard<K, V, Hash>>(t1_capacity));
        t2_.emplace_back(std::make_unique<LRUShard<K, V, Hash>>(t2_capacity));
        b1_.emplace_back(std::make_unique<FIFOCache<K, V, Hash>>(b1_capacity));
        b2_.emplace_back(std::make_unique<FIFOCache<K, V, Hash>>(b2_capacity));
    }
}

template <typename K, typename V, typename Hash>
ARCCache<K, V, Hash>::~ARCCache() {
    t1_.clear();
    t2_.clear();
    b1_.clear();
    b2_.clear();
}

template <typename K, typename V, typename Hash>
size_t ARCCache<K, V, Hash>::getShard(const K& key) const {
    return hasher_(key) % shard_count_;
}

template <typename K, typename V, typename Hash>
size_t ARCCache<K, V, Hash>::getCurrentT1Size(size_t shard_index) const {
    return t1_[shard_index]->size();
}

template <typename K, typename V, typename Hash>
size_t ARCCache<K, V, Hash>::getCurrentT2Size(size_t shard_index) const {
    return t2_[shard_index]->size();
}

template <typename K, typename V, typename Hash>
void ARCCache<K, V, Hash>::adaptP(size_t shard_index, bool hit_b1) {
    size_t old_p = p_.load();
    size_t b1_size = b1_[shard_index]->getSize();
    size_t b2_size = b2_[shard_index]->getSize();
    
    size_t new_p = old_p;
    if (hit_b1) {
        // 命中B1，增加T1的目标大小
        size_t delta = std::max(1UL, b2_size / b1_size);
        new_p = std::min(c_, old_p + delta);
    } else {
        // 命中B2，减少T1的目标大小
        size_t delta = std::max(1UL, b1_size / b2_size);
        new_p = (old_p > delta) ? old_p - delta : 0;
    }
    
    p_.store(new_p);
    adjustCacheSize();
}

template <typename K, typename V, typename Hash>
void ARCCache<K, V, Hash>::adjustCacheSize() {
    size_t current_p = p_.load();
    
    // 计算每个分片的新容量
    // B1大小与T2目标大小相同，B2大小与T1目标大小相同
    size_t b1_capacity_per_shard = std::max(1UL, (c_ - current_p) / shard_count_);  // B1 = T2大小
    size_t b2_capacity_per_shard = std::max(1UL, current_p / shard_count_);         // B2 = T1大小
    
    // 调整所有分片的B1和B2大小
    for (size_t i = 0; i < shard_count_; i++) {
        b1_[i]->resize(b1_capacity_per_shard);
        b2_[i]->resize(b2_capacity_per_shard);
    }
}

template <typename K, typename V, typename Hash>
void ARCCache<K, V, Hash>::replace(size_t shard_index, const K& key) {
    size_t current_p = p_.load();
    size_t t1_size = getCurrentT1Size(shard_index);
    size_t t2_size = getCurrentT2Size(shard_index);
    
    // 如果T1不为空且(|T1| > p 或 (key在B2中且|T1| = p))
    if (t1_size > 0 && (t1_size > current_p / shard_count_ || 
        (b2_[shard_index]->contains(key) && t1_size == current_p / shard_count_))) {
        
        // 从T1中淘汰最久未使用的页面
        auto evicted_node = t1_[shard_index]->evict();
        if (evicted_node) {
            b1_[shard_index]->put(evicted_node->key, evicted_node->value);
            delete evicted_node;
        }
    } else if (t2_size > 0) {
        // 从T2中淘汰最久未使用的页面
        auto evicted_node = t2_[shard_index]->evict();
        if (evicted_node) {
            b2_[shard_index]->put(evicted_node->key, evicted_node->value);
            delete evicted_node;
        }
    }
}

template <typename K, typename V, typename Hash>
bool ARCCache<K, V, Hash>::get(const K& key, V& out_value) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    size_t shard_index = getShard(key);
    
    // Case 1: 命中T1，移动到T2
    if (t1_[shard_index]->get(key, out_value)) {
        t1_[shard_index]->remove(key);
        t2_[shard_index]->put(key, out_value);
        return true;
    }
    
    // Case 2: 命中T2，更新LRU位置
    if (t2_[shard_index]->get(key, out_value)) {
        return true;
    }
    
    // Case 3: 命中B1，调整参数并放入T2
    if (b1_[shard_index]->contains(key)) {
        V value;
        if (b1_[shard_index]->get(key, value)) {
            b1_[shard_index]->remove(key);
            adaptP(shard_index, true);
            
            // 可能需要替换
            if (getCurrentT1Size(shard_index) + getCurrentT2Size(shard_index) >= c_ / shard_count_) {
                replace(shard_index, key);
            }
            
            t2_[shard_index]->put(key, value);
            out_value = value;
            return true;
        }
    }
    
    // Case 4: 命中B2，调整参数并放入T2
    if (b2_[shard_index]->contains(key)) {
        V value;
        if (b2_[shard_index]->get(key, value)) {
            b2_[shard_index]->remove(key);
            adaptP(shard_index, false);
            
            // 可能需要替换
            if (getCurrentT1Size(shard_index) + getCurrentT2Size(shard_index) >= c_ / shard_count_) {
                replace(shard_index, key);
            }
            
            t2_[shard_index]->put(key, value);
            out_value = value;
            return true;
        }
    }
    
    return false;
}

template <typename K, typename V, typename Hash>
void ARCCache<K, V, Hash>::put(const K& key, const V& value, int expire_time) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    size_t shard_index = getShard(key);
    
    // Case 1: 已在T1中，移动到T2
    if (t1_[shard_index]->contains(key)) {
        t1_[shard_index]->remove(key);
        t2_[shard_index]->put(key, value, expire_time);
        return;
    }
    
    // Case 2: 已在T2中，更新值
    if (t2_[shard_index]->contains(key)) {
        t2_[shard_index]->put(key, value, expire_time);
        return;
    }
    
    // Case 3: 在B1中，调整参数并放入T2
    if (b1_[shard_index]->contains(key)) {
        b1_[shard_index]->remove(key);
        adaptP(shard_index, true);
        
        // 可能需要替换
        if (getCurrentT1Size(shard_index) + getCurrentT2Size(shard_index) >= c_ / shard_count_) {
            replace(shard_index, key);
        }
        
        t2_[shard_index]->put(key, value, expire_time);
        return;
    }
    
    // Case 4: 在B2中，调整参数并放入T2
    if (b2_[shard_index]->contains(key)) {
        b2_[shard_index]->remove(key);
        adaptP(shard_index, false);
        
        // 可能需要替换
        if (getCurrentT1Size(shard_index) + getCurrentT2Size(shard_index) >= c_ / shard_count_) {
            replace(shard_index, key);
        }
        
        t2_[shard_index]->put(key, value, expire_time);
        return;
    }
    
    // Case 5: 新键，放入T1
    // 检查是否需要替换
    if (getCurrentT1Size(shard_index) + getCurrentT2Size(shard_index) >= c_ / shard_count_) {
        replace(shard_index, key);
    }
    
    t1_[shard_index]->put(key, value, expire_time);
}

template <typename K, typename V, typename Hash>
bool ARCCache<K, V, Hash>::remove(const K& key) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    size_t shard_index = getShard(key);
    
    bool removed = false;
    
    if (t1_[shard_index]->remove(key)) {
        removed = true;
    }
    
    if (t2_[shard_index]->remove(key)) {
        removed = true;
    }
    
    return removed;
}

template <typename K, typename V, typename Hash>
bool ARCCache<K, V, Hash>::contains(const K& key) const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    size_t shard_index = getShard(key);
    
    return t1_[shard_index]->contains(key) || t2_[shard_index]->contains(key);
}

template <typename K, typename V, typename Hash>
typename ARCCache<K, V, Hash>::CacheStats ARCCache<K, V, Hash>::getStats() const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    
    CacheStats stats;
    stats.target_p = p_.load();
    stats.total_capacity = c_;
    
    for (size_t i = 0; i < shard_count_; ++i) {
        stats.t1_size += getCurrentT1Size(i);
        stats.t2_size += getCurrentT2Size(i);
        stats.b1_size += b1_[i]->getSize();
        stats.b2_size += b2_[i]->getSize();
    }
    
    return stats;
}

#endif // ARC_CACHE_H