/*
@Author: Lzww  
@LastEditTime: 2025-8-23 21:22:25
@Description: GDSF Cache
@Language: C++17
*/

#include "../../include/GDSF/cache.h"

// using namespace CRP;

namespace GDSF {

template <typename K, typename V, typename Hash>
GDSFCache<K, V, Hash>::GDSFCache(size_t capacity, double l_value)
    : capacity_(capacity), current_size_(0), l_value_(l_value) {}

template <typename K, typename V, typename Hash>
bool GDSFCache<K, V, Hash>::put(const K& key, const V& value, size_t size) {
    std::lock_guard<std::shared_mutex> lock(mtx_);

    if (size > capacity_) {
        return false;
    }
    
    size_t existing_frequency = 1;
    auto existing_it = lookup_table_.find(key);
    if (existing_it != lookup_table_.end()) {
        existing_frequency = existing_it->second->frequency;
        erase(key);
    }

    evict(size);

    auto node = Node(key, value, size, calculate_priority(existing_frequency, size));
    node.frequency = existing_frequency;

    auto [it, success] = priority_queue_.insert(node);
    if (success) {
        lookup_table_.emplace(key, it);
        current_size_ += size;
        return true;
    } else {
        return false;
    }
}

template <typename K, typename V, typename Hash>
std::optional<V> GDSFCache<K, V, Hash>::get(const K& key) {
    std::lock_guard<std::shared_mutex> lock(mtx_);

    auto it = lookup_table_.find(key);
    if (it == lookup_table_.end()) {
        return std::nullopt;
    }

    auto node_it = it->second;
    const auto& node = *node_it;
    V value = node.value;

    // update frequency
    priority_queue_.erase(node_it);
    
    // create updated node, increase frequency and recalculate priority
    auto updated_node = node;
    updated_node.frequency++;
    updated_node.priority = calculate_priority(updated_node.frequency, updated_node.size);
    
    // reinsert
    auto [new_it, success] = priority_queue_.insert(updated_node);
    if (success) {
        lookup_table_[key] = new_it;
    }

    return value;
}

template <typename K, typename V, typename Hash>
bool GDSFCache<K, V, Hash>::contains(const K& key) const {
    std::shared_lock<std::shared_mutex> lock(const_cast<std::shared_mutex&>(mtx_));
    return lookup_table_.find(key) != lookup_table_.end();
}

template <typename K, typename V, typename Hash>
size_t GDSFCache<K, V, Hash>::size() const {
    std::shared_lock<std::shared_mutex> lock(const_cast<std::shared_mutex&>(mtx_));
    return current_size_;
}

template <typename K, typename V, typename Hash>
size_t GDSFCache<K, V, Hash>::capacity() const {
    return capacity_;
}

template <typename K, typename V, typename Hash>
double GDSFCache<K, V, Hash>::count() const {
    std::shared_lock<std::shared_mutex> lock(const_cast<std::shared_mutex&>(mtx_));
    return priority_queue_.size();
}

template <typename K, typename V, typename Hash>
void GDSFCache<K, V, Hash>::erase(const K& key) {
    auto it = lookup_table_.find(key);
    if (it == lookup_table_.end()) {
        return;
    }

    const auto& node_iterator = it->second;
    current_size_ -= node_iterator->size;
    priority_queue_.erase(node_iterator);
    lookup_table_.erase(it);
}

template <typename K, typename V, typename Hash>
double GDSFCache<K, V, Hash>::calculate_priority(size_t frequency, size_t size) const {
    return l_value_ + static_cast<double>(frequency) / size;
}



template <typename K, typename V, typename Hash>
void GDSFCache<K, V, Hash>::evict(size_t needed_space) {
    while (current_size_ + needed_space > capacity_ && !priority_queue_.empty()) {
        auto it_to_evict = priority_queue_.begin();
        const auto& node_to_evict = *it_to_evict;

        // core
        l_value_ = node_to_evict.priority;

        // update
        current_size_ -= node_to_evict.size;
        priority_queue_.erase(it_to_evict);
        lookup_table_.erase(node_to_evict.key);
    }
}






} // namespace GDSF