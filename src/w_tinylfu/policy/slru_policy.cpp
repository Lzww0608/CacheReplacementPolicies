
#include "../../../include/w_tinylfu/policy/eviction_policy.h"

namespace CRP {
namespace w_tinylfu {

template <typename K, typename V, typename Hash>
CRP::w_tinylfu::SLRU<K, V, Hash>::SLRU(uint64_t probation_capacity, uint64_t protection_capacity)
    : probation_capacity_(probation_capacity), protection_capacity_(protection_capacity) {
}

// 从probation 升至 protected
template <typename K, typename V, typename Hash>
void CRP::w_tinylfu::SLRU<K, V, Hash>::OnAccess(Node* node) {      
    if (node->is_in_protected) {
        return;
    }

    /* protected_ 有空余，直接升至 protected—_ */
    auto victim = protected_.pop_back();
    if (victim == nullptr) {
        probation_.remove(node);
        protection_.push_front(node);
        node->is_in_protected = true;
        return;
    }

    /* protected_ 不为空，竞争 */
    if (Compete(node, victim) == 0) {
        probation_.remove(node);
        protection_.push_front(node);
        node->is_in_protected = true;
        // victim 降级至 probation_
        probation_.push_front(victim);
        victim->is_in_protected = false;
    } else { // 竞争失败，victim 回原位置
        protected_.push_back(victim);
    }

    return;
}

/* 从窗口缓存升至 probation */
template <typename K, typename V, typename Hash>
void CRP::w_tinylfu::SLRU<K, V, Hash>::OnAdd(Node* node) {
    /* probation_ 有空余，直接升至 probation_ */
    if (probation_.size() < probation_capacity_) {
        probation_.push_front(node);
        key_to_node_[node->key] = node;
        return;
    }

    /* probation_ 已满，竞争 */
    auto victim = probation.pop_back();
    if (Compete(node, victim) == 0) {
        probation_.push_front(node);
        key_to_node_[node->key] = node;
        delete victim;
    }

    return;
}

/* 移除主缓存的某一个节点 */
template <typename K, typename V, typename Hash>
uint32_t CRP::w_tinylfu::SLRU<K, V, Hash>::EraseNode(Node* node) {
    if (!key_to_node_.contains(node->key)) {
        return -1; // 主缓存中没有该节点
    }
    
    key_to_node_.erase(node->key);
    if (node->is_in_protected) {
        protected_.remove(node);
    } else {
        protection_.remove(node);
    }

    return 0;
}

} // namespace w_tinylfu
} // namespace CRP
