#ifndef INTERNAL_INTRUSIVE_LIST_H
#define INTERNAL_INTRUSIVE_LIST_H

#include "node.h"
#include <cassert>

namespace CRP {
// 侵入式链表模板类，管理 CacheNode 的链接
template <typename K, typename V>
class IntrusiveList {
public:
    using Node = ::Node<K, V>;

    IntrusiveList();
    ~IntrusiveList();

    void clear();
    void push_back(Node* node);
    void push_front(Node* node);
    void remove(Node* node);
    Node* pop_back();
private:
    // 头结点  
    Node* head_;
    // 辅助函数
    void unlink(Node* node) {
        if (node->prev && node->next) {
            Node* prev = node->prev;
            Node* next = node->next;
            prev->next = next;
            next->prev = prev;
        }
        node->prev = nullptr;
        node->next = nullptr;
    }
};

template <typename K, typename V>
IntrusiveList<K, V>::IntrusiveList() {
    head_ = new Node();
    head_->next = head_;
    head_->prev = head_;
}

template <typename K, typename V>
IntrusiveList<K, V>::~IntrusiveList() {
    clear();
}

template <typename K, typename V>
void IntrusiveList<K, V>::clear() {
    Node* node = head_;
    while (node) {
        Node* next = node->next;
        delete node;
        node = next;
    }
}

template <typename K, typename V>
void IntrusiveList<K, V>::push_back(Node* node) {
    unlink(node);
    node->next = head_;
    node->prev = head_->prev;
    node->next->prev = node;
    node->prev->next = node;
}

template <typename K, typename V>
void IntrusiveList<K, V>::push_front(Node* node) {
    unlink(node);
    node->next = head_->next;
    node->prev = head_;
    node->next->prev = node;
    node->prev->next = node;
}

template <typename K, typename V>
void IntrusiveList<K, V>::remove(Node* node) {
    unlink(node);
}

template <typename K, typename V>
auto IntrusiveList<K, V>::pop_back() -> Node* {
    Node* node = head_->prev;
    if (node == head) return nullptr;
    remove(node);
    return node;
}

} // namespace CRP

#endif
