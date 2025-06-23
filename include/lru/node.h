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
    
    // 默认构造函数（用于哨兵节点）
    Node() : key(K{}), value(V{}), prev(nullptr), next(nullptr), expire_time(std::chrono::steady_clock::now() + std::chrono::seconds(DEFAULT_EXPIRE_TIME)) {}
    
    // 带值构造函数
    Node(const K& k, const V& v) : key(k), value(v), prev(nullptr), next(nullptr), expire_time(std::chrono::steady_clock::now() + std::chrono::seconds(DEFAULT_EXPIRE_TIME)) {}
    
    // 带值和过期时间构造函数
    Node(const K& k, const V& v, std::chrono::steady_clock::time_point exp_time) : key(k), value(v), prev(nullptr), next(nullptr), expire_time(exp_time) {}

    ~Node() {
        prev = nullptr;
        next = nullptr;
    }
};

template <typename K, typename V>
inline void remove(Node<K, V> *node) {
    if (node == nullptr) {
        throw std::invalid_argument("Node is nullptr");
    }
    node->prev->next = node->next;
    node->next->prev = node->prev;
}


#endif