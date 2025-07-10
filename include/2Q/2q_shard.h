/*
@Author: Lzww  
@LastEditTime: 2025-7-10 21:25:12
@Description: 2Q算法
@Language: C++17
*/

#ifndef TWO_Q_SHARD_H
#define TWO_Q_SHARD_H

#include "../node.h"

#include <unordered_map>
#include <string>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <cstdint>
#include <vector>

constexpr size_t DEFAULT_CAPACITY = 1024 * 1024;

template <typename K, typename V, typename Hash = std::hash<std::string>>
class TwoQShard {
public:
    TwoQShard(size_t capacity = DEFAULT_CAPACITY);
    ~TwoQShard();

    void put(const K& key, const V& value);
    bool get(const K& key, V& value);
    void remove(const K& key);
    void clear();
    void cleanupExpired(); // TTL清理方法

private:
    size_t fifo_capacity_;
    size_t lru_capacity_;
    size_t expired_capacity_;
    size_t fifo_size_;
    size_t lru_size_;
    size_t expired_size_;

    Node<K, V>* fifo_head_;
    Node<K, V>* lru_head_;
    Node<K, V>* expired_head_;

    std::unordered_map<K, Node<K, V>*, Hash> fifo_cache_;
    std::unordered_map<K, Node<K, V>*, Hash> lru_cache_;
    std::unordered_map<K, Node<K, V>*, Hash> expired_cache_;
    std::mutex fifo_mutex_;
    std::mutex lru_mutex_;
    std::mutex expired_mutex_;

    void remove(Node<K, V>* node);

    // evict方法假设调用者已持有必要的锁
    void fifo_evict();
    void lru_evict();
    void expired_evict();
    
    // 辅助方法：将节点移动到expired队列
    void move_to_expired(Node<K, V>* node);
};

template <typename K, typename V, typename Hash>
TwoQShard<K, V, Hash>::TwoQShard(size_t capacity) : fifo_capacity_(capacity), lru_capacity_(capacity), expired_capacity_(capacity) {
    fifo_size_ = 0;
    lru_size_ = 0;
    expired_size_ = 0;

    fifo_head_ = new Node<K, V>();
    lru_head_ = new Node<K, V>();
    expired_head_ = new Node<K, V>();

    fifo_head_->next = fifo_head_;
    fifo_head_->prev = fifo_head_;

    lru_head_->next = lru_head_;
    lru_head_->prev = lru_head_;

    expired_head_->next = expired_head_;
    expired_head_->prev = expired_head_;
}

template <typename K, typename V, typename Hash>
TwoQShard<K, V, Hash>::~TwoQShard() {
    clear();
    delete fifo_head_;
    delete lru_head_;
    delete expired_head_;
}

template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::clear() {
    std::scoped_lock<std::mutex, std::mutex, std::mutex> lock(fifo_mutex_, lru_mutex_, expired_mutex_);
    
    for (auto& pair : fifo_cache_) {
        remove(pair.second);
        delete pair.second;
    }
    for (auto& pair : lru_cache_) {
        remove(pair.second);
        delete pair.second;
    }
    for (auto& pair : expired_cache_) {
        remove(pair.second);
        delete pair.second;
    }
    
    fifo_cache_.clear();
    lru_cache_.clear();
    expired_cache_.clear();

    fifo_size_ = 0;
    lru_size_ = 0;
    expired_size_ = 0;
}

template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::remove(Node<K, V>* node) {
    if (node == nullptr) return;
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = nullptr;
    node->prev = nullptr;
}

template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::remove(const K& key) {
    std::scoped_lock<std::mutex, std::mutex, std::mutex> lock(fifo_mutex_, lru_mutex_, expired_mutex_);
    
    auto it = lru_cache_.find(key);
    if (it != lru_cache_.end()) {
        remove(it->second);
        delete it->second;
        lru_size_--;
        lru_cache_.erase(key);
        return;
    }
    
    it = fifo_cache_.find(key);
    if (it != fifo_cache_.end()) {
        remove(it->second);
        delete it->second;
        fifo_size_--;
        fifo_cache_.erase(key);
        return;
    }
    
    it = expired_cache_.find(key);
    if (it != expired_cache_.end()) {
        remove(it->second);
        delete it->second;
        expired_size_--;
        expired_cache_.erase(key);
        return;
    }
}

template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::put(const K& key, const V& value) {
    std::scoped_lock<std::mutex, std::mutex, std::mutex> lock(fifo_mutex_, lru_mutex_, expired_mutex_);
    
    auto it = lru_cache_.find(key);
    if (it != lru_cache_.end()) {
        auto node = it->second;
        remove(node);
        node->prev = lru_head_;
        node->next = lru_head_->next;
        node->prev->next = node;
        node->next->prev = node;
        node->value = std::move(value);
        return;
    }

    it = fifo_cache_.find(key);
    if (it != fifo_cache_.end()) {
        auto node = it->second;
        remove(node);
        node->prev = lru_head_;
        node->next = lru_head_->next;
        node->prev->next = node;
        node->next->prev = node;
        node->value = std::move(value);
        fifo_size_--;
        fifo_cache_.erase(node->key);
        lru_size_++;
        lru_cache_.emplace(key, node);

        if (lru_size_ > lru_capacity_) {
            lru_evict();
        }
        return;
    }

    it = expired_cache_.find(key);
    if (it != expired_cache_.end()) {
        auto node = it->second;
        remove(node);
        node->prev = lru_head_;
        node->next = lru_head_->next;
        node->prev->next = node;
        node->next->prev = node;
        node->value = std::move(value);
        expired_size_--;
        expired_cache_.erase(node->key);
        lru_size_++;
        lru_cache_.emplace(key, node);

        if (lru_size_ > lru_capacity_) {
            lru_evict();
        }
        return;
    }

    auto node = new Node<K, V>(key, value);
    node->prev = fifo_head_;
    node->next = fifo_head_->next;
    node->prev->next = node;
    node->next->prev = node;
    fifo_size_++;
    fifo_cache_.emplace(key, node);

    if (fifo_size_ > fifo_capacity_) {
        fifo_evict();
    }
}

template <typename K, typename V, typename Hash>
bool TwoQShard<K, V, Hash>::get(const K& key, V& value) {
    {
        std::unique_lock<std::mutex> lock(lru_mutex_);
        auto it = lru_cache_.find(key);
        if (it != lru_cache_.end()) {
            auto node = it->second;
            remove(node);
            node->prev = lru_head_;
            node->next = lru_head_->next;
            node->prev->next = node;
            node->next->prev = node;
            value = node->value;
            return true;
        }
    }

    {
        std::scoped_lock<std::mutex, std::mutex> lock(fifo_mutex_, lru_mutex_);
        auto it = fifo_cache_.find(key);
        if (it != fifo_cache_.end()) {
            auto node = it->second;
            remove(node);
            node->prev = lru_head_;
            node->next = lru_head_->next;
            node->prev->next = node;
            node->next->prev = node;
            value = node->value;
            fifo_size_--;
            fifo_cache_.erase(node->key);
            lru_size_++;
            lru_cache_.emplace(key, node);

            if (lru_size_ > lru_capacity_) {
                lru_evict();
            }
            return true;
        }
    }

    {
        std::scoped_lock<std::mutex, std::mutex> lock(expired_mutex_, lru_mutex_);
        auto it = expired_cache_.find(key);
        if (it != expired_cache_.end()) {
            auto node = it->second;
            remove(node);
            node->prev = lru_head_;
            node->next = lru_head_->next;
            node->prev->next = node;
            node->next->prev = node;
            value = node->value;
            expired_size_--;
            expired_cache_.erase(node->key);
            lru_size_++;
            lru_cache_.emplace(key, node);

            if (lru_size_ > lru_capacity_) {
                lru_evict();
            }
            return true;
        }
    }

    return false; // Key not found
}
    
// TTL清理方法：清理过期节点
template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::cleanupExpired() {
    std::scoped_lock<std::mutex, std::mutex, std::mutex> lock(fifo_mutex_, lru_mutex_, expired_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    std::vector<K> expired_keys;
    
    // 收集过期的键
    for (auto& pair : expired_cache_) {
        if (pair.second->expire_time <= now) {
            expired_keys.push_back(pair.first);
        }
    }
    
    // 删除过期节点
    for (const auto& key : expired_keys) {
        auto it = expired_cache_.find(key);
        if (it != expired_cache_.end()) {
            remove(it->second);
            delete it->second;
            expired_cache_.erase(it);
            expired_size_--;
        }
    }
}

// FIFO淘汰：将最旧的节点移到expired队列
template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::fifo_evict() {
    // 假设调用者已持有fifo_mutex_和expired_mutex_
    if (fifo_size_ == 0) return;
    
    Node<K, V>* victim = fifo_head_->prev; // 最旧的节点
    if (victim == fifo_head_) return;
    
    remove(victim);
    fifo_cache_.erase(victim->key);
    fifo_size_--;
    
    // 移动到expired队列
    move_to_expired(victim);
}

// LRU淘汰：将最久未使用的节点移到expired队列
template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::lru_evict() {
    // 假设调用者已持有lru_mutex_和expired_mutex_
    if (lru_size_ == 0) return;
    
    Node<K, V>* victim = lru_head_->prev; // 最久未使用的节点
    if (victim == lru_head_) return;
    
    remove(victim);
    lru_cache_.erase(victim->key);
    lru_size_--;
    
    // 移动到expired队列
    move_to_expired(victim);
}

// Expired淘汰：直接删除过期节点
template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::expired_evict() {
    // 假设调用者已持有expired_mutex_
    if (expired_size_ == 0) return;
    
    Node<K, V>* victim = expired_head_->prev; // 最旧的过期节点
    if (victim == expired_head_) return;
    
    remove(victim);
    expired_cache_.erase(victim->key);
    expired_size_--;
    delete victim; // 直接删除
}

// 辅助方法：将节点移动到expired队列
template <typename K, typename V, typename Hash>
void TwoQShard<K, V, Hash>::move_to_expired(Node<K, V>* node) {
    // 假设调用者已持有expired_mutex_
    if (node == nullptr) return;
    
    // 设置过期时间戳（用于异步清理）
    node->expire_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000); // 5秒后过期
    
    // 检查expired队列容量
    if (expired_size_ >= expired_capacity_) {
        expired_evict(); // 淘汰最旧的过期节点
    }
    
    // 将节点插入到expired队列头部
    node->prev = expired_head_;
    node->next = expired_head_->next;
    node->prev->next = node;
    node->next->prev = node;
    
    expired_cache_.emplace(node->key, node);
    expired_size_++;
}

#endif