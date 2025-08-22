/*
@Author: Lzww
@LastEditTime: 2025-8-22 21:57:34
@Description: Sieve Cache
@Language: C++17
*/

#include "cache.h"

#include <stdexcept>
#include <sstream>

using namespace CRP;

namespace Sieve {

template <typename K, typename V>
void Cache<K, V>::addToHead(Node<K, V>* node) {
    if (node == nullptr) {
        throw std::invalid_argument("Node is nullptr");
    }

    node->prev = head_;
    node->next = head_->next;
    node->prev->next = node;
    node->next->prev = node;
}

template <typename K, typename V>
void Cache<K, V>::removeNode(Node<K, V>* node) {
    if (node == nullptr) {
        throw std::invalid_argument("Node is nullptr");
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;
}

template <typename K, typename V>
void Cache<K, V>::moveToHead(Node<K, V>* node) {
    removeNode(node);
    addToHead(node);
}


template <typename K, typename V>
Node<K, V>* Cache<K, V>::evict() {
    if (tail_->prev == head_) {
        return nullptr;
    }

    while (hand_->visited != 0) {
        if (hand_ != head_) {
            hand_->visited = 0;
        }        
        hand_ = hand_->next;
    }
    auto next = hand_->next;
    removeNode(hand_);
    map_.erase(hand_->key);
    delete hand_;
    hand_ = next;
    return hand_;
}

template <typename K, typename V>
Cache<K, V>::Cache(uint32_t capacity) : capacity_(capacity) {
    head_ = new Node<K, V>(0, 0);
    hand_ = head_;
    hand_->visited = 1;
    head_->next = hand_;
    hand_->prev = head_;
}

template <typename K, typename V>
Cache<K, V>::~Cache() {
    while (head_ != nullptr) {
        Node<K, V>* temp = head_;
        head_ = head_->next;
        delete temp;
    }
    delete head_;
}

template <typename K, typename V>
std::optional<V> Cache<K, V>::get(const K& key) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        return std::nullopt;
    }

    auto node = it->second;
    node->visited = 1;
    return node->value;
}

template <typename K, typename V>
void Cache<K, V>::put(const K& key, const V& value) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = map_.find(key);
    if (it != map_.end()) {
        auto node = it->second;
        node->value = value;
        node->visited = 1;
        return;
    } 

    if (map_.size() >= capacity_) {
        evict();
    }

    auto node = new Node<K, V>(key, value);
    node->next = hand_;
    node->prev = hand_->prev;
    node->prev->next = node;
    node->next->prev = node;
    map_[key] = node;
    return;
}

template <typename K, typename V>
bool Cache<K, V>::del(const K& key) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        return false;
    }

    auto node = it->second;
    removeNode(node);
    map_.erase(key);
    delete node;
    return true;
}

template <typename K, typename V>
uint32_t Cache<K, V>::size() const {
    return map_.size();
}

template <typename K, typename V>
bool Cache<K, V>::empty() const {
    return map_.empty();
}


template <typename K, typename V>
std::string Cache<K, V>::toString() const {
    std::stringstream ss;
    ss << "Cache: ";
    for (auto it = head_->next; it != hand_; it = it->next) {
        ss << it->key << "=" << it->value << " ";
    }
    return ss.str();
}
} // namespace Sieve