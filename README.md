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

- **LIRS (Low Inter-reference Recency Set)**

  - Advanced cache replacement policy designed to improve performance over LRU, especially for workloads with weak locality
  - Classifies cache blocks into LIR (Low Inter-reference Recency) and HIR (High Inter-reference Recency) based on their access patterns
  - Maintains a stack (recency list) and a queue to track block status and recency efficiently
  - Evicts HIR blocks preferentially, while LIR blocks are protected from eviction until their access pattern changes
  - Adapts dynamically to changing workload characteristics, providing high hit ratios for a wide range of access patterns
  - Particularly effective for databases, file systems, and other applications with non-uniform or looping access patterns

- **S3FIFO (Static-Dynamic-Static FIFO)**
  - Three-tier cache replacement policy with small (S), main (M), and ghost (G) queues
  - New entries start in the small FIFO queue; frequently accessed items are promoted to the main queue
  - Uses second-chance algorithm in the main queue for better hit ratios
  - Ghost queue tracks recently evicted items to handle re-access patterns efficiently
  - Provides excellent performance with simple implementation and low overhead
  - Particularly effective for workloads with mixed access patterns and temporal locality

- **Sieve**
  - Simple and efficient cache replacement policy that approximates FIFO with better hit ratios
  - Uses a single "visited" bit per cache entry to track recent access
  - During eviction, scans entries in FIFO order, clearing visited bits and skipping visited entries
  - Evicts the first unvisited entry encountered, providing a second chance for recently accessed items
  - Maintains FIFO insertion order while giving preference to recently accessed entries
  - Offers excellent performance with minimal memory overhead and simple implementation
  - Particularly effective for workloads with temporal locality and streaming access patterns

- **GDSF (Greedy Dual Size Frequency)**
  - Advanced cache replacement policy that considers both object size and access frequency
  - Assigns a priority value to each cache entry based on frequency, size, and aging factor
  - Prioritizes smaller, more frequently accessed objects for better cache utilization
  - Uses an aging mechanism to prevent stale entries from occupying cache indefinitely
  - Particularly effective for web caches and content delivery networks where object sizes vary significantly
  - Provides excellent hit ratios and byte hit ratios for heterogeneous workloads
  - Balances between maximizing hit count and minimizing bandwidth usage

- **MGLRU (Multi-Generational LRU)**
  - Advanced cache replacement policy that extends traditional LRU with multiple generations
  - Divides cache entries into multiple age groups (generations) based on access recency and frequency
  - Uses a hierarchical aging mechanism where entries progress through generations based on access patterns
  - Maintains separate LRU lists for each generation, allowing fine-grained eviction decisions
  - Provides better scan resistance compared to traditional LRU by protecting frequently accessed items
  - Adapts to workload changes by dynamically adjusting generation boundaries and promotion policies
  - Particularly effective for memory management in operating systems and large-scale caching systems
  - Offers excellent performance for workloads with mixed temporal and spatial locality patterns

## ToDo:

- **BRRIP (Bimodal Re-Reference Interval Prediction)**
  - Enhanced version of SRRIP that uses bimodal insertion policy
  - Combines the benefits of LRU and SRRIP by using two different insertion policies
  - Most new entries are inserted with high RRPV (SRRIP behavior), while a small fraction are inserted with low RRPV (LRU behavior)
  - Uses a bimodal throttle parameter (typically ε = 1/32) to control the insertion policy selection
  - Provides robustness against both scan-resistant and thrash-resistant workloads
  - Delivers superior performance compared to pure SRRIP or LRU across diverse access patterns
  - Widely adopted in modern processor caches and high-performance storage systems

- **DRRIP (Dynamic Re-Reference Interval Prediction)**
  - Adaptive cache replacement policy that dynamically switches between SRRIP and BRRIP
  - Uses set dueling technique to continuously monitor the performance of both policies
  - Maintains dedicated sets for SRRIP and BRRIP, with follower sets adopting the better-performing policy
  - Employs a Policy Selection Counter (PSEL) to track which policy is currently performing better
  - Automatically adapts to changing workload characteristics without manual tuning
  - Provides the best of both worlds: scan resistance and thrash resistance
  - Particularly effective for applications with varying or unknown access patterns
  - Represents the state-of-the-art in adaptive cache replacement policies



## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
