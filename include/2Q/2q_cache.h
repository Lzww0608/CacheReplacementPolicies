/*
@Author: Lzww  
@LastEditTime: 2025-7-11 20:51:57
@Description: 2Q算法实现
@Language: C++17
*/

#ifndef TWO_Q_CACHE_H
#define TWO_Q_CACHE_H

#include "2q_shard.h"
#include "../utils/bit_utils.h"

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

constexpr size_t DEFAULT_CACHE_CAPACITY = 1024 * 1024 * 1024;
constexpr size_t DEFAULT_SHARD_COUNT = 16;
constexpr int TTL_CLEANUP_INTERVAL_MS = 1000; // 1s
constexpr int DEFAULT_EXPIRE_TIME = 1000; // 1分钟，毫秒

template <typename K, typename V, typename Hash = std::hash<std::string>>
class TTLManager;

template <typename K, typename V, typename Hash = std::hash<std::string>>
class TwoQCache {
    friend class TTLManager<K, V, Hash>;

private:
    std::vector<std::unique_ptr<TwoQShard<K, V>>> shards_;
    size_t shard_count_;
    Hash hasher_;

    // TTL后台清理
    std::unique_ptr<TTLManager<K, V, Hash>> ttl_manager_;
    std::atomic<bool> enable_ttl_;
    
    size_t getShard(const K& key) const;
    
public:
    TwoQCache(size_t capacity = DEFAULT_CACHE_CAPACITY, size_t shard_count = 0);
    ~TwoQCache();

    bool get(const K& key, V& out_value);
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    bool remove(const K& key);
    
    // TTL控制
    void enableTTL(bool enable = true);
    void disableTTL();
};

// TTL管理器类
template <typename K, typename V, typename Hash>
class TTLManager {
private:
    TwoQCache<K, V, Hash>* cache_;
    std::thread cleanup_thread_;
    std::atomic<bool> running_;
    std::condition_variable cv_;
    std::mutex cv_mutex_;
    
    void cleanupLoop();
    
public:
    explicit TTLManager(TwoQCache<K, V, Hash>* cache);
    ~TTLManager();
    
    void start();
    void stop();
    void wakeup();  // 立即触发一次清理
};


template <typename K, typename V, typename Hash>
TwoQCache<K, V, Hash>::TwoQCache(size_t capacity, size_t shard_count) {
    if (shard_count == 0) {
        shard_count = nextPowerOf2(capacity / DEFAULT_CACHE_CAPACITY);
        if (shard_count < DEFAULT_SHARD_COUNT) {
            shard_count = DEFAULT_SHARD_COUNT;
        }
        shard_count = nextPowerOf2(shard_count);
    }
    shard_count_ = shard_count;
    shards_.reserve(shard_count_);
    for (size_t i = 0; i < shard_count_; ++i) {
        shards_.emplace_back(new TwoQShard<K, V>(DEFAULT_CACHE_CAPACITY / shard_count_));
    }
    ttl_manager_ = std::make_unique<TTLManager<K, V, Hash>>(this);
    enable_ttl_ = false;    
}

template <typename K, typename V, typename Hash>
TwoQCache<K, V, Hash>::~TwoQCache() {
    ttl_manager_.reset();
    shards_.clear();
}

template <typename K, typename V, typename Hash>
size_t TwoQCache<K, V, Hash>::getShard(const K& key) const {
    return hasher_(key) % shard_count_;
}

template <typename K, typename V, typename Hash>
bool TwoQCache<K, V, Hash>::get(const K& key, V& out_value) {
    size_t shard_idx = getShard(key);
    return shards_[shard_idx]->get(key, out_value);
}

template <typename K, typename V, typename Hash>
void TwoQCache<K, V, Hash>::put(const K& key, const V& value, int expire_time) {
    size_t shard_idx = getShard(key);
    shards_[shard_idx]->put(key, value);
}

template <typename K, typename V, typename Hash>
bool TwoQCache<K, V, Hash>::remove(const K& key) {
    size_t shard_idx = getShard(key);
    return shards_[shard_idx]->remove(key);
}

template <typename K, typename V, typename Hash>
void TwoQCache<K, V, Hash>::enableTTL(bool enable) {
    enable_ttl_ = enable;
    if (enable) {
        ttl_manager_->start();
    } else {
        ttl_manager_->stop();
    }
}

template <typename K, typename V, typename Hash>    
void TwoQCache<K, V, Hash>::disableTTL() {
    enable_ttl_ = false;
    ttl_manager_->stop();
}


// TTL管理器实现（内联）
template <typename K, typename V, typename Hash>
inline TTLManager<K, V, Hash>::TTLManager(TwoQCache<K, V, Hash>* cache) 
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