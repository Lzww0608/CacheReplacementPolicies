/*
@Author: Lzww
@LastEditTime: 2025-6-28 22:36:48
@Description: LRU缓存实现
@Language: C++17
*/

#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "lru_shard.h"
#include <thread>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <condition_variable>

#define DEFAULT_CAPACITY 1024
#define DEFAULT_EXPIRE_TIME 60000  // 1分钟，毫秒
#define TTL_CLEANUP_INTERVAL_MS 1000  // TTL清理间隔


template <typename K, typename V, typename Hash>
class TTLManager;

template <typename K, typename V, typename Hash>
class LRUCache {
    // 友元类声明
    friend class TTLManager<K, V, Hash>;

private:
    std::vector<std::unique_ptr<LRUShard<K, V>>> shards_;
    size_t shard_count_;
    Hash hasher_;
    
    // TTL后台清理
    std::unique_ptr<TTLManager<K, V, Hash>> ttl_manager_;
    std::atomic<bool> enable_ttl_;
    
    size_t getShard(const K& key) const;
    static size_t nextPowerOf2(size_t n);

public:
    LRUCache();
    LRUCache(int capacity);
    explicit LRUCache(size_t total_capacity, size_t shard_count = 0);
    ~LRUCache();
    
    bool get(const K& key, V& out_value);
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    bool remove(const K& key);
    
    // TTL控制
    void enableTTL(bool enable = true);
    void disableTTL();
    
    // 统计信息
    struct CacheStats {
        size_t total_hits = 0;
        size_t total_misses = 0;
        size_t total_evictions = 0;
        size_t expired_count = 0;
        double hit_rate() const { 
            return total_hits + total_misses > 0 ? 
                   static_cast<double>(total_hits) / (total_hits + total_misses) : 0.0; 
        }
    };
    CacheStats getStats() const;
};

// TTL管理器类
template <typename K, typename V, typename Hash>
class TTLManager {
private:
    LRUCache<K, V, Hash>* cache_;
    std::thread cleanup_thread_;
    std::atomic<bool> running_;
    std::condition_variable cv_;
    std::mutex cv_mutex_;
    
    void cleanupLoop();
    
public:
    explicit TTLManager(LRUCache<K, V, Hash>* cache);
    ~TTLManager();
    
    void start();
    void stop();
    void wakeup();  // 立即触发一次清理
};



template <typename K, typename V, typename Hash>
LRUCache<K, V, Hash>::LRUCache(): LRUCache(DEFAULT_CAPACITY) {}



template <typename K, typename V, typename Hash>
LRUCache<K, V, Hash>::LRUCache(int capacity) : enable_ttl_(true) {
    // CPU cores * 2，平衡并发与内存开销
    shard_count_ = nextPowerOf2(std::thread::hardware_concurrency() * 2);
    // 例：8核 → 16分片 → 16倍理论并发度
    shards_.reserve(shard_count_);
    for (size_t i = 0; i < shard_count_; ++i) {
        shards_.emplace_back(std::make_unique<LRUShard<K, V>>(std::max(1UL, static_cast<size_t>(capacity) / shard_count_)));
    }
    
    // 初始化TTL管理器
    ttl_manager_ = std::make_unique<TTLManager<K, V, Hash>>(this);
    ttl_manager_->start();
}

template <typename K, typename V, typename Hash>
LRUCache<K, V, Hash>::LRUCache(size_t total_capacity, size_t shard_count) : enable_ttl_(true) {
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
    
    // 初始化TTL管理器
    ttl_manager_ = std::make_unique<TTLManager<K, V, Hash>>(this);
    ttl_manager_->start();
}


template <typename K, typename V, typename Hash>
size_t LRUCache<K, V, Hash>::getShard(const K& key) const {
    size_t hash_val = hasher_(key);
    return hash_val & (shard_count_ - 1);
}


template <typename K, typename V, typename Hash>
size_t LRUCache<K, V, Hash>::nextPowerOf2(size_t n) {
    if (n <= 1) return 1;
    n--;
    n |= n >> 1;  n |= n >> 2;  n |= n >> 4;
    n |= n >> 8;  n |= n >> 16; n |= n >> 32;
    return n + 1;
}


template <typename K, typename V, typename Hash>
bool LRUCache<K, V, Hash>::get(const K& key, V& out_value) {
    size_t shard_id = getShard(key);
    return shards_[shard_id]->get(key, out_value);
}
    
template <typename K, typename V, typename Hash>
void LRUCache<K, V, Hash>::put(const K& key, const V& value, int expire_time) {
    size_t shard_id = getShard(key);
    shards_[shard_id]->put(key, value, expire_time);
}

// 析构函数
template <typename K, typename V, typename Hash>
LRUCache<K, V, Hash>::~LRUCache() {
    if (ttl_manager_) {
        ttl_manager_->stop();
    }
}

// 删除方法
template <typename K, typename V, typename Hash>
bool LRUCache<K, V, Hash>::remove(const K& key) {
    size_t shard_id = getShard(key);
    return shards_[shard_id]->remove(key);
}

// TTL控制方法
template <typename K, typename V, typename Hash>
void LRUCache<K, V, Hash>::enableTTL(bool enable) {
    enable_ttl_.store(enable);
    if (enable && ttl_manager_) {
        ttl_manager_->wakeup();
    }
}

template <typename K, typename V, typename Hash>
void LRUCache<K, V, Hash>::disableTTL() {
    enable_ttl_.store(false);
}

// 统计信息
template <typename K, typename V, typename Hash>
typename LRUCache<K, V, Hash>::CacheStats LRUCache<K, V, Hash>::getStats() const {
    CacheStats total_stats;
    for (const auto& shard : shards_) {
        auto shard_stats = shard->getStats();
        total_stats.total_hits += shard_stats.hits;
        total_stats.total_misses += shard_stats.misses;
        total_stats.total_evictions += shard_stats.evictions;
        total_stats.expired_count += shard_stats.expired_count;
    }
    return total_stats;
}

// TTL管理器实现（内联）
template <typename K, typename V, typename Hash>
inline TTLManager<K, V, Hash>::TTLManager(LRUCache<K, V, Hash>* cache) 
    : cache_(cache), running_(false) {}

template <typename K, typename V, typename Hash>
inline TTLManager<K, V, Hash>::~TTLManager() {
    stop();
}

template <typename K, typename V, typename Hash>
inline void TTLManager<K, V, Hash>::start() {
    if (!running_.load()) {
        running_.store(true);
        cleanup_thread_ = std::thread(&TTLManager::cleanupLoop, this);
    }
}

template <typename K, typename V, typename Hash>
inline void TTLManager<K, V, Hash>::stop() {
    if (running_.load()) {
        running_.store(false);
        cv_.notify_all();
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
    }
}

template <typename K, typename V, typename Hash>
inline void TTLManager<K, V, Hash>::wakeup() {
    cv_.notify_one();
}

template <typename K, typename V, typename Hash>
inline void TTLManager<K, V, Hash>::cleanupLoop() {
    while (running_.load()) {
        // 只有在启用TTL时才进行清理
        if (cache_->enable_ttl_.load()) {
            // 遍历所有分片进行过期清理
            for (auto& shard : cache_->shards_) {
                shard->cleanupExpired();
            }
        }
        
        // 等待下一次清理周期
        std::unique_lock<std::mutex> lock(cv_mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(TTL_CLEANUP_INTERVAL_MS), 
                     [this] { return !running_.load(); });
    }
}



#endif