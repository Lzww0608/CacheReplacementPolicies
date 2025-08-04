/*
@Author: Lzww
@LastEditTime: 2025-7-21 21:16:33
@Description: TinyLFU窗口缓存
@Language: C++17
*/

#include "../../../include/w_tinylfu/api/loading_cache.h"



namespace CPR {
namespace W_Tinylfu {

template <typename K, typename V, typename Hash>
LoadingCache<K, V, Hash>::LoadingCache(size_t total_capacity, size_t shard_count)
    : cache_(total_capacity, shard_count) {}

template <typename K, typename V, typename Hash>
void LoadingCache<K, V, Hash>::put(const K& key, const V& value, int expire_time) {
    cache_.put(key, value, expire_time);
}

template <typename K, typename V, typename Hash>
bool LoadingCache<K, V, Hash>::get(const K& key, V& out_value) {
    return cache_.get(key, out_value);
}

template <typename K, typename V, typename Hash>
bool LoadingCache<K, V, Hash>::remove(const K& key) {
    return cache_.remove(key);
}

template <typename K, typename V, typename Hash>
void LoadingCache<K, V, Hash>::enableTTL(bool enable) {
    cache_.enableTTL(enable);
}

template <typename K, typename V, typename Hash>
bool LoadingCache<K, V, Hash>::contains(const K& key) {
    return cache_.contains(key);
}

}
}
