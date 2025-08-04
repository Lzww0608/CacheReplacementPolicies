
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
void CRP::w_tinylfu::SLRU<K, V, Hash>::onAccess(Node* node) {
    if (node->is_in_protected) {
        return;
    }

    /* protection_ 有空余，直接升至 protected—_ */
    auto victim = protection_.pop_back();
    if (victim == nullptr) {
        probation_.remove(node);
        protection_.push_front(node);
        node->is_in_protected = true;
        return;
    }

    /* protection_ 不为空，竞争 */
    if (Compete(node, victim) == 0) {
        probation_.remove(node);
        protection_.push_front(node);
        node->is_in_protected = true;
        // victim 降级至 probation_
        probation_.push_front(victim);
        victim->is_in_protected = false;
    } else { // 竞争失败，victim 回原位置
        protection_.push_back(victim);
    }

    return;
}

/* 从窗口缓存升至 probation */
template <typename K, typename V, typename Hash>
void CRP::w_tinylfu::SLRU<K, V, Hash>::onAdd(Node* node) {
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
uint32_t CRP::w_tinylfu::SLRU<K, V, Hash>::eraseNode(Node* node) {
    std::scoped_lock<std::shared_mutex, std::shared_mutex> scoped_lock_(probation_mutex_, protection_mutex_);
    if (!key_to_node_.contains(node->key)) {
        return -1; // 主缓存中没有该节点
    }
    
    key_to_node_.erase(node->key);
    if (node->is_in_protected) {
        protection_.remove(node);
    } else {
        protection_.remove(node);
    }

    return 0;
}

/* 公平竞争：候选数据 与 受害者*/
template <typename K, typename V, typename Hash>
uint32_t CRP::w_tinylfu::SLRU<K, V, Hash>::compete(Node* candidate, Node* victim) {
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
uint64_t CRP::w_tinylfu::SLRU<K, V, Hash>::probationSize() const {
     std::shared_lock<std::shared_mutex> probation_read_lock_(probation_mutex_);
    return probation_.size();
}

template <typename K, typename V, typename Hash>
uint64_t CRP::w_tinylfu::SLRU<K, V, Hash>::protectionSize() const {
    std::shared_lock<std::shared_mutex> protection_read_lock_(protection_mutex_);
    return protection_.size();
}

template <typename K, typename V, typename Hash>
uint64_t CRP::w_tinylfu::SLRU<K, V, Hash>::size() const {
    std::shared_lock<std::shared_mutex> probation_read_lock_(probation_mutex_);
    std::shared_lock<std::shared_mutex> protection_read_lock_(protection_mutex_);
    return probation_.size() + protection_.size();
}

template <typename K, typename V, typename Hash>
uint64_t CRP::w_tinylfu::SLRU<K, V, Hash>::capacity() const {
    return probation_capacity_ + protection_capacity_;
}

/* 判断节点是否存在于主缓存 */
template <typename K, typename V, typename Hash>
bool CRP::w_tinylfu::SLRU<K, V, Hash>::contains(const K& key) const {
    std::shared_lock<std::shared_mutex> probation_read_lock_(probation_mutex_);
    std::shared_lock<std::shared_mutex> protection_read_lock_(protection_mutex_);
    return probation_.contains(key) || protection_.contains(key);
}

template <typename K, typename V, typename Hash>
void CRP::w_tinylfu::SLRU<K, V, Hash>::decay_all_frequencies(double factor) {
    std::unique_lock<std::shared_mutex> protection_write_lock(protection_mutex_);
    for (auto it = protection_->head_->next; it != protection_->head_; it = it->next;) {
        it->frequency = static_cast<uint64_t>(it->frequency * factor);
    }
}

template <typename K, typename V, typename Hash>
bool CRP::w_tinylfu::SLRU<K, V, Hash>::get(const K& key, V& value) {
    /* 因为可能涉及到onAdd和onAccess等节点移动操作，使用写锁 */
    std::scoped_lock<std::shared_mutex, std::shared_mutex> scoped_lock_(probation_mutex_, protection_mutex_);
    if (!key_to_node_.contains(key)) {
        return false;
    }

    auto node = key_to_node_.find(key);
    // 在 protection 部分，直接获取并按照LRU算法移到链表开头
    if (node->is_in_protected) {
        protection_.get(key, value);
        return true;
    }

    if (loading_cache_.contains(key)) {
        loading_cache_.get(key, value);
        /* onAdd 操作 */
        return true;
    }

    if (probation_.contains(key)) {
        probation_.get(key, value);
        /* onAccess 操作 */
        return true;
    }

    return false;
}

/* 插入缓存：若key已存在则更新value，否则插入新条目 
 * 插入缓存时不会触发onAdd和onAccess等升级操作
*/
template <typename K, typename V, typename Hash>
void CRP::w_tinylfu::SLRU<K, V, Hash>::put(const K& key, const V& value) {
    std::scoped_lock<std::shared_mutex, std::shared_mutex> scoped_lock_(probation_mutex_, protection_mutex_);

    if (slru_.contains(key)) {
        slru_.put(key, value);
        return;
    }

    loading_cache_.put(key, value);
    return;
}

} // namespace w_tinylfu
} // namespace CRP
