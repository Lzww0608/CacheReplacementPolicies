
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
    auto victim = GetVictim();
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
    } else { // 竞争失败，从 probation_ 中淘汰
        probation_.pop_back();
        probation_.push_front(victim);
        victim->is_in_protected = false;
    }

    return;
}

/* 从窗口缓存升至 probation */
template <typename K, typename V, typename Hash>
void CRP::w_tinylfu::SLRU<K, V, Hash>::OnAdd(Node* node) {
    /* probation_ 有空余，直接升至 probation_ */
    if (probation_.size() < probation_capacity_) {
        probation_.push_front(node);
        return;
    }

    /* probation_ 已满，竞争 */
}


} // namespace w_tinylfu
} // namespace CRP
