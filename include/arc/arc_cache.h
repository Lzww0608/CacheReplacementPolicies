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

#include <unordered_map>
#include <string>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>

constexpr size_t DEFAULT_SHARD_COUNT = 1024;

template <typename K, typename V, typename Hash = std::hash<std::string>>
class ARCCache {
private:
    std::vector<std::unique_ptr<LRUShard<K, V, Hash>>> t1_, t2_;
    std::vector<std::unique_ptr<FIFOCache<K, V, Hash>>> b1_, b2_;
    size_t t1_size_, t2_size_, b1_size_, b2_size_;
    size_t p_, c_;
    size_t shard_count_;
    Hash hasher_;
    mutable std::shared_mutex mtx_;

    size_t getShard(const K& key) const;
    void t1_evict();
    void t2_evict();
    void b1_evict();
    void b2_evict();

public:
    ARCCache(size_t p, size_t c, size_t shard_count = DEFAULT_SHARD_COUNT);
    ~ARCCache();

    bool get(const K& key, V& out_value);
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    bool remove(const K& key);

};

template <typename K, typename V, typename Hash>
ARCCache<K, V, Hash>::ARCCache(size_t p, size_t c, size_t shard_count) : p_(p), c_(c), shard_count_(shard_count) {
    t1_size_ = p_;
    t2_size_ = c_ - p_;
    b1_size_ = c_ - p_;
    b2_size_ = p_;

    t1_ = std::make_unique<LRUShard<K, V, Hash>>(p_);
    t2_ = std::make_unique<LRUShard<K, V, Hash>>(c_ - p_);
    b1_ = std::make_unique<LRUShard<K, V, Hash>>(c_ - p_);
    b2_ = std::make_unique<LRUShard<K, V, Hash>>(p_);
};

template <typename K, typename V, typename Hash>
ARCCache<K, V, Hash>::~ARCCache() {
    t1_.reset();
    t2_.reset();
    b1_.reset();
    b2_.reset();
}

template <typename K, typename V, typename Hash>
size_t ARCCache<K, V, Hash>::getShard(const K& key) const {
    return hasher_(key) % shard_count_;
}


template <typename K, typename V, typename Hash>
bool ARCCache<K, V, Hash>::get(const K& key, V& out_value) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    size_t shard_index = getShard(key);

    // 如果key在t1中，则将key从t1中移除，并将其放入t2中
    if (t1_[shard_index]->contains(key)) {
        t1_[shard_index]->get(key, out_value);
        t1_[shard_index]->remove(key);
        if (t2_[shard_index]->full()) {
            auto node = t2_[shard_index]->evict();
            b2_[shard_index]->put(node->key, node->value);
            delete node;
        }
        t2_[shard_index]->put(key, out_value);
        return true;
    }

    // 如果key在t2中，则直接返回
    if (t2_[shard_index]->contains(key)) {
        t2_[shard_index]->get(key, out_value);
        return true;
    }

    // 如果key在b1中，则将key从b1中移除，并将其放入t1中
    // 由于命中b1，所以需要增大t1的最大容量p_，同时减小t2的最大容量c_ - p_
    if (b1_[shard_index]->contains(key)) {
        b1_[shard_index]->get(key, out_value);
        b1_[shard_index]->remove(key);

        /* 增大t1的最大容量p_，同时减小t2的最大容量c_ - p_ */
        p_++;
        t1_[shard_index]->resize(p_);
        t2_[shard_index]->resize(c_ - p_);
        b1_[shard_index]->resize(c_ - p_);
        b2_[shard_index]->resize(p_);
        
        if (t2_[shard_index]->full()) {
            auto node = t2_[shard_index]->evict();
            b2_[shard_index]->put(node->key, node->value);
            delete node;
        }
        return true;
    }

    // 如果key在b2中, 直接更新到t2
    if (b2_[shard_index]->contains(key)) {
        b2_[shard_index]->get(key, out_value);
        b2_[shard_index]->remove(key);
        
        /* 增大t2的最大容量c_ - p_，同时减小t1的最大容量p_ */
        p_--;
        t1_[shard_index]->resize(p_);
        t2_[shard_index]->resize(c_ - p_);
        b1_[shard_index]->resize(c_ - p_);
        b2_[shard_index]->resize(p_);

        t2_[shard_index]->put(key, out_value);
        return true;
    }

    return false;
}



#endif // ARC_CACHE_H