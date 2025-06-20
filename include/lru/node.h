/*
@Author: Lzww
@LastEditTime: 2025-6-20 20:43:23
@Description: LRU缓存节点实现
@Language: C++17
*/

#ifndef NODE_H
#define NODE_H

#include <utility>
#include <stdexcept>
#include <chrono>

constexpr int DEFAULT_EXPIRE_TIME = 100;

template <typename K, typename V>
struct Node {
    K key;
    V value;
    Node *prev, *next;
    std::chrono::steady_clock::time_point expire_time;
    Node(K k, V v) : key(k), value(v), prev(nullptr), next(nullptr), expire_time(std::chrono::steady_clock::now() + std::chrono::seconds(DEFAULT_EXPIRE_TIME)) {}
    Node(K&& k, V&& v) : key(std::move(k)), value(std::move(v)), prev(nullptr), next(nullptr), expire_time(std::chrono::steady_clock::now() + std::chrono::seconds(DEFAULT_EXPIRE_TIME)) {}
    Node(K k, V v, std::chrono::steady_clock::time_point expire_time) : key(k), value(v), prev(nullptr), next(nullptr), expire_time(expire_time) {}
    Node(K&& k, V&& v, std::chrono::steady_clock::time_point expire_time) : key(std::move(k)), value(std::move(v)), prev(nullptr), next(nullptr), expire_time(expire_time) {}

    ~Node() {
        prev = nullptr;
        next = nullptr;
    }
};

inline void remove(Node<K, V> *node) {
    if (node == nullptr) {
        throw std::invalid_argument("Node is nullptr");
    }
    node->prev->next = node->next;
    node->next->prev = node->prev;
}


#endif