/*
@Author: Lzww  
@LastEditTime: 2025-8-23 18:19:22
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

using namespace CRP;

namespace GDSF {

template <typename K, typename V, typename Hash = std::hash<std::string>>
class GDSFCache {
public:
    explicit GDSFCache(size_t capacity, double l_value = 0.5);

    void put(const K& key, const V& value, size_t size);

    std::optional<V> get(const K& key) const;

    bool erase(const K& key);

    bool contains(const K& key) const;

    [[nodiscard]] size_t size() const;

    [[nodiscard]] size_t capacity() const;

    [[nodiscard]] double count() const;
private:
    void evict(size_t needed_space);

    double frequency_function(size_t count) const;

    double calculate_priority(size_t frequency) const;
private:
    struct Node {
        K key;
        V value;
        size_t size;
        size_t frequency;
        double priority;
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