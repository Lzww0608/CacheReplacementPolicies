/*
@Author: Lzww
@LastEditTime: 2025-8-20 18:48:42
@Description: S3FIFO Cache Unit Tests
@Language: C++17
*/

#include <gtest/gtest.h>
#include "../include/s3fifo/cache.h"
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <random>

using namespace S3FIFO;

class S3FIFOCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<S3FIFOCache<std::string, int>>(10, 0.1); // 10 capacity, 10% for S queue
    }

    void TearDown() override {
        cache.reset();
    }

    std::unique_ptr<S3FIFOCache<std::string, int>> cache;
};

// 基础功能测试
TEST_F(S3FIFOCacheTest, BasicInsertAndGet) {
    // Test basic put/get operations
    cache->put("key1", 100);
    auto result = cache->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 100);

    // Test get non-existent key
    auto result2 = cache->get("nonexistent");
    EXPECT_FALSE(result2.has_value());
}

TEST_F(S3FIFOCacheTest, CacheCapacity) {
    // Test initial state
    EXPECT_EQ(cache->capacity(), 10);
    EXPECT_EQ(cache->size(), 0);
    EXPECT_TRUE(cache->empty());

    // Add some items and access them to keep them in cache
    for (int i = 0; i < 5; ++i) {
        cache->put("key" + std::to_string(i), i);
        cache->get("key" + std::to_string(i)); // Access to promote to M queue
    }
    
    EXPECT_EQ(cache->size(), 5);
    EXPECT_FALSE(cache->empty());
}

TEST_F(S3FIFOCacheTest, SmallQueueBehavior) {
    // With 10% ratio and capacity 10, S queue should have capacity 1
    // Test that items go to S queue first
    
    cache->put("key1", 1);
    EXPECT_EQ(cache->size(), 1);
    
    // Verify we can retrieve from S queue
    auto result = cache->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
}

TEST_F(S3FIFOCacheTest, EvictionFromSQueue) {
    // Fill S queue (capacity = 1) and one more to trigger eviction
    cache->put("key1", 1);
    cache->put("key2", 2);
    
    // key1 should be moved to ghost queue, key2 should be in S queue
    auto result1 = cache->get("key1");
    auto result2 = cache->get("key2");
    
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), 2);
    
    // key1 might be in ghost queue - if we access it, it should be promoted
    EXPECT_EQ(cache->size(), 2); // Both should be accessible
}

TEST_F(S3FIFOCacheTest, PromotionFromSToM) {
    // Test promotion from S queue to M queue when item is accessed
    cache->put("key1", 1);
    
    // Access the item to set its clock bit
    cache->get("key1");
    
    // Add another item to trigger eviction from S
    cache->put("key2", 2);
    
    // key1 should now be in M queue (promoted due to access)
    auto result = cache->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
}

TEST_F(S3FIFOCacheTest, GhostQueueHit) {
    // Test ghost queue hit and promotion to M queue
    cache->put("key1", 1);
    cache->put("key2", 2); // This should move key1 to ghost queue
    
    // Now access key1 which should be in ghost queue
    auto result = cache->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
    
    // key1 should now be in M queue
}

TEST_F(S3FIFOCacheTest, FullCapacityBehavior) {
    // Fill the cache to full capacity
    for (int i = 0; i < 15; ++i) {  // More than capacity to test eviction
        cache->put("key" + std::to_string(i), i);
    }
    
    EXPECT_LE(cache->size(), cache->capacity());
    
    // Some of the later items should still be accessible
    for (int i = 10; i < 15; ++i) {
        auto result = cache->get("key" + std::to_string(i));
        EXPECT_TRUE(result.has_value());
    }
}

TEST_F(S3FIFOCacheTest, UpdateExistingKey) {
    // Test updating an existing key
    cache->put("key1", 100);
    cache->put("key1", 200); // Update same key
    
    auto result = cache->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 200);
}

TEST_F(S3FIFOCacheTest, ClearCache) {
    // Add some items and access them to keep them in cache
    for (int i = 0; i < 5; ++i) {
        cache->put("key" + std::to_string(i), i);
        cache->get("key" + std::to_string(i)); // Access to promote to M queue
    }
    
    EXPECT_EQ(cache->size(), 5);
    
    // Clear cache
    cache->clear();
    
    EXPECT_EQ(cache->size(), 0);
    EXPECT_TRUE(cache->empty());
    
    // Verify items are gone
    auto result = cache->get("key1");
    EXPECT_FALSE(result.has_value());
}

TEST_F(S3FIFOCacheTest, SecondChanceAlgorithm) {
    // Test second chance algorithm in M queue
    // This is a more complex test to verify clock bit behavior
    
    // Create a larger cache for this test
    auto large_cache = std::make_unique<S3FIFOCache<std::string, int>>(20, 0.2);
    
    // Fill S queue and promote items to M
    for (int i = 0; i < 10; ++i) {
        large_cache->put("key" + std::to_string(i), i);
        large_cache->get("key" + std::to_string(i)); // Access to set clock bit
    }
    
    // Add more items to fill M queue
    for (int i = 10; i < 25; ++i) {
        large_cache->put("new_key" + std::to_string(i), i);
    }
    
    // Some original items should still be accessible due to second chance
    int accessible_count = 0;
    for (int i = 0; i < 5; ++i) {
        auto result = large_cache->get("key" + std::to_string(i));
        if (result.has_value()) {
            accessible_count++;
        }
    }
    
    // At least some items should have gotten second chance
    EXPECT_GT(accessible_count, 0);
}

// 线程安全测试
TEST_F(S3FIFOCacheTest, ThreadSafety) {
    const int num_threads = 4;
    const int operations_per_thread = 1000;
    std::vector<std::thread> threads;
    
    // Launch multiple threads doing concurrent operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 99);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = dis(gen);
                std::string key_str = "thread" + std::to_string(t) + "_key" + std::to_string(key);
                
                if (i % 3 == 0) {
                    // Put operation
                    cache->put(key_str, key);
                } else {
                    // Get operation
                    cache->get(key_str);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Cache should still be in valid state
    EXPECT_LE(cache->size(), cache->capacity());
}

// 性能测试
TEST_F(S3FIFOCacheTest, PerformanceTest) {
    const int num_operations = 10000;
    auto large_cache = std::make_unique<S3FIFOCache<std::string, int>>(1000, 0.1);
    
    // Measure put performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        large_cache->put("perf_key" + std::to_string(i), i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto put_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Measure get performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations / 2; ++i) {
        large_cache->get("perf_key" + std::to_string(i));
    }
    end = std::chrono::high_resolution_clock::now();
    auto get_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Print results (or just ensure operations complete in reasonable time)
    EXPECT_LT(put_duration.count(), 1000000); // Less than 1 second
    EXPECT_LT(get_duration.count(), 500000);  // Less than 0.5 second
    
    std::cout << "Put operations: " << num_operations << " in " 
              << put_duration.count() << " microseconds" << std::endl;
    std::cout << "Get operations: " << num_operations/2 << " in " 
              << get_duration.count() << " microseconds" << std::endl;
}

// 边界情况测试
TEST_F(S3FIFOCacheTest, S3FIFOCorrectBehavior) {
    // Test S3FIFO specific behavior: unaccessed items go to ghost queue
    
    // Add items without accessing them - they should go to ghost queue
    cache->put("key1", 1);
    cache->put("key2", 2);
    cache->put("key3", 3);
    
    // With S queue capacity = 1, only the last item should count toward size
    EXPECT_EQ(cache->size(), 1);
    
    // But all items should still be accessible via ghost queue hits
    auto result1 = cache->get("key1");
    auto result2 = cache->get("key2");
    auto result3 = cache->get("key3");
    
    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
    
    EXPECT_EQ(result1.value(), 1);
    EXPECT_EQ(result2.value(), 2);
    EXPECT_EQ(result3.value(), 3);
    
    // After accessing them, they should be in M queue and count toward size
    EXPECT_EQ(cache->size(), 3);
}

TEST_F(S3FIFOCacheTest, EdgeCases) {
    // Test with capacity 1
    auto tiny_cache = std::make_unique<S3FIFOCache<std::string, int>>(1, 0.5);
    
    tiny_cache->put("key1", 1);
    EXPECT_EQ(tiny_cache->size(), 1);
    
    tiny_cache->put("key2", 2);
    EXPECT_EQ(tiny_cache->size(), 1);
    
    // Test with very large capacity
    auto huge_cache = std::make_unique<S3FIFOCache<std::string, int>>(100000, 0.1);
    for (int i = 0; i < 1000; ++i) {
        huge_cache->put("huge_key" + std::to_string(i), i);
    }
    EXPECT_EQ(huge_cache->size(), 1000);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
