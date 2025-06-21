/*
@Author: Lzww
@LastEditTime: 2025-6-20 21:00:49
@Description: LRU缓存实现
@Language: C++17
*/

#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "lru_shard.h"
#include <thread>
#include <vector>
#include <unordered_map>

#define DEFAULT_CAPACITY 1024

template <typename K, typename V, typename Hash = std::hash<K>>
class LRUCache {

private:
    std::vector<std::unique_ptr<LRUShard<K, V>>> shards_;
    size_t shard_count_;
    Hash hasher_;
    
    size_t getShard(const K& key) const;

    static size_t nextPowerOf2(size_t n);

public:
    LRU();
    LRU(int capacity);
    explicit LRUCache(size_t total_capacity, size_t shard_count = 0);
    bool get(const K& key, V& out_value);
    void put(K& key, V& value);

    

};



template <typename K, typename V>
LRUCache<K, V>::LRU(): LRUCache(DEFAULT_CAPACITY) {}



template <typename K, typename V>
LRUCache<K, V>::LRUCache(int capacity): capacity(capacity) {
    // CPU cores * 2，平衡并发与内存开销
    shard_count = nextPowerOf2(std::thread::hardware_concurrency() * 2);
    // 例：8核 → 16分片 → 16倍理论并发度
    shards.reserve(shard_count);
    for (int i = 0; i < shard_count; ++i) {
        shards.emplace_back(std::make_unique<LRUShard<K, V>>(std::max(1UL, capacity / shard_count)));
    }
}

template <typename K, typename V>
explicit LRUCache<K, V>::LRUCache(size_t total_capacity, size_t shard_count = 0) {
    if (shard_count == 0) {
        shard_count = nextPowerOf2(std::thread::hardware_concurrency() * 2);
    }
    shard_count_ = shard_count;
    
    // 每个分片容量 = 总容量 / 分片数
    size_t shard_capacity = std::max(1UL, total_capacity / shard_count_);
    
    shards_.reserve(shard_count_);
    for (size_t i = 0; i < shard_count_; ++i) {
        shards_.emplace_back(std::make_unique<LRUShard<K, V>>(shard_capacity));
    }
}


template <typename K, typename V>
size_t LRUCache<K, V>::getShard(const K& key) const {
    size_t hash_val = hasher_(key);
    return hash_val & (shard_count_ - 1);
}


template <typename K, typename V>
size_t LRUCache<K, V>::nextPowerOf2(size_t n) {
    if (n <= 1) return 1;
    n--;
    n |= n >> 1;  n |= n >> 2;  n |= n >> 4;
    n |= n >> 8;  n |= n >> 16; n |= n >> 32;
    return n + 1;
}


template <typename K, typename V>
bool LRUCache<K, V>::get(const K& key, V& out_value) {
    size_t shard_id = getShard(key);
    return shards_[shard_id]->get(key, out_value);
}
    
template <typename K, typename V>
void LRUCache<K, V>::put(K& key, V& value) {
    size_t shard_id = getShard(key);
    shards_[shard_id]->put(key, value);
}



#endif