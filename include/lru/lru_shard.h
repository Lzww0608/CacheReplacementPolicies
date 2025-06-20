/*
@Author: Lzww
@LastEditTime: 2025-6-20 21:07:30
@Description: LRU缓存分片实现
@Language: C++17
*/

#ifndef LRU_SHARD_H
#define LRU_SHARD_H

#include "node.h"
#include <mutex>
#include <unordered_map>



template<typename K, typename V>
class LRUShard {

private:
    std::unordered_map<K, Node<K, V>*> keyToNode;
    Node<K, V> *head;
    int capacity;
    std::mutex mtx;

public:
    LRUShard(int capacity);
    bool get(const K& key, V& out_value);
    void put(K& key, V& value);

    void pushToFront(Node<K, V> *node);
};


template <typename K, typename V>
LRUShard<K, V>::LRUShard(int capacity): capacity(capacity) {
    head = new Node<K, V>(K{}, V{});
    head->next = head;
    head->prev = head;
}


// 1. std::optional<V> LRUShard<K, V>::get(const K& key)
// 2. const V* LRUShard<K, V>::get(const K& key)
// 3 bool LRUShard<K, V>::get(const K& key, V& out_value) 
// 零拷贝，与C风格API兼容
template <typename K, typename V>
bool LRUShard<K, V>::get(const K& key, V& out_value) {
    std::lock_guard<std::mutex> lock(mtx);
    if (keyToNode.find(key) == keyToNode.end()) {
        return false;
    }

    Node<K, V> *node = keyToNode[key];
    if (node->expire_time < std::chrono::steady_clock::now()) {
        remove(node);
        keyToNode.erase(key);
        delete node;
        return false;
    }
    remove(node);
    pushToFront(node);
    node->expire_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(DEFAULT_EXPIRE_TIME);
    out_value = node->value;
    return true;
}

template <typename K, typename V>
void LRUShard<K, V>::put(K& key, V& value, int expire_time = DEFAULT_EXPIRE_TIME) {
    if (get(key, value)) {
        keyToNode[key]->value = value;
        return;
    }

    std::lock_guard<std::mutex> lock(mtx);
    if (keyToNode.size() >= capacity) {
        Node<K, V> *node = head->prev;
        remove(node);
        keyToNode.erase(node->key);
        delete node;
    }

    Node<K, V> *newNode = new Node<K, V>(std::move(key), std::move(value), std::chrono::steady_clock::now() + std::chrono::milliseconds(expire_time));
    pushToFront(newNode);
    keyToNode[key] = newNode;
}

template <typename K, typename V>
void LRUShard<K, V>::pushToFront(Node<K, V> *node) {
    if (node == nullptr) {
        throw std::runtime_error("Node is nullptr");
    }

    node->prev = head;
    node->next = head->next;
    node->next->prev = node;
    node->prev->next = node;

    //node->expire_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(DEFAULT_EXPIRE_TIME);
}


#endif