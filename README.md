# CacheReplacementPolicies
 
A comprehensive C++ implementation of various cache replacement policies. This project provides efficient and well-tested implementations of common cache replacement algorithms used in computer systems, including LRU (Least Recently Used), LFU (Least Frequently Used), FIFO (First In First Out), and other popular caching strategies.


## Supported Policies

- **LRU (Least Recently Used)**: Evicts the least recently accessed item
- **LFU (Least Frequently Used)**: Evicts the least frequently accessed item
- **FIFO (First In First Out)**: Evicts items in the order they were added
- **Clock**: Circular buffer-based replacement policy
- **2Q**: Two-queue algorithm that uses a small FIFO queue (A1in) for new items and a large LRU queue (Am) for frequently accessed items, providing better hit ratios than simple LRU
- **ARC (Adaptive Replacement Cache)**: Self-tuning cache replacement policy
- **TinyLFU**: A high-performance cache replacement policy that uses a frequency sketch to track item access patterns with minimal memory overhead, combining the benefits of LFU with efficient space utilization through probabilistic data structures

This project serves as both a practical library for cache implementations and an educational resource for understanding different cache replacement policies.
