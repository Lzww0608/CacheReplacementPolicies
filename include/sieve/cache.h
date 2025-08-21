/*
@Author: Lzww
@LastEditTime: 2025-8-21 22:39:40
@Description: Sieve Cache
@Language: C++17
*/

#ifndef SIEVE_CACHE_H
#define SIEVE_CACHE_H

#include "node.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <optional>
#include <string>

using namespace CRP;

namespace Sieve {

template <typename K, typename V>
class Cache {
public:
    Cache(uint32_t capacity);
    ~Cache();

    // Get value by key, returns std::nullopt if key not found
    std::optional<V> get(const K& key);
    
    // Insert or update key-value pair in cache
    void put(const K& key, const V& value);
    
    // Delete entry by key, returns true if deleted, false if key not found
    bool del(const K& key);
    
    // Get current number of entries in cache
    uint32_t size() const;
    
    // Check if cache is empty
    bool empty() const;
    
    // Get string representation of cache for debugging
    std::string toString() const;


private:


private:
    mutable std::mutex mtx_;
    std::unordered_map<K, Node<K, V>*> map_;
    Node<K, V>* head_;
    Node<K, V>* tail_;
    Node<K, V>* hand_;

    uint32_t capacity_;
};

} // namespace Sieve
#endif // SIEVE_CACHE_H