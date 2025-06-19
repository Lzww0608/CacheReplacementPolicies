#ifndef LRU_H
#define LRU_H

#include <unordered_map>

template <typename K, typename V>
struct Node {
    K key;
    V value;
    Node *prev, *next;
    Node(K k, V v) : key(k), value(v), prev(nullptr), next(nullptr) {}
    Node(K&& k, V&& v) : key(std::move(k)), value(std::move(v)), prev(nullptr), next(nullptr) {}
}


template <typename K, typename V>
class LRU {

private:
    std::unordered_map<K, Node<K, V>*> keyToNode;
    Node<K, V> *head;
    int capacity;

public:
    LRU(int capacity);
    bool get(const K& key, V& out_value);
    void put(K& key, V& value);

    void remove(Node<K, V> *node);
    void pushToFront(Node<K, V> *node);
};

#endif // LRU_H