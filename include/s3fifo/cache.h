/*
@Author: Lzww
@LastEditTime: 2025-8-19 21:59:36
@Description: S3FIFO cache implementation
@Language: C++17
*/

#ifndef S3FIFO_CACHE_H
#define S3FIFO_CACHE_H

#include "../utils/node.h"

#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>
#include <optional>
#include <memory>


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
    IntrusiveList<K, V> s_queue_;  // Small queue for new entries
    IntrusiveList<K, V> m_queue_;  // Main queue for promoted entries
    IntrusiveList<K, V> g_queue_;  // Ghost queue for evicted entries

    std::unordered_map<K, Node<K, V>*> s_map_;  // Hash map for S queue
    std::unordered_map<K, Node<K, V>*> m_map_;  // Hash map for M queue
    std::unordered_map<K, Node<K, V>*> g_map_;  // Hash map for G queue

    size_t s_capacity_;  // Capacity of S queue
    size_t m_capacity_;  // Capacity of M queue
    size_t g_capacity_;  // Capacity of G queue

    mutable std::mutex mtx_;  // Mutex for thread safety
};


} // namespace S3FIFO

#endif // !S3FIFO_CACHE_H