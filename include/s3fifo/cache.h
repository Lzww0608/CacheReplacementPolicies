/*
@Author: Lzww
@LastEditTime: 2025-8-20 18:49:00
@Description: S3FIFO cache implementation
@Language: C++17
*/

#ifndef S3FIFO_CACHE_H
#define S3FIFO_CACHE_H

#include "../utils/node.h"
#include "../utils/intrusive_list.h"

#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>
#include <optional>
#include <memory>
#include <cassert>

using namespace CRP;


namespace S3FIFO {

template <typename K, typename V, typename Hash = std::hash<std::string>>
class S3FIFOCache {
public:
    explicit S3FIFOCache(size_t capacity, double s_ratio = 0.1);
    ~S3FIFOCache();

    void put(const K& key, const V& value);
    std::optional<V> get(const K& key);
    void clear();
    size_t size() const;
    size_t capacity() const;
    bool empty() const;

private:
    // Handle cache hit in S queue
    void handle_s_hit(Node<K, V>* node);

    // Handle cache hit in M queue
    void handle_m_hit(Node<K, V>* node);

    // Handle cache miss but ghost queue hit
    void handle_ghost_hit(Node<K, V>* node);

    // Handle complete miss
    void handle_miss(const K& key, const V& value);

    // Evict an entry from S queue to G queue
    Node<K, V>* evict_from_s();

    // Evict an entry from M queue (using second chance algorithm)
    Node<K, V>* evict_from_m();

    // Insert a new entry into M queue
    void insert_into_m(Node<K, V>* node);

    // Insert a new entry into S queue
    void insert_into_s(Node<K, V>* node);

    // Insert a new entry into G queue
    void insert_into_g(Node<K, V>* node);

private:
    CRP::IntrusiveList<K, V> s_queue_;  // Small queue for new entries
    CRP::IntrusiveList<K, V> m_queue_;  // Main queue for promoted entries
    CRP::IntrusiveList<K, V> g_queue_;  // Ghost queue for evicted entries

    std::unordered_map<K, Node<K, V>*> s_map_;  // Hash map for S queue
    std::unordered_map<K, Node<K, V>*> m_map_;  // Hash map for M queue
    std::unordered_map<K, Node<K, V>*> g_map_;  // Hash map for G queue

    size_t s_capacity_;  // Capacity of S queue
    size_t m_capacity_;  // Capacity of M queue
    size_t g_capacity_;  // Capacity of G queue

    mutable std::mutex mtx_;  // Mutex for thread safety
};


// Template implementations must be in header for proper instantiation
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

    // Don't reset capacities - they should remain as configured
}

template <typename K, typename V, typename Hash>
void S3FIFOCache<K, V, Hash>::put(const K& key, const V& value) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (m_map_.find(key) != m_map_.end()) {
        // hit main queue - update value
        m_map_[key]->value = value;
        handle_m_hit(m_map_[key]);
    } else if (s_map_.find(key) != s_map_.end()) {
        // hit small queue - update value
        s_map_[key]->value = value;
        handle_s_hit(s_map_[key]);
    } else if (g_map_.find(key) != g_map_.end()) {
        // hit ghost queue - update value
        g_map_[key]->value = value;
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
        // S queue is full, need to make space
        while (s_queue_.size() >= s_capacity_) {
            auto victim = evict_from_s();
            if (victim) {
                // Move victim to ghost queue
                insert_into_g(victim);
            } else {
                // All items in S were promoted to M queue, S is now empty
                break;
            }
        }
        insert_into_s(new_node);
    } 
}

template <typename K, typename V, typename Hash>
Node<K, V>* S3FIFOCache<K, V, Hash>::evict_from_s() {
    // For S3FIFO: promote accessed items to M queue, evict non-accessed items
    while (!s_queue_.empty()) {
        auto node = s_queue_.pop_back();
        if (node == nullptr) {
            return nullptr;  // S queue is empty
        }
        
        s_map_.erase(node->key);
        
        // Check if node has been accessed
        if (node->clock_bit == 0) {
            // Not accessed, return for eviction to ghost queue
            return node;
        } else {
            // Node was accessed, promote to M queue
            node->clock_bit = 0;  // Reset clock bit
            
            if (m_queue_.size() < m_capacity_) {
                insert_into_m(node);
            } else {
                auto victim = evict_from_m();
                if (victim) {
                    insert_into_g(victim);
                }
                insert_into_m(node);
            }
            // Continue looking for a victim in S queue
        }
    }
    return nullptr; // No victim found in S queue
}

template <typename K, typename V, typename Hash>
Node<K, V>* S3FIFOCache<K, V, Hash>::evict_from_m() {
    // Second chance algorithm for M queue
    while (!m_queue_.empty()) {
        auto node = m_queue_.pop_back();
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
    return nullptr; // M queue is empty
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

#endif // !S3FIFO_CACHE_H