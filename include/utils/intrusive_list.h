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
    size_t size() const;
    bool empty() const;
private:
    // 头结点  
    Node* head_;
    // 节点计数
    size_t size_;
    // 辅助函数 - 只是安全地清理节点的链接，不影响size_
    void unlink(Node* node) {
        if (node->prev && node->next && node->prev != node && node->next != node) {
            Node* prev = node->prev;
            Node* next = node->next;
            prev->next = next;
            next->prev = prev;
            size_--; // 从链表中移除时减少计数
        }
        node->prev = nullptr;
        node->next = nullptr;
    }
};

template <typename K, typename V>
IntrusiveList<K, V>::IntrusiveList() : size_(0) {
    head_ = new Node();
    head_->next = head_;
    head_->prev = head_;
}

template <typename K, typename V>
IntrusiveList<K, V>::~IntrusiveList() {
    clear();
    delete head_;
    head_ = nullptr;
}

template <typename K, typename V>
void IntrusiveList<K, V>::clear() {
    if (!head_) return;
    
    Node* node = head_->next;
    while (node && node != head_) {
        Node* next = node->next;
        delete node;
        node = next;
    }
    
    head_->next = head_;
    head_->prev = head_;
    size_ = 0;
}

template <typename K, typename V>
void IntrusiveList<K, V>::push_back(Node* node) {
    unlink(node);
    node->next = head_;
    node->prev = head_->prev;
    node->next->prev = node;
    node->prev->next = node;
    size_++;
}

template <typename K, typename V>
void IntrusiveList<K, V>::push_front(Node* node) {
    unlink(node);
    node->next = head_->next;
    node->prev = head_;
    node->next->prev = node;
    node->prev->next = node;
    size_++;
}

template <typename K, typename V>
void IntrusiveList<K, V>::remove(Node* node) {
    unlink(node);
}

template <typename K, typename V>
auto IntrusiveList<K, V>::pop_back() -> Node* {
    Node* node = head_->prev;
    if (node == head_) return nullptr;
    remove(node);
    return node;
}

template <typename K, typename V>
size_t IntrusiveList<K, V>::size() const {
    return size_;
}

template <typename K, typename V>
bool IntrusiveList<K, V>::empty() const {
    return size_ == 0;
}

} // namespace CRP

#endif
