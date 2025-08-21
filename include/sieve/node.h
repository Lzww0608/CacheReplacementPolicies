/*
@Author: Lzww
@LastEditTime: 2025-8-21 22:39:43
@Description: Sieve Cache Node
@Language: C++17
*/

#ifndef SIEVE_NODE_H
#define SIEVE_NODE_H

#include <cstdint>
using namespace CRP;

namespace Sieve {

template <typename K, typename V>
struct Node {
    K key;
    V value;
    uint8_t visited;
    Node* prev;
    Node* next;
    
    Node(K key, V value, uint8_t visited = 0) : key(key), value(value), visited(visited) {}
    Node(const Node& other) : key(other.key), value(other.value), visited(other.visited) {}
    Node(Node&& other) noexcept : key(std::move(other.key)), value(std::move(other.value)), visited(other.visited) {}
    Node& operator=(const Node& other) {
        if (this != &other) {
            key = other.key;
            value = other.value;
            visited = other.visited;
        }
        return *this;
    }
    Node& operator=(Node&& other) noexcept {
        if (this != &other) {
            key = std::move(other.key);
            value = std::move(other.value);
            visited = other.visited;
        }
        return *this;
    }
    ~Node() = default;
    bool operator==(const Node& other) const {
        return key == other.key && value == other.value && visited == other.visited;
    }
    bool operator!=(const Node& other) const {
        return !(*this == other);
    }
    bool operator<(const Node& other) const {
        return key < other.key;
    }
    bool operator>(const Node& other) const {
        return key > other.key;
    }
    bool operator<=(const Node& other) const {
        return key <= other.key;
    }
    bool operator>=(const Node& other) const {
        return key >= other.key;
    }

};


} // namespace Sieve

#endif // SIEVE_NODE_H