/*
@Author: Lzww  
@LastEditTime: 2025-7-9 10:00:00
@Description: FIFO缓存
@Language: C++17
*/

#ifndef FIFO_CACHE_H
#define FIFO_CACHE_H

#include "../utils/node.h"
#include <functional>
#include <string>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

#define DEFAULT_CAPACITY 1024 * 1024

template <typename K, typename V, typename Hash = std::hash<std::string>>
class FIFOCache {
private:
	Node<K, V>* dummy;
	uint64_t capacity;
	uint64_t size;
	
	mutable std::shared_mutex mtx;
	std::unordered_map<K, Node<K, V>*, Hash> keyToNode;

	void remove(Node<K, V>* node);

public:
	FIFOCache();
	FIFOCache(int capacity = DEFAULT_CAPACITY);
	~FIFOCache();
	bool get(const K& key, V& out_value) const;
	void put(const K& key, const V& value);

	void resize(size_t new_capacity);
};

template <typename K, typename V, typename Hash>
FIFOCache<K, V, Hash>::FIFOCache(): FIFOCache(DEFAULT_CAPACITY) {}

template <typename K, typename V, typename Hash>
FIFOCache<K, V, Hash>::FIFOCache(int capacity): capacity(capacity) {
	size = 0;
	dummy = new Node<K, V>();
	dummy->next = dummy;
	dummy->prev = dummy;
}

template <typename K, typename V, typename Hash>
FIFOCache<K, V, Hash>::~FIFOCache() {
	std::unique_lock<std::shared_mutex> lock(mtx);
	
	// 清理所有节点
	while (dummy->next != dummy) {
		Node<K, V>* node = dummy->next;
		remove(node);
		delete node;
	}
	delete dummy;
}

template <typename K, typename V, typename Hash>
void FIFOCache<K, V, Hash>::remove(Node<K, V>* node) {
	if (node == nullptr) return;
	
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

template <typename K, typename V, typename Hash>
bool FIFOCache<K, V, Hash>::get(const K& key, V& out_value) const {
	std::shared_lock<std::shared_mutex> lock(mtx);
	
	auto it = keyToNode.find(key);
	if (it == keyToNode.end()) {
		return false;
	}
	auto node = it->second;
	out_value = node->value;
	return true;
}

template <typename K, typename V, typename Hash>
void FIFOCache<K, V, Hash>::put(const K& key, const V& value) {
	std::unique_lock<std::shared_mutex> lock(mtx);
	
	auto it = keyToNode.find(key);
	if (it != keyToNode.end()) {
		auto node = it->second;
		node->value = std::move(value);
		return;
	}

	auto node = new Node<K, V>(key, value);
	if (size == capacity) {
		auto last = dummy->prev;
		remove(last);
		keyToNode.erase(last->key);
		delete last;
		size--;
	}

	node->next = dummy->next;
	node->prev = dummy;
	dummy->next->prev = node;
	dummy->next = node;
	
	keyToNode[key] = node;
	size++;
}

template <typename K, typename V, typename Hash>
void FIFOCache<K, V, Hash>::resize(size_t new_capacity) {
	std::unique_lock<std::shared_mutex> lock(mtx);
	capacity = new_capacity;
	while (size > capacity) {
		auto last = dummy->prev;
		remove(last);
		keyToNode.erase(last->key);
		delete last;
		size--;
	}
}

#endif