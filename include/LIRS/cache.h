/*
@Author: Lzww
@LastEditTime: 2025-8-18 21:15:36
@Description: LIRS缓存实现
@Language: C++17
*/

#ifndef LIRS_CACHE_H
#define LIRS_CACHE_H

#include "lirs_node.h"

#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>
#include <optional>
#include <memory>

constexpr size_t DEFAULT_CAPACITY = 1024 * 1024 * 100;

constexpr double LIR_RATIO = 0.99;

namespace LIRS {

template <typename K, typename V, typename Hash = std::hash<std::string>>
class LIRSCache {
public:
    LIRSCache(size_t capacity = DEFAULT_CAPACITY);
    ~LIRSCache();
    
    // core methods
    void put(const K& key, const V& value);
    std::optional<V> get(const K& key);
    bool contains(const K& key) const;
    size_t size() const;
    size_t capacity() const;
    bool empty() const;
    void clear();

private:
    std::unordered_map<K, LIRSNode<K, V>*, Hash> key_to_node_;
    LIRSNode<K, V> *lir_head_;
    LIRSNode<K, V> *hir_head_;

    size_t capacity_;
    size_t size_;
    size_t lir_size_;
    size_t hir_size_;

    mutable std::mutex mtx_;

    void push_to_s(LIRSNode<K, V> *node);
    void push_to_q(LIRSNode<K, V> *node);

    void evict_victim();
    void prune_s_stack();
    void demote_lir_to_hir();
    bool should_promote_to_lir(LIRSNode<K, V> *node, size_t hir_original_distance);
    size_t get_distance_from_head(LIRSNode<K, V> *node);

    bool contains(const K& key) const;
};

} // namespace LIRS

#endif