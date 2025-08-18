/*
@Author: Lzww
@LastEditTime: 2025-8-18 21:15:36
@Description: LIRS缓存实现
@Language: C++17
*/

#include "../../include/LIRS/cache.h"

#include <stdexcept>
#include <cstdint>

namespace LIRS {

template <typename K, typename V, typename Hash = std::hash<std::string>>
LIRSCache<K, V, Hash>::LIRSCache(size_t capacity)
    : capacity_(capacity), size_(0), lir_size_(0), hir_size_(0) {
    if (capacity_ == 0) {
        throw std::invalid_argument("capacity must be greater than 0");
    }
    
    /* create sentinel nodes for S stack and Q queue */
    lir_head_ = new LIRSNode<K, V>(K(), V());
    hir_head_ = new LIRSNode<K, V>(K(), V());
    
    /* initialize circular doubly linked lists */
    lir_head_->next_s = lir_head_;
    lir_head_->prev_s = lir_head_;
    
    hir_head_->next_q = hir_head_;
    hir_head_->prev_q = hir_head_;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
LIRSCache<K, V, Hash>::~LIRSCache() {
    clear();
    delete lir_head_;
    delete hir_head_;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void LIRSCache<K, V, Hash>::put(const K& key, const V& value) {
    // write lock
    std::unique_lock<std::mutex> write_lock(mtx_);

    /* a new key */
    if (!contains(key)) {
        /* cache is full, need to evict */
        if (size_ >= capacity_) {
            evict_victim();
        }
        
        /* create new node */
        LIRSNode<K, V> *node = new LIRSNode<K, V>(key, value);
        
        /* add to hash map */
        key_to_node_[key] = node;
        size_++;
        
        /* check if we have space in LIR set */
        size_t max_lir_size = static_cast<size_t>(capacity_ * LIR_RATIO);
        if (lir_size_ < max_lir_size) {
            /* add as LIR block */
            node->is_LIRS = true;
            node->is_resident = true;
            push_to_s(node);
            lir_size_++;
        } else {
            /* add as HIR block */
            node->is_LIRS = false;
            node->is_resident = true;
            push_to_s(node);
            push_to_q(node);
            hir_size_++;
        }
        
        prune_s_stack();
    } else {
        /* existing key - update value */
        LIRSNode<K, V> *node = key_to_node_[key];
        node->value = value;

        if (node->is_LIRS) {        
            /* LIR block: move to top of S stack */
            node->remove_from_s();
            push_to_s(node);
            /* prune S stack to maintain LIR property */
            prune_s_stack();
        } else if (node->is_resident) {                    
            /* HIR resident block: check for promotion */
            /* 计算HIR节点移动前的原始距离 */
            size_t hir_original_distance = get_distance_from_head(node);
            
            node->remove_from_s();
            node->remove_from_q();
            
            /* check if this HIR block should be promoted to LIR */
            if (should_promote_to_lir(node, hir_original_distance)) {
                node->is_LIRS = true;
                hir_size_--;
                lir_size_++;
                push_to_s(node);
                
                /* ensure LIR set doesn't exceed limit */
                size_t max_lir_size = static_cast<size_t>(capacity_ * LIR_RATIO);
                if (lir_size_ > max_lir_size) {
                    demote_lir_to_hir();
                }
            } else {
                /* still HIR, add back to Q */
                push_to_s(node);
                push_to_q(node);
            }
            prune_s_stack();
        } else {
            /* HIR non-resident block: make resident */
            node->is_resident = true;
            node->remove_from_s();
            push_to_s(node);
            push_to_q(node);
            hir_size_++;
            size_++;
            prune_s_stack();
        }
    }
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
std::optional<V> LIRSCache<K, V, Hash>::get(const K& key) {
    // write lock
    std::unique_lock<std::mutex> write_lock(mtx_);

    if (!contains(key)) {
        return std::nullopt;
    }

    LIRSNode<K, V> *node = key_to_node_[key];
    
    /* only access resident blocks */
    if (!node->is_resident) {
        return std::nullopt;
    }

    if (node->is_LIRS) {
        /* LIR block: move to top of S stack */
        node->remove_from_s();
        push_to_s(node);
        prune_s_stack();
    } else {
        /* HIR block: check for promotion */
        /* 计算HIR节点移动前的原始距离 */
        size_t hir_original_distance = get_distance_from_head(node);
        
        node->remove_from_s();
        node->remove_from_q();
        
        /* check if this HIR block should be promoted to LIR */
        if (should_promote_to_lir(node, hir_original_distance)) {
            node->is_LIRS = true;
            hir_size_--;
            lir_size_++;
            push_to_s(node);
            
            /* ensure LIR set doesn't exceed limit */
            size_t max_lir_size = static_cast<size_t>(capacity_ * LIR_RATIO);
            if (lir_size_ > max_lir_size) {
                demote_lir_to_hir();
            }
        } else {
            /* still HIR, add back to Q */
            push_to_s(node);
            push_to_q(node);
        }
        prune_s_stack();
    }

    return std::make_optional(node->value);
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
bool LIRSCache<K, V, Hash>::contains(const K& key) const {
    return key_to_node_.find(key) != key_to_node_.end();
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
size_t LIRSCache<K, V, Hash>::size() const {
    return size_;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
size_t LIRSCache<K, V, Hash>::capacity() const {
    return capacity_;
}


template <typename K, typename V, typename Hash = std::hash<std::string>>
bool LIRSCache<K, V, Hash>::empty() const {
    return size_ == 0;
}


template <typename K, typename V, typename Hash = std::hash<std::string>>
void LIRSCache<K, V, Hash>::clear() {
    std::unique_lock<std::mutex> write_lock(mtx_);

    for (auto& [_, node] : key_to_node_) {
        delete node;
    }
    key_to_node_.clear();
    
    /* reset sentinel nodes */
    lir_head_->next_s = lir_head_;
    lir_head_->prev_s = lir_head_;
    hir_head_->next_q = hir_head_;
    hir_head_->prev_q = hir_head_;
    
    /* reset counters */
    size_ = 0;
    lir_size_ = 0;
    hir_size_ = 0;
}


template <typename K, typename V, typename Hash = std::hash<std::string>>
void LIRSCache<K, V, Hash>::push_to_s(LIRSNode<K, V> *node) {
    node->next_s = lir_head_->next_s;
    node->prev_s = lir_head_;
    node->next_s->prev_s = node;
    node->prev_s->next_s = node;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void LIRSCache<K, V, Hash>::push_to_q(LIRSNode<K, V> *node) {
    node->next_q = hir_head_->next_q;
    node->prev_q = hir_head_;
    node->next_q->prev_q = node;
    node->prev_q->next_q = node;
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void LIRSCache<K, V, Hash>::evict_victim() {
    /* evict HIR block from front of Q queue */
    if (hir_head_->next_q != hir_head_) {
        LIRSNode<K, V> *victim = hir_head_->next_q;
        victim->remove_from_q();
        victim->is_resident = false;
        hir_size_--;
        size_--;
        
        /* keep non-resident HIR block in S stack for history */
        /* it will be cleaned up during pruning */
    }
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void LIRSCache<K, V, Hash>::prune_s_stack() {
    /* remove non-resident HIR blocks from bottom of S stack */
    LIRSNode<K, V> *node = lir_head_->prev_s;
    
    while (node != lir_head_) {
        LIRSNode<K, V> *prev = node->prev_s;
        
        if (!node->is_resident && !node->is_LIRS) {
            /* non-resident HIR block - remove from stack and delete */
            node->remove_from_s();
            key_to_node_.erase(node->key);
            delete node;
        } else if (node->is_LIRS) {
            /* reached LIR block - stop pruning */
            break;
        } else {
            /* resident HIR block - stop pruning */
            break;
        }
        
        node = prev;
    }
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void LIRSCache<K, V, Hash>::demote_lir_to_hir() {
    /* 
     * 降级距离lir_head_最远的LIR节点为HIR
     * 这与should_promote_to_lir中的逻辑保持一致
     */
    
    // 找到距离lir_head_最远的LIR节点（从栈底向上找第一个LIR节点）
    LIRSNode<K, V> *node = lir_head_->prev_s;  // 栈底开始
    
    while (node != lir_head_) {
        if (node->is_LIRS) {
            /* 降级这个最远的LIR节点为HIR */
            node->is_LIRS = false;
            lir_size_--;
            hir_size_++;
            
            /* 添加到Q队列 */
            push_to_q(node);
            break;
        }
        node = node->prev_s;
    }
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
size_t LIRSCache<K, V, Hash>::get_distance_from_head(LIRSNode<K, V> *target_node) {
    /* 计算指定节点距离lir_head_的距离 */
    LIRSNode<K, V> *current = lir_head_->next_s;  // 从栈顶开始
    size_t distance = 1;
    
    while (current != lir_head_) {
        if (current == target_node) {
            return distance;
        }
        current = current->next_s;
        distance++;
    }
    
    return SIZE_MAX;  // 节点不在S栈中
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
bool LIRSCache<K, V, Hash>::should_promote_to_lir(LIRSNode<K, V> *hir_node, size_t hir_original_distance) {
    size_t max_lir_size = static_cast<size_t>(capacity_ * LIR_RATIO);
    
    /* Always promote if there's space in LIR set */
    if (lir_size_ < max_lir_size) {
        return true;
    }
    
    /* 
     * 当HIR节点距离lir_head_的距离比最远的LIR节点要近时，提升HIR为LIR
     * 距离定义为在S栈中从head开始的位置
     */
    
    // 找到距离lir_head_最远的LIR节点（从栈底向上找第一个LIR节点）
    LIRSNode<K, V> *farthest_lir = nullptr;
    size_t farthest_lir_distance = 0;
    
    // 从栈底开始向上找最远的LIR节点
    LIRSNode<K, V> *current = lir_head_->prev_s;  // 栈底
    size_t distance = 1;
    
    while (current != lir_head_) {
        if (current->is_LIRS) {
            farthest_lir = current;
            farthest_lir_distance = distance;
            break;  // 找到第一个（最远的）LIR节点就停止
        }
        current = current->prev_s;
        distance++;
    }
    
    if (farthest_lir == nullptr) {
        return false;  // 没有找到LIR节点，不应该发生
    }
    
    // 使用HIR节点移动之前的原始距离进行比较
    // 如果HIR节点原始距离更近（距离值更小），则提升
    return hir_original_distance < farthest_lir_distance;
}

} // namespace LIRS