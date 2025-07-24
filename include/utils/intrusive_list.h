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

} // namespace CRP

#endif
