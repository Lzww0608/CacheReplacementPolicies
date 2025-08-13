# Cache Replacement Policies (CRP)
 
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

A comprehensive, high-performance C++ library implementing various cache replacement policies with modern C++17 features.
This project provides efficient, thread-safe, and well-tested implementations of common cache replacement algorithms used in computer systems, databases, and distributed systems.

## 📋 Supported Cache Replacement Policies

### Core Policies

- **LRU (Least Recently Used)**

  - Evicts the least recently accessed item
  - O(1) get/put operations with hash table + doubly linked list
  - Thread-safe sharded implementation for high concurrency
  - Built-in TTL support with background cleanup

- **LFU (Least Frequently Used)**

  - Evicts the least frequently accessed item
  - O(1) operations using frequency-based data structures
  - Automatic frequency decay to handle changing access patterns
  - Sharded design for concurrent access

- **FIFO (First In First Out)**
  - Evicts items in the order they were added
  - Simple queue-based implementation
  - Thread-safe with shared mutex for read/write operations
  - Minimal memory overhead

### Advanced Policies

- **Clock Algorithm**

  - Circular buffer-based replacement policy with reference bits
  - Approximates LRU with lower overhead
  - Good balance between performance and cache hit ratio

- **2Q (Two Queue)**

  - Uses a small FIFO queue (A1in) for new items and a large LRU queue (Am) for frequently accessed items
  - Provides better hit ratios than simple LRU for many workloads
  - Adaptive to different access patterns

- **ARC (Adaptive Replacement Cache)**

  - Self-tuning cache replacement policy
  - Dynamically balances between recency and frequency
  - Adapts to workload characteristics automatically

- **W-TinyLFU (Window Tiny Least Frequently Used)**
  - High-performance cache replacement policy using frequency sketches
  - Combines LRU window with TinyLFU main cache
  - Uses Bloom filters for space-efficient frequency tracking
  - Minimal memory overhead through probabilistic data structures
  - Excellent performance for real-world workloads

- **SRRIP (Static Re-Reference Interval Prediction)**

  - Cache replacement policy based on re-reference interval prediction
  - Assigns a Re-Reference Prediction Value (RRPV) counter to each cache entry to estimate the likelihood of future access
  - Newly inserted entries are given a high RRPV, making them more likely to be evicted if not accessed soon
  - Eviction is performed by decrementing RRPV counters and selecting entries with the maximum RRPV; efficient for both hardware and software implementations
  - Delivers excellent performance under high concurrency and complex access patterns; widely used in high-performance caching systems


## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.


