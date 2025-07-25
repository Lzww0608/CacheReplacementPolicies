
#include "../../../include/w_tinylfu/policy/eviction_policy.h"

namespace CRP {
namespace w_tinylfu {

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





} // namespace w_tinylfu
} // namespace CRP
