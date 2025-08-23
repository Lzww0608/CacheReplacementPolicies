/*
@Author: Lzww  
@LastEditTime: 2025-8-23 21:22:36
@Description: GDSF Cache
@Language: C++17
*/

#ifndef GDSF_CACHE_H
#define GDSF_CACHE_H

#include <optional>
#include <functional>
#include <string>
#include <set>
#include <unordered_map>
#include <shared_mutex>
#include <memory>
#include <cmath>
#include <cstdint>

// using namespace CRP;

namespace GDSF {

template <typename K, typename V, typename Hash = std::hash<std::string>>
class GDSFCache {
public:
    explicit GDSFCache(size_t capacity, double l_value = 0.5);

    bool put(const K& key, const V& value, size_t size);

    [[nodiscard]] std::optional<V> get(const K& key);

    [[nodiscard]] bool contains(const K& key) const;

    [[nodiscard]] size_t size() const;

    [[nodiscard]] size_t capacity() const;

    [[nodiscard]] double count() const;
private:
    void erase(const K& key);

    void evict(size_t needed_space);



    double calculate_priority(size_t frequency, size_t size) const;
private:
    struct Node {
        K key;
        V value;
        size_t size;
        size_t frequency;
        double priority;

        Node(const K& key, const V& value, size_t size, double priority)
            : key(key), value(value), size(size), frequency(1), priority(priority) {}
    };

    struct NodeComparator {
        // allow comparison of different types
        using is_transparent = void;

        bool operator()(const Node& a, const Node& b) const {
            if (a.priority != b.priority) {
                return a.priority < b.priority;
            }

            return a.key < b.key;
        }
    };

    using NodeSet = std::set<Node, NodeComparator>;
    using NodeMap = std::unordered_map<K, typename NodeSet::iterator, Hash>;

    size_t capacity_;
    size_t current_size_;
    double l_value_;

    NodeSet priority_queue_;
    NodeMap lookup_table_;

    std::shared_mutex mtx_;
};


} // namespace GDSF

#endif