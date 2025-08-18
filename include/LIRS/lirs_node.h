/*
@Author: Lzww
@LastEditTime: 2025-8-18 21:26:11
@Description: LIRS侵入式链表节点定义
@Language: C++17
*/

#ifndef LIRS_NODE_H
#define LIRS_NODE_H

#include <cstdint>

constexpr uint32_t MAX_IRR_VALUE = UINT32_MAX;

namespace LIRS {

template <typename K, typename V>
struct LIRSNode {
    K key;
    V value;
    bool is_LIRS;
    bool is_resident;
    uint32_t irr_value;

    LIRSNode *prev_s, *next_s;
    LIRSNode *prev_q, *next_q;


    LIRSNode(const K& k, const V& v);
    ~LIRSNode();

    void remove_from_s();
    void remove_from_q();
};

template <typename K, typename V>
LIRSNode<K, V>::LIRSNode(const K& k, const V& v)
    : key(std::move(k)), value(std::move(v)), is_LIRS(false), is_resident(true), irr_value(MAX_IRR_VALUE),
      prev_s(nullptr), next_s(nullptr), prev_q(nullptr), next_q(nullptr) {}

template <typename K, typename V>
LIRSNode<K, V>::~LIRSNode() {}

template <typename K, typename V>
void LIRSNode<K, V>::remove_from_s() {
    if (prev_s) {
        prev_s->next_s = next_s;
    }
    if (next_s) {
        next_s->prev_s = prev_s;
    }
    prev_s = nullptr;
    next_s = nullptr;
}

template <typename K, typename V>
void LIRSNode<K, V>::remove_from_q() {
    if (prev_q) {
        prev_q->next_q = next_q;
    }
    if (next_q) {
        next_q->prev_q = prev_q;
    }
    prev_q = nullptr;
    next_q = nullptr;
}

} // namespace LIRS
#endif