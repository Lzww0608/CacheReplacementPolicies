/*
@Author: Lzww
@Description: S3FIFO Cache Demo
@Language: C++17
*/

#include "../include/s3fifo/cache.h"
#include <iostream>
#include <string>
#include <chrono>
#include <random>

using namespace S3FIFO;

int main() {
    std::cout << "S3FIFO Cache Demo" << std::endl;
    std::cout << "=================" << std::endl;
    
    // Create a cache with capacity 10, 10% for S queue (1 slot), 90% for M queue (9 slots)
    S3FIFOCache<std::string, int> cache(10, 0.1);
    
    std::cout << "Cache created with capacity: " << cache.capacity() << std::endl;
    std::cout << "Initial size: " << cache.size() << std::endl;
    std::cout << "Is empty: " << (cache.empty() ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
    
    // Basic operations
    std::cout << "1. Basic Put and Get Operations:" << std::endl;
    cache.put("key1", 100);
    cache.put("key2", 200);
    cache.put("key3", 300);
    
    std::cout << "Put key1=100, key2=200, key3=300" << std::endl;
    std::cout << "Cache size: " << cache.size() << std::endl;
    
    auto result = cache.get("key1");
    if (result.has_value()) {
        std::cout << "key1 = " << result.value() << std::endl;
    } else {
        std::cout << "key1 not found" << std::endl;
    }
    
    result = cache.get("key2");
    if (result.has_value()) {
        std::cout << "key2 = " << result.value() << std::endl;
    } else {
        std::cout << "key2 not found" << std::endl;
    }
    
    std::cout << std::endl;
    
    // Test S queue behavior
    std::cout << "2. Testing S Queue Eviction:" << std::endl;
    std::cout << "Adding more items to trigger eviction from S queue..." << std::endl;
    
    for (int i = 4; i <= 8; ++i) {
        cache.put("key" + std::to_string(i), i * 100);
        std::cout << "Put key" << i << "=" << i * 100 << ", Cache size: " << cache.size() << std::endl;
    }
    
    std::cout << std::endl;
    
    // Test promotion behavior
    std::cout << "3. Testing Promotion from S to M Queue:" << std::endl;
    cache.put("promote_me", 999);
    std::cout << "Put promote_me=999" << std::endl;
    
    // Access it to set clock bit
    result = cache.get("promote_me");
    if (result.has_value()) {
        std::cout << "Got promote_me = " << result.value() << " (clock bit set)" << std::endl;
    }
    
    // Add another item to trigger promotion
    cache.put("trigger", 111);
    std::cout << "Put trigger=111 (should promote promote_me to M queue)" << std::endl;
    
    // Verify promote_me is still accessible
    result = cache.get("promote_me");
    if (result.has_value()) {
        std::cout << "promote_me still accessible = " << result.value() << std::endl;
    } else {
        std::cout << "promote_me was evicted" << std::endl;
    }
    
    std::cout << std::endl;
    
    // Test cache at full capacity
    std::cout << "4. Testing Full Cache Behavior:" << std::endl;
    std::cout << "Filling cache to capacity..." << std::endl;
    
    for (int i = 10; i < 20; ++i) {
        cache.put("full" + std::to_string(i), i);
        if (i % 3 == 0) {
            std::cout << "Cache size: " << cache.size() << std::endl;
        }
    }
    
    std::cout << "Final cache size: " << cache.size() << std::endl;
    std::cout << "Cache capacity: " << cache.capacity() << std::endl;
    
    std::cout << std::endl;
    
    // Performance test
    std::cout << "5. Performance Test:" << std::endl;
    S3FIFOCache<std::string, int> perf_cache(1000, 0.1);
    
    const int num_ops = 10000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1999);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Mix of puts and gets
    for (int i = 0; i < num_ops; ++i) {
        int key_num = dis(gen);
        std::string key = "perf_key" + std::to_string(key_num);
        
        if (i % 3 == 0) {
            // Get operation
            perf_cache.get(key);
        } else {
            // Put operation
            perf_cache.put(key, key_num);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Performed " << num_ops << " mixed operations in " << duration.count() << " ms" << std::endl;
    std::cout << "Performance cache final size: " << perf_cache.size() << std::endl;
    
    std::cout << std::endl;
    
    // Clear test
    std::cout << "6. Clear Cache Test:" << std::endl;
    std::cout << "Cache size before clear: " << cache.size() << std::endl;
    cache.clear();
    std::cout << "Cache size after clear: " << cache.size() << std::endl;
    std::cout << "Is empty after clear: " << (cache.empty() ? "Yes" : "No") << std::endl;
    
    std::cout << std::endl;
    std::cout << "S3FIFO Cache Demo completed successfully!" << std::endl;
    
    return 0;
}
