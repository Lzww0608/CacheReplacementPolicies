/*
@Author: Lzww
@LastEditTime: 2025-7-3 21:39:55
@Description: LRU缓存分片实现
@Language: C++17
*/

#ifndef LRU_SHARD_H
#define LRU_SHARD_H

#include "../utils/node.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <chrono>
#include <stdexcept>
#include <cstdint>
#include <utility>

#define DEFAULT_EXPIRE_TIME 60000  // 1分钟，毫秒



template<typename K, typename V, typename Hash = std::hash<std::string>>
class LRUShard {
private:
    std::unordered_map<K, Node<K, V>*, Hash> keyToNode;
    Node<K, V> *head;
    size_t capacity;
    mutable std::shared_mutex mtx;  // 读写分离锁
    
    // 统计信息
    mutable size_t hits_ = 0;
    mutable size_t misses_ = 0;
    mutable size_t evictions_ = 0;
    mutable size_t expired_count_ = 0;
    
    void remove(Node<K, V> *node);
public:
    LRUShard(size_t capacity);
    ~LRUShard();

    size_t size() const;
    bool contains(const K& key) const;
    bool full() const;
    void resize(size_t new_capacity);
    
    bool get(const K& key, V& out_value);
    void put(const K& key, const V& value, int expire_time = DEFAULT_EXPIRE_TIME);
    bool remove(const K& key);

    Node<K, V>* evict();
    
    void pushToFront(Node<K, V> *node);
    void cleanupExpired();  // TTL清理方法
    
    // 统计信息
    struct ShardStats {
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t evictions = 0;
        uint64_t expired_count = 0;
    };
    ShardStats getStats() const;
};


template <typename K, typename V, typename Hash>   
LRUShard<K, V, Hash>::LRUShard(size_t capacity): capacity(capacity) {
    head = new Node<K, V>();  // 使用默认构造函数
    head->next = head;
    head->prev = head;
}

template <typename K, typename V, typename Hash>
LRUShard<K, V, Hash>::~LRUShard() {
    // 清理所有节点
    while (head->next != head) {
        Node<K, V>* node = head->next;
        remove(node);
        delete node;
    }
    delete head;
}


// 1. std::optional<V> LRUShard<K, V>::get(const K& key)
// 2. const V* LRUShard<K, V>::get(const K& key)
// 3 bool LRUShard<K, V>::get(const K& key, V& out_value) 
// 零拷贝，与C风格API兼容
template <typename K, typename V, typename Hash>
bool LRUShard<K, V, Hash>::get(const K& key, V& out_value) {
    Node<K, V> *node = nullptr;
    bool found = false;
    
    // 第一阶段：使用共享锁进行查找和过期检查
    {
        std::shared_lock<std::shared_mutex> shared_lock(mtx);
        auto it = keyToNode.find(key);
        if (it == keyToNode.end()) {
            ++misses_;
            return false;
        }
        
        node = it->second;
        node->frequency++;
        found = true;
        
        // 检查是否过期
        auto now = std::chrono::steady_clock::now();
        if (node->expire_time < now) {
            // 节点已过期，需要在独占锁下删除，但不返回值
            // 这里不设置out_value，让第二阶段处理删除
        } else {
            // 未过期，获取值（在共享锁下安全）
            out_value = node->value;
        }
    }
    
    // 第二阶段：如果需要修改（移动节点或删除过期节点），使用独占锁
    if (found) {
        std::unique_lock<std::shared_mutex> unique_lock(mtx);
        
        // 双重检查：确保节点仍然存在
        auto it = keyToNode.find(key);
        if (it == keyToNode.end()) {
            ++misses_;
            return false;
        }
        
        node = it->second;
        
        // 再次检查过期状态
        auto now = std::chrono::steady_clock::now();
        if (node->expire_time < now) {
            // 节点已过期，删除它
            remove(node);
            keyToNode.erase(it);
            delete node;
            ++expired_count_;
            ++misses_;
            return false;  // 过期节点不返回值
        }
        
        // 节点有效，移动到前面（LRU更新）
        remove(node);
        pushToFront(node);
        ++hits_;
        
        return true;
    }
    
    return false;
}

template <typename K, typename V, typename Hash>
void LRUShard<K, V, Hash>::put(const K& key, const V& value, int expire_time) {
    std::unique_lock<std::shared_mutex> lock(mtx);  // 写操作使用独占锁
    
    // 检查是否已存在
    auto it = keyToNode.find(key);
    if (it != keyToNode.end()) {
        // 更新现有节点
        Node<K, V>* node = it->second;
        node->value = std::move(value);
        node->expire_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(expire_time);
        remove(node);
        pushToFront(node);
        return;
    }

    // 检查容量限制
    if (keyToNode.size() >= capacity) {
        Node<K, V> *node = head->prev;
        remove(node);
        keyToNode.erase(node->key);
        delete node;
        ++evictions_;
    }

    // 创建新节点
    Node<K, V> *newNode = new Node<K, V>(key, value, expire_time);
    pushToFront(newNode);
    keyToNode[key] = newNode;
}

// 私有remove方法实现
template <typename K, typename V, typename Hash>
void LRUShard<K, V, Hash>::remove(Node<K, V> *node) {
    if (node == nullptr) return;
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

template <typename K, typename V, typename Hash>
void LRUShard<K, V, Hash>::pushToFront(Node<K, V> *node) {
    if (node == nullptr) {
        throw std::runtime_error("Node is nullptr");
    }

    node->prev = head;
    node->next = head->next;
    head->next->prev = node;
    head->next = node;
}

// 公有remove方法
template <typename K, typename V, typename Hash>
bool LRUShard<K, V, Hash>::remove(const K& key) {
    std::unique_lock<std::shared_mutex> lock(mtx);  // 写操作使用独占锁
    auto it = keyToNode.find(key);
    if (it == keyToNode.end()) {
        return false;
    }
    
    Node<K, V>* node = it->second;
    remove(node);
    keyToNode.erase(it);
    delete node;
    return true;
}

// TTL清理方法
template <typename K, typename V, typename Hash>
void LRUShard<K, V, Hash>::cleanupExpired() {
    std::unique_lock<std::shared_mutex> lock(mtx);  // 写操作使用独占锁
    auto now = std::chrono::steady_clock::now();
    
    // 从尾部开始扫描（最不常用的）
    Node<K, V>* current = head->prev;
    while (current != head) {
        Node<K, V>* prev_node = current->prev;
        
        if (current->expire_time < now) {
            keyToNode.erase(current->key);
            remove(current);  
            delete current;
            ++expired_count_;
        }
        
        current = prev_node; 
    }
}

// 统计信息方法
template <typename K, typename V, typename Hash>
typename LRUShard<K, V, Hash>::ShardStats LRUShard<K, V, Hash>::getStats() const {
    std::shared_lock<std::shared_mutex> lock(mtx);  // 只读操作使用共享锁
    ShardStats stats;
    stats.hits = hits_;
    stats.misses = misses_;
    stats.evictions = evictions_;
    stats.expired_count = expired_count_;
    return stats;
}

template <typename K, typename V, typename Hash>
size_t LRUShard<K, V, Hash>::size() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return keyToNode.size();
}

template <typename K, typename V, typename Hash>
bool LRUShard<K, V, Hash>::contains(const K& key) const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return keyToNode.find(key) != keyToNode.end();
}

template <typename K, typename V, typename Hash>
bool LRUShard<K, V, Hash>::full() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return keyToNode.size() >= capacity;
}

template <typename K, typename V, typename Hash>
Node<K, V>* LRUShard<K, V, Hash>::evict() {
    std::unique_lock<std::shared_mutex> lock(mtx);
    if (head->next == head) {
        return nullptr;
    }
    Node<K, V>* node = head->next;
    remove(node);
    keyToNode.erase(node->key);
    ++evictions_;
    return node;
}

template <typename K, typename V, typename Hash>
void LRUShard<K, V, Hash>::resize(size_t new_capacity) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    capacity = new_capacity;
    while (keyToNode.size() > capacity) {
        Node<K, V>* node = head->prev;
        remove(node);
        keyToNode.erase(node->key);
        delete node;
    }
}


#endif