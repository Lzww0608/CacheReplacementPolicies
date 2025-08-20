/*
@Author: Lzww
@LastEditTime: 2025-8-20 20:27:05
@Description: S3FIFO缓存实现
@Language: C++17
*/

#include "../../include/s3fifo/cache.h"
#include "../../include/utils/intrusive_list.h"

#include <cassert>

using namespace CRP;

namespace S3FIFO {

template <typename K, typename V, typename Hash>
S3FIFOCache<K, V, Hash>::S3FIFOCache(size_t capacity, double s_ratio)
    : s_capacity_(static_cast<size_t>(capacity * s_ratio)),
      m_capacity_(capacity - s_capacity_),
      g_capacity_(capacity),
      s_map_(),
      m_map_(),
      g_map_() {
    assert(s_capacity_ + m_capacity_ == capacity);
}


template <typename K, typename V, typename Hash>
S3FIFOCache<K, V, Hash>::~S3FIFOCache() {
    clear();
}

template <typename K, typename V, typename Hash>
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

template <typename K, typename V, typename Hash>
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


template <typename K, typename V, typename Hash>
std::optional<V> S3FIFOCache<K, V, Hash>::get(const K& key) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (m_map_.find(key) != m_map_.end()) {
        // hit main queue
        handle_m_hit(m_map_[key]);
        return m_map_[key]->value;
    } else if (s_map_.find(key) != s_map_.end()) {
        // hit small queue
        handle_s_hit(s_map_[key]);
        return s_map_[key]->value;
    } else if (g_map_.find(key) != g_map_.end()) {
        // hit ghost queue - promote to M queue
        auto old_value = g_map_[key]->value;
        handle_ghost_hit(g_map_[key]);
        return old_value;
    } 

    return std::nullopt;
}

template <typename K, typename V, typename Hash>
size_t S3FIFOCache<K, V, Hash>::size() const {
    return s_queue_.size() + m_queue_.size(); // Ghost queue doesn't count as cache size
}

template <typename K, typename V, typename Hash>
size_t S3FIFOCache<K, V, Hash>::capacity() const {
    return s_capacity_ + m_capacity_; // Only actual cache capacity, not ghost
}

template <typename K, typename V, typename Hash>
bool S3FIFOCache<K, V, Hash>::empty() const {
    return s_queue_.empty() && m_queue_.empty(); // Ghost queue doesn't matter for empty check
}

template <typename K, typename V, typename Hash>
void S3FIFOCache<K, V, Hash>::handle_s_hit(Node<K, V>* node) {
    node->clock_bit = 1;
}

template <typename K, typename V, typename Hash>
void S3FIFOCache<K, V, Hash>::handle_m_hit(Node<K, V>* node) {
    node->clock_bit = 1;
}

template <typename K, typename V, typename Hash>
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


template <typename K, typename V, typename Hash>
void S3FIFOCache<K, V, Hash>::handle_miss(const K& key, const V& value) {
    auto new_node = new Node<K, V>(key, value);
    
    if (s_queue_.size() < s_capacity_) {
        // S queue has space, insert directly
        insert_into_s(new_node);
    } else {
        // S queue is full, evict from S and insert into G
        auto victim = evict_from_s();
        if (victim) {
            insert_into_g(victim);
        }
        insert_into_s(new_node);
    } 
}

template <typename K, typename V, typename Hash>
Node<K, V>* S3FIFOCache<K, V, Hash>::evict_from_s() {
    // Simple FIFO for S queue (no second chance)
    Node<K, V>* node = nullptr;
    
    while (true) {
        node = s_queue_.pop_back();
        if (node == nullptr) {
            return nullptr;  // S queue is empty
        }
        
        // Check if node has been accessed
        if (node->clock_bit == 0) {
            // Not accessed, evict this node
            s_map_.erase(node->key);
            return node;
        } else {
            // Node was accessed, promote to M queue
            node->clock_bit = 0;  // Reset clock bit
            s_map_.erase(node->key);
            
            if (m_queue_.size() < m_capacity_) {
                insert_into_m(node);
            } else {
                auto victim = evict_from_m();
                if (victim) {
                    insert_into_g(victim);
                }
                insert_into_m(node);
            }
            // Continue looking for victim in S queue
        }
    }
}

template <typename K, typename V, typename Hash>
Node<K, V>* S3FIFOCache<K, V, Hash>::evict_from_m() {
    // Second chance algorithm for M queue
    Node<K, V>* node = nullptr;
    
    while (true) {
        node = m_queue_.pop_back();
        if (node == nullptr) {
            return nullptr;  // M queue is empty
        }
        
        // Check clock bit
        if (node->clock_bit == 0) {
            // Clock bit is 0, evict this node
            m_map_.erase(node->key);
            return node;
        } else {
            // Clock bit is 1, give second chance
            node->clock_bit = 0;
            m_queue_.push_front(node);
        }
    }
}

template <typename K, typename V, typename Hash>
void S3FIFOCache<K, V, Hash>::insert_into_m(Node<K, V>* node) {
    m_queue_.push_front(node);
    m_map_[node->key] = node;
}


template <typename K, typename V, typename Hash>
void S3FIFOCache<K, V, Hash>::insert_into_s(Node<K, V>* node) {
    s_queue_.push_front(node);
    s_map_[node->key] = node;
}

template <typename K, typename V, typename Hash>
void S3FIFOCache<K, V, Hash>::insert_into_g(Node<K, V>* node) {
    // Check if ghost queue is full
    if (g_queue_.size() >= g_capacity_) {
        // Remove oldest ghost entry
        auto oldest = g_queue_.pop_back();
        if (oldest) {
            g_map_.erase(oldest->key);
            delete oldest;
        }
    }
    
    g_queue_.push_front(node);
    g_map_[node->key] = node;
}

} // namespace S3FIFO
