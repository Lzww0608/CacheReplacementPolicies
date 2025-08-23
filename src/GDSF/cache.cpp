/*
@Author: Lzww  
@LastEditTime: 2025-8-23 18:19:22
@Description: GDSF Cache
@Language: C++17
*/

#include "../../include/GDSF/cache.h"

using namespace CRP;

namespace GDSF {

template <typename K, typename V, typename Hash>
GDSFCache<K, V, Hash>::GDSFCache(size_t capacity, double l_value)
    : capacity_(capacity), size_(0), l_value_(l_value) {}

template <typename K, typename V, typename Hash>
void GDSFCache<K, V, Hash>::put(const K& key, const V& value, size_t size) {

}



template <typename K, typename V, typename Hash>
double GDSFCache<K, V, Hash>::calculate_priority(size_t frequency) const {
    return l_value_ + frequency_function(frequency);
}

template <typename K, typename V, typename Hash>
double GDSFCache<K, V, Hash>::frequency_function(size_t count) const {
    return std::log(1 + count);
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