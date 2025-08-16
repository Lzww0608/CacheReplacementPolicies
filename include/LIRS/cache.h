/*
@Author: Lzww
@LastEditTime: 2025-8-16 22:17:24
@Description: LIRS缓存实现
@Language: C++17
*/

#ifndef LIRS_CACHE_H
#define LIRS_CACHE_H

#include "../utils/intrusive_list.h"

#include <unordered_map>
#include <string>
#include <functional>


namespace LIRS {

template <typename K, typename V, typename Hash = std::hash<std::string>>
class LIRSCache {


private:
    std::unordered_map<K, Node<K, V>*, Hash> key_to_node_;
    IntrusiveList<K, V> lir_list_;
    IntrusiveList<K, V> hir_list_;
    IntrusiveList<K, V> non_resident_list_;

    size_t capacity_;
    size_t size_;
    mutable std::shared_mutex mtx_;

    void remove(Node<K, V>* node);
}

} // namespace LIRS

#endif