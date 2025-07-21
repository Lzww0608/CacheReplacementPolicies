/*
@Author: Lzww
@LastEditTime: 2025-7-21 21:16:27
@Description: TinyLFU窗口缓存
@Language: C++17
*/

#ifndef W_TINYL_CACHE_API_LOADING_CACHE_H_
#define W_TINYL_CACHE_API_LOADING_CACHE_H_

#include "../../lru/lru_cache.h"


namespace CPR {
namespace W_Tinylfu {

template <typename K, typename V, typename Hash = std::hash<std::string>>
struct LoadingCache {
    using CacheType = LRUCache<K, V, Hash>;
    using CachePtr = std::shared_ptr<CacheType>;

    CachePtr cache_;

    LoadingCache(size_t total_capacity, size_t shard_count = 0);
    
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    
    bool get(const K& key, V& out_value);
    
    bool remove(const K& key);
    
    void enableTTL(bool enable = true);
    
};



}
}

#endif // W_TINYL_CACHE_API_LOADING_CACHE_H_