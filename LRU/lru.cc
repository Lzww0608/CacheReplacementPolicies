#include "lru.h"
#include <stdexcept>

template <typename K, typename V>
void LRU<K, V>::remove(Node<K, V> *node) {
    if (node == nullptr) {
        throw std::invalid_argument("Node is nullptr");
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;
}

template <typename K, typename V>
void LRU<K, V>::pushToFront(Node<K, V> *node) {
    if (node == nullptr) {
        throw std::invalid_argument("Node is nullptr");
    }

    node->prev = head;
    node->next = head->next;
    node->next->prev = node;
    node->prev->next = node;
}


template <typename K, typename V>
LRU<K, V>::LRU(int capacity): capacity(capacity) {
    head = new Node<K, V>(0, 0);
    head->next = head;
    head->prev = head;
}


// 1. std::optional<V> LRU<K, V>::get(const K& key)
// 2. const V* LRU<K, V>::get(const K& key)
// 3 bool LRU<K, V>::get(const K& key, V& out_value) 
// 零拷贝，与C风格API兼容
template <typename K, typename V>
bool LRU<K, V>::get(const K& key, V& out_value) {
    if (keyToNode.find(key) == keyToNode.end()) {
        return false;
    }

    Node<K, V> *node = keyToNode[key];
    remove(node);
    pushToFront(node);
    out_value = node->value;
    return true;
}

template <typename K, typename V>
void LRU<K, V>::put(K& key, V& value) {
    if (get(key, value)) {
        keyToNode[key]->value = value;
        return;
    }

    if (keyToNode.size() >= capacity) {
        Node<K, V> *node = head->prev;
        remove(node);
        keyToNode.erase(node->key);
        delete node;
    }

    Node<K, V> *newNode = new Node<K, V>(std::move(key), std::move(value));
    pushToFront(newNode);
    keyToNode[key] = newNode;
}