/*
@Author: Lzww
@LastEditTime: 2025-7-7 10:00:00
@Description: LFU缓存实现
@Language: C++17
*/

#ifndef LFU_CACHE_H
#define LFU_CACHE_H

#define DEFAULT_CAPACITY 1024
#define DEFAULT_EXPIRE_TIME 60000  // 1分钟，毫秒
#define TTL_CLEANUP_INTERVAL_MS 1000  // TTL清理间隔

#include <vector>
#include <memory>
#include <atomic>
#include <cstdint>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <algorithm>

#include "lfu_shard.h"

template <typename K, typename V, typename Hash = std::hash<K>>
class TTLManager;

template <typename K, typename V, typename Hash = std::hash<K>>
class LFUCache {
private:
    friend class TTLManager<K, V, Hash>;

    std::vector<std::unique_ptr<LFUShard<K, V>>> shards_;
    size_t shard_count_;
    Hash hasher_;

    size_t getShard(const K& key) const;
    static size_t nextPowerOf2(size_t n);

    // TTL后台清理
    std::unique_ptr<TTLManager<K, V, Hash>> ttl_manager_;
    std::atomic<bool> enable_ttl_;
public:
    LFUCache();
    LFUCache(int capacity);
    explicit LFUCache(size_t total_capacity, size_t shard_count = 0);
    ~LFUCache();

    bool get(const K& key, V& out_value);
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    bool remove(const K& key);
    
    // TTL控制
    void enableTTL(bool enable = true);
    void disableTTL();

    // 统计信息
    struct CacheStats {
        uint64_t total_hits = 0;
        uint64_t total_misses = 0;
        uint64_t total_evictions = 0;
        uint64_t expired_count = 0;
        double hit_rate() const {
            return total_hits + total_misses > 0 ?
                static_cast<double>(total_hits) / (total_hits + total_misses) : 0.0;
        }
    };

    CacheStats getStats() const;
};


template <typename K, typename V, typename Hash>
class TTLManager {
private:
    LFUCache<K, V, Hash>* cache_;
    std::thread cleanup_thread_;
    std::atomic<bool> running_;
    std::condition_variable cv_;
    std::mutex cv_mutex_;

    void cleanupLoop();

public:
    explicit TTLManager(LFUCache<K, V, Hash>* cache);
    ~TTLManager();

    void start();
    void stop();
    void wakeup(); // clean up immediately
};

template <typename K, typename V, typename Hash>
LFUCache<K, V, Hash>::LFUCache(): LFUCache(DEFAULT_CAPACITY) {}

template <typename K, typename V, typename Hash>
LFUCache<K, V, Hash>::LFUCache(int capacity): enable_ttl_(true) {
    // CPU cores * 2, balance concurrency and memory overhead
    shard_count_ = nextPowerOf2(std::thread::hardware_concurrency() * 2);
    shards_.reserve(shard_count_);
    for (size_t i = 0; i < shard_count_; i++) {
        shards_.emplace_back(std::make_unique<LFUShard<K, V>> (std::max(1UL, static_cast<size_t>(capacity) / shard_count_)));
    }

    ttl_manager_ = std::make_unique<TTLManager<K, V, Hash>>(this);
    ttl_manager_->start();
}

template <typename K, typename V, typename Hash>
LFUCache<K, V, Hash>::LFUCache(size_t total_capacity, size_t shard_count) : enable_ttl_(true) {
    if (shard_count == 0) {
        shard_count = nextPowerOf2(std::thread::hardware_concurrency() * 2);
    }
    shard_count_ = shard_count;
    
    size_t shard_capacity = std::max(1UL, total_capacity / shard_count_);
    
    shards_.reserve(shard_count_);
    for (size_t i = 0; i < shard_count_; ++i) {
        shards_.emplace_back(std::make_unique<LFUShard<K, V>>(shard_capacity));
    }
    
    ttl_manager_ = std::make_unique<TTLManager<K, V, Hash>>(this);
    ttl_manager_->start();
}

template <typename K, typename V, typename Hash>
size_t LFUCache<K, V, Hash>::getShard(const K& key) const {
    size_t hash_val = hasher_(key);
    return hash_val & (shard_count_ - 1);
}

template <typename K, typename V, typename Hash>
size_t LFUCache<K, V, Hash>::nextPowerOf2(size_t n) {
    if (n <= 1) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

template <typename K, typename V, typename Hash>
LFUCache<K, V, Hash>::~LFUCache() {
    if (enable_ttl_) {
        disableTTL();
        ttl_manager_->stop();
    }
}

template <typename K, typename V, typename Hash>
bool LFUCache<K, V, Hash>::get(const K& key, V& out_value) {
    size_t shard_id = getShard(key);
    return shards_[shard_id]->get(key, out_value);
}

template <typename K, typename V, typename Hash>
void LFUCache<K, V, Hash>::put(const K& key, const V& value, int expire_time) {
    size_t shard_id = getShard(key);
    shards_[shard_id]->put(key, value, expire_time);
}

template <typename K, typename V, typename Hash>
bool LFUCache<K, V, Hash>::remove(const K& key) {
    size_t shard_id = getShard(key);
    return shards_[shard_id]->remove(key);
}

template <typename K, typename V, typename Hash>
void LFUCache<K, V, Hash>::enableTTL(bool enable) {
    enable_ttl_.store(enable);
    // explicit operator bool() const
    if (enable && ttl_manager_) {
        ttl_manager_->wakeup();
    }
}

template <typename K, typename V, typename Hash>
void LFUCache<K, V, Hash>::disableTTL() {
    enable_ttl_.store(false);
    if (ttl_manager_) {
        ttl_manager_->stop();
    }
}

template <typename K, typename V, typename Hash>
typename LFUCache<K, V, Hash>::CacheStats LFUCache<K, V, Hash>::getStats() const {
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

template <typename K, typename V, typename Hash>
TTLManager<K, V, Hash>::TTLManager(LFUCache<K, V, Hash>* cache) : cache_(cache), running_(false) {}

template <typename K, typename V, typename Hash>
TTLManager<K, V, Hash>::~TTLManager() {
    stop();
}

template <typename K, typename V, typename Hash>
void TTLManager<K, V, Hash>::start() {
    if (!running_.exchange(true)) {
        cleanup_thread_ = std::thread(&TTLManager::cleanupLoop, this);
    }
}


template <typename K, typename V, typename Hash>
void TTLManager<K, V, Hash>::stop() {
    if (running_.exchange(false)) {
        cv_.notify_all();
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
    }
}

template <typename K, typename V, typename Hash>
void TTLManager<K, V, Hash>::wakeup() {
    std::unique_lock<std::mutex> lock(cv_mutex_);
    cv_.notify_all();
}

template <typename K, typename V, typename Hash>
void TTLManager<K, V, Hash>::cleanupLoop() {
    while (running_.load()) {
        if (cache_->enable_ttl_.load()) {
            for (auto& shard : cache_->shards_) {
                shard->cleanupExpired();
            }
        }
        std::unique_lock<std::mutex> lock(cv_mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(TTL_CLEANUP_INTERVAL_MS), [this] { return !running_.load(); });
    }
}
#endif