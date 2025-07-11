/*
@Author: Lzww
@LastEditTime: 2025-6-28 22:36:48
@Description: 双向链表节点定义
@Language: C++17
*/

#ifndef NODE_H
#define NODE_H

#include <chrono>

template <typename K, typename V>
struct Node {
    K key;
    V value;
    Node* prev;
    Node* next;
    std::chrono::steady_clock::time_point expire_time;
    uint64_t frequency = 1; // LFU访问频率
    uint8_t clock_bit = 0; // Clock算法位

    // 构造函数
    Node() : prev(nullptr), next(nullptr) {}
    
    Node(const K& k, const V& v, int expire_ms = 3600000) 
        : key(k), value(v), prev(nullptr), next(nullptr) {
        if (expire_ms > 0) {
            expire_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(expire_ms);
        } else {
            expire_time = std::chrono::steady_clock::time_point::max();
        }
    }
};

#endif