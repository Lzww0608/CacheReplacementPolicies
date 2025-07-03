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
    Node<K, V> *head;
    size_t capacity;
    mutable std::shared_mutex mtx;  // 读写分离锁

public:
    LFUShard(size_t capacity);
    ~LFUShard();

    bool get(const K& key, V& out_value);
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    bool remove(const K& key);

    void pushToFront(Node<K, V> *node);
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
    head = new Node<K, V>();
    head->next = head;
    head->prev = head;
    keyToNode.reserve(capacity);
}

template <typename K, typename V>
LFUShard<K, V>::~LFUShard() {
    Node<K, V> *current = head->next;
    while (current != head) {
        auto next = current->next;
        delete current;
        current = next;
    }
    delete head;
    head = nullptr;
    keyToNode.clear();
    capacity = 0;
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