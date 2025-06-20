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



template <typename K, typename V>
class LRUCache {

private:
    std::vector<LRUShard<K, V>> shards;
    int capacity;
    int shard_count;

public:
    LRU(int capacity);
    bool get(const K& key, V& out_value);
    void put(K& key, V& value);

};





template <typename K, typename V>
LRUCache<K, V>::LRUCache(int capacity): capacity(capacity) {
    shard_count = std::thread::hardware_concurrency();
    shards.reserve(shard_count);
    for (int i = 0; i < shard_count; ++i) {
        shards.emplace_back(capacity / shard_count);
    }
}



#endif