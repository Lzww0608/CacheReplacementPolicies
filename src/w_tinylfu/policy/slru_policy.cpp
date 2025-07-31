
#include "../../../include/w_tinylfu/policy/eviction_policy.h"
#include "../../../include/utils/rand.h"

namespace CRP {
namespace w_tinylfu {

template <typename K, typename V, typename Hash>
CRP::w_tinylfu::SLRU<K, V, Hash>::SLRU(uint64_t probation_capacity, uint64_t protection_capacity)
    : probation_capacity_(probation_capacity), protection_capacity_(protection_capacity) {
}

// 从probation 升至 protected
template <typename K, typename V, typename Hash>
void CRP::w_tinylfu::SLRU<K, V, Hash>::OnAccess(Node* node) {
    std::scoped_lock<std::shared_mutex, std::shared_mutex> scoped_lock_(probation_mutex_, protected_mutex_);
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
    std::unique_lock<std::shared_lock> protected_write_lock(protected_mutex_);
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
    std::scoped_lock<std::shared_mutex, std::shared_mutex> scoped_lock_(probation_mutex_, protected_mutex_);
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

/* 公平竞争：候选数据 与 受害者*/
template <typename K, typename V, typename Hash>
uint32_t CRP::w_tinylfu::SLRU<K, V, Hash>::Compete(Node* candidate, Node* victim) {
    std::scoped_lock<std::shared_mutex, std::shared_mutex> scoped_lock_(probation_mutex_, protected_mutex_);
    
    // 主缓存中没有victim
    if (!key_to_node_.contains(victim->key)) {
        return -1;
    }

    // candidate频率 > Victim 频率
    if (candidate->frequency > victim->frequency) {
        return 0;
    }
    // candidate频率 <= Victim 频率
    // 仅当候选频率≥5 时，才可能通过随机淘汰机制（概率 50%）获得晋升资格
    if (candidate->frequency < 5) {
        return -1;
    }

    return getRandomBool();
}

/* 获取probation和protection大小 */
template <typename K, typename V, typename Hash>
uint64_t CRP::w_tinylfu::SLRU<K, V, Hash>::GetProbationSize() const {
     std::shared_lock<std::shared_mutex> probation_read_lock_(probation_mutex_);
    return probation_.size();
}

template <typename K, typename V, typename Hash>
uint64_t CRP::w_tinylfu::SLRU<K, V, Hash>::GetProtectionSize() const {
    std::shared_lock<std::shared_mutex> protected_read_lock_(protected_mutex_);
    return protected_.size();
}

template <typename K, typename V, typename Hash>
uint64_t CRP::w_tinylfu::SLRU<K, V, Hash>::GetSize() const {
    std::shared_lock<std::shared_mutex> probation_read_lock_(probation_mutex_);
    std::shared_lock<std::shared_mutex> protected_read_lock_(protected_mutex_);
    return probation_.size() + protected_.size();
}

template <typename K, typename V, typename Hash>
uint64_t CRP::w_tinylfu::SLRU<K, V, Hash>::GetCapacity() const {
    return probation_capacity_ + protection_capacity_;
}

/* 判断节点是否存在于主缓存 */
template <typename K, typename V, typename Hash>
bool Contains(const K& key) const {
    std::shared_lock<std::shared_mutex> probation_read_lock_(probation_mutex_);
    std::shared_lock<std::shared_mutex> protected_read_lock_(protected_mutex_);
    return probation_.contains(key) || protected_.contains(key);
}

} // namespace w_tinylfu
} // namespace CRP
