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
#include <unordered_map>
#include <chrono>
#include <stdexcept>

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

    bool remove(Node<K, V> *node);

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
LFUShard<K, V>::LFUShard(size_t capacity): capacity(capacity) {
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
bool LFUShard<K, V>::get(const K& key, V& out_value) {
    bool found = false;
    
    {
        std::shared_lock<std::shared_mutex> lock(mtx);
        auto it = keyToNode.find(key);
        if (it == keyToNode.end()) {
            return false;
        }

        Node<K, V> *node = it->second;
        auto now = std::chrono::steady_clock::now();
        if (node->expire_time > 0 && node->expire_time < now)  {
            expired_count_++;
            misses_++;
            remove(key);
            return false;
        }

        found = true;
    }

    if (found) {
        std::unique_lock<std::shared_mutex> lock(mtx);
        Node<K, V>* node = keyToNode[key];
        hits_++;
        out_value = node->value;
        remove(node);
        auto dummy = freqToList[min_freq];
        if (dummy->next == dummy) {
            reqToList.erase(min_freq);
            delete dummmy;
        }
        node->frequency++;
        pushToFront(node, node->frequency);
        return true;
    }

    return false;
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
    std::unique_lock<std::shared_lock> lock(mtx);
    auto node = keyToNode[key];
    remove(node);
    auto dummy = freToList[node->frequency];
    if (dummy->next == dummy) {
        freqToList.erase(node->frequency);
        delete dummy;
    }
    keyToNode.erase(key);
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
    keyToNode[node->key] = node;
}

template <typename K, typename V>
typename LFUShard<K, V>::ShardStats LFUShard<K, V>::getStats() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    ShardStats stats;
    stats.hits = hits_;
    stats.misses = misses_;
    stats.evictions = evictions_;
    stats.expired_count = expired_count_;
}
#endif