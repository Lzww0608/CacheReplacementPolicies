/*
@Author: Lzww
@LastEditTime: 2025-6-28 22:36:48
@Description: 双向链表节点定义
@Language: C++17
*/

#ifndef NODE_H
#define NODE_H

#include <chrono>

// 侵入式链表节点基类（提供 prev/next 指针）
template <typename T>
struct IntrusiveListNode {
    T* prev;  // 前向指针
    T* next;  // 后向指针

    // 构造函数：初始化指针为 nullptr
    IntrusiveListNode() : prev(nullptr), next(nullptr) {}
};

template <typename K, typename V>
struct Node: public IntrusiveListNode<Node<K, V>> {
    K key;
    V value;
    std::chrono::steady_clock::time_point expire_time;
    uint64_t frequency = 1; // LFU访问频率
    uint8_t clock_bit = 0; // Clock算法位
    bool is_in_protected;  // 标记是否在 protected 区

    // 构造函数
    Node() : prev(nullptr), next(nullptr) {}
    
    Node(const K& k, const V& v, int expire_ms = 3600000) 
        : key(k), value(v), prev(nullptr), next(nullptr), is_in_protected(false) {
        if (expire_ms > 0) {
            expire_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(expire_ms);
        } else {
            expire_time = std::chrono::steady_clock::time_point::max();
        }
    }
};

#endif
