#ifndef CLOCK_CACHE_H
#define CLOCK_CACHE_H

#include "../node.h"

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
    size_t size() const;
    void clear();

private:
    size_t capacity_;
    size_t size_;
    std::unordered_map<K, Node<K, V>*> keyToNode_;
    mutable std::shared_mutex mutex_;
    Node<K, V>* clock_head_;

    void remove(const K& key);
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
    
}

#endif