/*
@Author: Lzww  
@LastEditTime: 2025-7-9 21:29:34
@Description: Clock缓存
@Language: C++17
*/

#ifndef CLOCK_CACHE_H
#define CLOCK_CACHE_H

#include "../utils/node.h"

#include <unordered_map>
#include <string>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

#define DEFAULT_CAPACITY 1024 * 1024

template <typename K, typename V, typename Hash = std::hash<std::string>>
class ClockCache {
public:
    ClockCache(size_t capacity = DEFAULT_CAPACITY);
    ~ClockCache();
    void put(const K& key, const V& value);
    bool get(const K& key, V& value) const;
    bool contains(const K& key) const;
    void remove(const K& key);
    size_t size() const;
    void clear();

private:
    size_t capacity_;
    size_t size_;
    std::unordered_map<K, Node<K, V>*> keyToNode_;
    mutable std::shared_mutex mutex_;
    Node<K, V>* clock_head_;
    Node<K, V>* clock_pointer_; /* Clock算法指针 */

    void remove(Node<K, V>* node);
    void evict();
};

template <typename K, typename V, typename Hash>
ClockCache<K, V, Hash>::ClockCache(size_t capacity) : capacity_(capacity), size_(0), clock_head_(nullptr) {
    if (capacity_ <= 0) {
        throw std::invalid_argument("Capacity must be greater than 0");
    }
    clock_head_ = new Node<K, V>();
    clock_head_->next = clock_head_;
    clock_head_->prev = clock_head_;
    keyToNode_.reserve(capacity_);
    clock_pointer_ = clock_head_;
}

template <typename K, typename V, typename Hash>
ClockCache<K, V, Hash>::~ClockCache() {
    clear();
    delete clock_head_;
}

template <typename K, typename V, typename Hash>
void ClockCache<K, V, Hash>::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto& [key, node] : keyToNode_) {
        delete node;
    }
    keyToNode_.clear();
    size_ = 0;
    clock_head_->next = clock_head_;
    clock_head_->prev = clock_head_;
}

template <typename K, typename V, typename Hash>
void ClockCache<K, V, Hash>::put(const K& key, const V& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = keyToNode_.find(key);
    if (it != keyToNode_.end()) {
        it->second->value = std::move(value);
        it->second->clock_bit = 1;
    } else {
        Node<K, V>* new_node = new Node<K, V>(key, value);
        if (size_ >= capacity_) {
            evict();
        }
        keyToNode_[key] = new_node;
        size_++;
        new_node->clock_bit = 1;  // 修复：设置新节点的clock_bit而不是clock_pointer_的
        new_node->next = clock_head_->next;
        new_node->prev = clock_head_;
        new_node->next->prev = new_node;
        new_node->prev->next = new_node;
    }
}

template <typename K, typename V, typename Hash>
bool ClockCache<K, V, Hash>::get(const K& key, V& value) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = keyToNode_.find(key);
    if (it == keyToNode_.end()) {
        return false;
    }

    Node<K, V>* node = it->second;
    value = node->value;
    node->clock_bit = 1;
    return true;
}

template <typename K, typename V, typename Hash>
bool ClockCache<K, V, Hash>::contains(const K& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);   
    return keyToNode_.find(key) != keyToNode_.end();
}

template <typename K, typename V, typename Hash>
size_t ClockCache<K, V, Hash>::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return size_;
}


template <typename K, typename V, typename Hash>
void ClockCache<K, V, Hash>::remove(const K& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = keyToNode_.find(key);
    if (it == keyToNode_.end()) {
        return;
    }

    Node<K, V>* node = it->second;
    if (node == clock_pointer_) {
        clock_pointer_ = node->next;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;
    keyToNode_.erase(it);
    delete node;
    size_--;
}

template <typename K, typename V, typename Hash>
void ClockCache<K, V, Hash>::evict() {
    Node<K, V>* start_pointer = clock_pointer_;
    
    while (true) {
        if (clock_pointer_ == clock_head_) {
            clock_pointer_ = clock_pointer_->next;
            if (clock_pointer_ == clock_head_) {
                return;
            }
        }

        if (clock_pointer_->clock_bit == 0) {
            Node<K, V>* victim = clock_pointer_;
            clock_pointer_ = clock_pointer_->next;
            remove(victim);
            return;
        }

        clock_pointer_->clock_bit = 0;
        clock_pointer_ = clock_pointer_->next;
        
        if (clock_pointer_ == start_pointer) {
            if (clock_pointer_ == clock_head_) {
                clock_pointer_ = clock_pointer_->next;
            }
            if (clock_pointer_ != clock_head_) {
                Node<K, V>* victim = clock_pointer_;
                clock_pointer_ = clock_pointer_->next;
                remove(victim);
            }
            return;
        }
    }
}   

template <typename K, typename V, typename Hash>
void ClockCache<K, V, Hash>::remove(Node<K, V>* node) {
    if (node == clock_pointer_) {
        clock_pointer_ = node->next;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;
    keyToNode_.erase(node->key);
    delete node;
    size_--;
}
#endif