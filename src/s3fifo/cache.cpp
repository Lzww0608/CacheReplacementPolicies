/*
@Author: Lzww
@LastEditTime: 2025-8-19 21:59:40
@Description: S3FIFO缓存实现
@Language: C++17
*/

#include "../../include/s3fifo/cache.h"

#include <cassert>

template <typename K, typename V, typename Hash = std::hash<std::string>>
S3FIFOCache<K, V, Hash>::S3FIFOCache(size_t capacity, double s_ratio)
    : s_capacity_(static_cast<size_t>(capacity * s_ratio)),
      m_capacity_(capacity - s_capacity_),
      g_capacity_(capacity),
      s_map_(),
      m_map_(),
      g_map_() {
    assert(s_capacity_ + m_capacity_ == capacity);
}


template <typename K, typename V, typename Hash = std::hash<std::string>>
S3FIFOCache<K, V, Hash>::~S3FIFOCache() {
    clear();
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::clear() {
    std::lock_guard<std::mutex> lock(mtx_);

    s_queue_.clear();
    m_queue_.clear();
    g_queue_.clear();

    s_map_.clear();
    m_map_.clear();
    g_map_.clear();

    s_capacity_ = 0;
    m_capacity_ = 0;
    g_capacity_ = 0;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::put(const K& key, const V& value) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (m_map_.find(key) != m_map_.end()) {
        // hit main queue
        handle_m_hit(m_map_[key]);
    } else if (s_map_.find(key) != s_map_.end()) {
        // hit small queue
        handle_s_hit(s_map_[key]);
    } else if (g_map_.find(key) != g_map_.end()) {
        // hit ghost queue
        handle_ghost_hit(g_map_[key]);
    } else {
        // complete miss
        handle_miss(key, value);
    }
}


template <typename K, typename V, typename Hash = std::hash<std::string>>
std::optional<V> S3FIFOCache<K, V, Hash>::get(const K& key) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (g_map_.find(key) != g_map_.end()) {
        // hit ghost queue
        handle_ghost_hit(g_map_[key]);
        return g_map_[key]->value;
    } else if (m_map_.find(key) != m_map_.end()) {
        // hit main queue
        handle_m_hit(m_map_[key]);
        return m_map_[key]->value;
    } else if (s_map_.find(key) != s_map_.end()) {
        // hit small queue
        handle_s_hit(s_map_[key]);
        return s_map_[key]->value;
    } 

    return std::nullopt;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>   
size_t S3FIFOCache<K, V, Hash>::size() const {
    return s_queue_.size() + m_queue_.size() + g_queue_.size();
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
size_t S3FIFOCache<K, V, Hash>::capacity() const {
    return s_capacity_ + m_capacity_ + g_capacity_;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
bool S3FIFOCache<K, V, Hash>::empty() const {
    return s_queue_.empty() && m_queue_.empty() && g_queue_.empty();
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::handle_s_hit(Node<K, V>* node) {
    node->clock_bit = 1;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::handle_m_hit(Node<K, V>* node) {
    node->clock_bit = 1;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::handle_ghost_hit(Node<K, V>* node) {
    node->clock_bit = 1;
    g_map_.erase(node->key);
    g_queue_.remove(node);
    
    if (m_queue_.size() < m_capacity_) {
        insert_into_m(node);
    } else {
        auto victim = evict_from_m();
        insert_into_g(victim);
        insert_into_m(node);
    }
}


template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::handle_miss(const K& key, const V& value) {
    if (s_map_.size() < s_capacity_) {
        // insert into small queue
        auto node = new Node<K, V>(key, value);
        insert_into_s(node);
    } else {
        auto node = evict_from_s();
        insert_into_g(node);
        node = new Node<K, V>(key, value);
        insert_into_s(node);
    } 
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
Node<K, V>* S3FIFOCache<K, V, Hash>::evict_from_s() {
    auto node = s_queue_.pop_back();
    s_map_.erase(node->key);
    return node;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
Node<K, V>* S3FIFOCache<K, V, Hash>::evict_from_m() {
    auto node = m_queue_.pop_back();
    m_map_.erase(node->key);
    return node;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::insert_into_m(Node<K, V>* node) {
    m_queue_.push_front(node);
    m_map_[key] = node;
}


template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::insert_into_s(Node<K, V>* node) {
    s_queue_.push_front(node);
    s_map_[node->key] = node;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void S3FIFOCache<K, V, Hash>::insert_into_g(Node<K, V>* node) {
    g_queue_.push_front(node);
    g_map_[node->key] = node;
}