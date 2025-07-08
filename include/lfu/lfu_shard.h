/*
@Author: Lzww
@LastEditTime: 2025-7-3 21:40:00
@Description: LFU缓存分片实现
@Language: C++17
*/

#ifndef LFU_SHARD_H
#define LFU_SHARD_H

#include "../node.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <chrono>
#include <stdexcept>
#include <atomic>
#include <vector>

#define DEFAULT_EXPIRE_TIME 60000  // 1小时，毫秒


template <typename K, typename V>
class LFUShard {
private:
    std::unordered_map<K, Node<K, V>*> keyToNode;
    std::unordered_map<uint64_t, Node<K, V>*> freqToList;
    size_t capacity;
    mutable std::shared_mutex mtx;  // 读写分离锁
    std::atomic<uint64_t> hits_;
    std::atomic<uint64_t> misses_;
    std::atomic<uint64_t> evictions_;
    std::atomic<uint64_t> expired_count_;
    uint64_t min_freq = 0;  // 当前最小频率

    void remove(Node<K, V> *node);
    void evictLFU();
    void updateMinFreq();

public:
    LFUShard(size_t capacity);
    ~LFUShard();

    bool get(const K& key, V& out_value);
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    bool remove(const K& key);

    void pushToFront(Node<K, V> *node, uint64_t frequency);
    void cleanupExpired();  // TTL清理方法

    struct ShardStats {
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t evictions = 0;
        uint64_t expired_count = 0;
    };
    ShardStats getStats() const;
};

template <typename K, typename V>
LFUShard<K, V>::LFUShard(size_t capacity): capacity(capacity), hits_(0), misses_(0), evictions_(0), expired_count_(0) {
    keyToNode.reserve(capacity);
}

template <typename K, typename V>
LFUShard<K, V>::~LFUShard() {
    for (auto& pair : keyToNode) {
        delete pair.second;
    }
    keyToNode.clear();
    for (auto& pair : freqToList) {
        delete pair.second;
    }
    freqToList.clear();
    capacity = 0;
}

template <typename K, typename V>
void LFUShard<K, V>::evictLFU() {
    
    auto it = freqToList.find(min_freq);
    if (it == freqToList.end()) {
        return;
    }

    auto dummy = it->second;
    if (dummy->next == dummy) {
        return;
    }

    auto victim = dummy->prev;
    remove(victim);
    keyToNode.erase(victim->key);
    delete victim;
    evictions_++;

    if (dummy->next == dummy) {
        freqToList.erase(min_freq);
        delete dummy;
        updateMinFreq();
    }

    return;
}

template <typename K, typename V>
void LFUShard<K, V>::updateMinFreq() {
    if (freqToList.empty()) {
        min_freq = 0;
        return;
    }

    min_freq = freqToList.begin()->first;
    for (const auto& pair : freqToList) {
        if (pair.first < min_freq) {
            min_freq = pair.first;
        }
    }
    return;
}

template <typename K, typename V>
bool LFUShard<K, V>::get(const K& key, V& out_value) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    auto it = keyToNode.find(key);
    if (it == keyToNode.end()) {
        misses_++;
        return false;
    }

    Node<K, V> *node = it->second;
    auto now = std::chrono::steady_clock::now();
    
    // 检查过期
    if (node->expire_time != std::chrono::steady_clock::time_point::max() && node->expire_time < now) {
        expired_count_++;
        misses_++;
        // 安全删除过期节点
        remove(node);
        
        // 检查是否需要更新min_freq
        auto dummy = freqToList[node->frequency];
        if (dummy->next == dummy) {
            freqToList.erase(node->frequency);
            delete dummy;
            if (node->frequency == min_freq) {
                updateMinFreq();
            }
        }
        
        keyToNode.erase(it);
        delete node;
        return false;
    }

    hits_++;
    out_value = node->value;
    
    uint64_t old_frequency = node->frequency;
    bool need_update_min_freq = false;
    
    remove(node);
    
    auto dummy = freqToList[old_frequency];
    if (dummy->next == dummy) {
        freqToList.erase(old_frequency);
        delete dummy;
        if (old_frequency == min_freq) {
            need_update_min_freq = true;
        }
    }

    node->frequency++;
    pushToFront(node, node->frequency);
    
    if (need_update_min_freq) {
        updateMinFreq();
    }
    
    return true;
}

template <typename K, typename V>
void LFUShard<K, V>::put(const K& key, const V& value, int expired_time) {
    std::unique_lock<std::shared_mutex> lock(mtx);

    auto it = keyToNode.find(key);
    if (it != keyToNode.end()) {
        auto node = it->second;
        node->value = value;
        auto now = std::chrono::steady_clock::now();
        node->expire_time = now + std::chrono::milliseconds(expired_time);
        
        return;
    } 

    
    // 检查容量限制
    if (keyToNode.size() >= capacity) {
        evictLFU();
    }

    auto node = new Node<K, V>(key, value, expired_time);
    node->frequency = 1;
    keyToNode[key] = node;
    pushToFront(node, 1);
    min_freq = 1;
    
    return;
}

template <typename K, typename V>
void LFUShard<K, V>::remove(Node<K, V> *node) {
    if (node == nullptr) {
        return;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;
}

template <typename K, typename V>
bool LFUShard<K, V>::remove(const K& key) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    auto iter = keyToNode.find(key);
    if (iter == keyToNode.end()) {
        return false;
    }
    auto node = iter->second;
    remove(node);

    auto dummy = freqToList[node->frequency];
    if (dummy->next == dummy) {
        freqToList.erase(node->frequency);
        delete dummy;
        if (node->frequency == min_freq) {
            updateMinFreq();
        }
    }

    keyToNode.erase(iter);
    delete node;
    return true;
}

template <typename K, typename V>
void LFUShard<K, V>::pushToFront(Node<K, V>* node, uint64_t frequency) {
    if (node == nullptr) {
        throw std::runtime_error("Node is nullptr");
    }

    auto it = freqToList.find(frequency);
    Node<K, V>* head = nullptr;
    
    if (it == freqToList.end()) {
        // 创建新的频率链表
        auto dummy = new Node<K, V>();
        dummy->next = dummy;
        dummy->prev = dummy;
        freqToList[frequency] = dummy;
        head = dummy;  // 直接使用新创建的dummy节点
    } else {
        head = it->second;  // 使用现有的头节点
    }
    
    // 将节点插入到链表头部
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

template <typename K, typename V>
typename LFUShard<K, V>::ShardStats LFUShard<K, V>::getStats() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    ShardStats stats;
    stats.hits = hits_;
    stats.misses = misses_;
    stats.evictions = evictions_;
    stats.expired_count = expired_count_;
    return stats;
}

template <typename K, typename V>
void LFUShard<K, V>::cleanupExpired() {
    std::unique_lock<std::shared_mutex> lock(mtx);
    auto now = std::chrono::steady_clock::now();

    std::vector<K> expired_keys;
    for (auto& pair : keyToNode) {
        if (pair.second->expire_time != std::chrono::steady_clock::time_point::max() && pair.second->expire_time <= now) {
            expired_keys.push_back(pair.first);
        }
    }

    for (const auto& key : expired_keys) {
        auto it = keyToNode.find(key);
        if (it != keyToNode.end()) {
            auto node = it->second;
            remove(node);
            
            auto dummy = freqToList[node->frequency];
            if (dummy->next == dummy) {
                freqToList.erase(node->frequency);
                delete dummy;
                if (node->frequency == min_freq) {
                    updateMinFreq();
                }
            }

            keyToNode.erase(it);
            delete node;
            expired_count_++;
        }
    }
}

#endif