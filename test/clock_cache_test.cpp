/*
@Author: Lzww  
@Description: Clock缓存单元测试 (Google Test)
@Language: C++17
*/

#include <gtest/gtest.h>
#include "../include/Clock/clock_cache.h"
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <unordered_set>
#include <algorithm>

class ClockCacheTest : public ::testing::Test {
protected:
    std::unique_ptr<ClockCache<std::string, int>> cache;

    void SetUp() override {
        // 每个测试前创建容量为5的缓存
        cache = std::make_unique<ClockCache<std::string, int>>(5);
    }

    void TearDown() override {
        cache.reset();
    }
};

// ================== 基础功能测试 ==================

TEST_F(ClockCacheTest, BasicPutAndGet) {
    int value;
    
    // 测试基本put和get操作
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    EXPECT_TRUE(cache->get("key1", value));
    EXPECT_EQ(value, 100);
    
    EXPECT_TRUE(cache->get("key2", value));
    EXPECT_EQ(value, 200);
    
    EXPECT_TRUE(cache->get("key3", value));
    EXPECT_EQ(value, 300);
    
    // 测试不存在的key
    EXPECT_FALSE(cache->get("nonexistent", value));
}

TEST_F(ClockCacheTest, UpdateExistingKey) {
    int value;
    
    // 插入初始值
    cache->put("key1", 100);
    EXPECT_TRUE(cache->get("key1", value));
    EXPECT_EQ(value, 100);
    
    // 更新值
    cache->put("key1", 999);
    EXPECT_TRUE(cache->get("key1", value));
    EXPECT_EQ(value, 999);
}

TEST_F(ClockCacheTest, ContainsFunction) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_TRUE(cache->contains("key2"));
    EXPECT_FALSE(cache->contains("nonexistent"));
}

TEST_F(ClockCacheTest, SizeTracking) {
    EXPECT_EQ(cache->size(), 0);
    
    cache->put("key1", 100);
    EXPECT_EQ(cache->size(), 1);
    
    cache->put("key2", 200);
    EXPECT_EQ(cache->size(), 2);
    
    cache->remove("key1");
    EXPECT_EQ(cache->size(), 1);
    
    cache->clear();
    EXPECT_EQ(cache->size(), 0);
}

TEST_F(ClockCacheTest, ClearFunction) {
    cache->put("key1", 100);
    cache->put("key2", 200);
    cache->put("key3", 300);
    
    EXPECT_EQ(cache->size(), 3);
    
    cache->clear();
    
    EXPECT_EQ(cache->size(), 0);
    int value;
    EXPECT_FALSE(cache->get("key1", value));
    EXPECT_FALSE(cache->get("key2", value));
    EXPECT_FALSE(cache->get("key3", value));
}

TEST_F(ClockCacheTest, RemoveOperation) {
    int value;
    
    cache->put("key1", 100);
    cache->put("key2", 200);
    
    // 删除存在的key
    cache->remove("key1");
    EXPECT_FALSE(cache->get("key1", value));
    EXPECT_EQ(cache->size(), 1);
    
    // 删除不存在的key（应该不会崩溃）
    cache->remove("nonexistent");
    EXPECT_EQ(cache->size(), 1);
    
    // key2应该仍然存在
    EXPECT_TRUE(cache->get("key2", value));
    EXPECT_EQ(value, 200);
}

// ================== Clock算法特性测试 ==================

TEST_F(ClockCacheTest, BasicEvictionPolicy) {
    int value;
    
    // 填满缓存（容量为5）
    for (int i = 1; i <= 5; ++i) {
        cache->put("key" + std::to_string(i), i * 100);
    }
    EXPECT_EQ(cache->size(), 5);
    
    // 验证所有key都存在
    for (int i = 1; i <= 5; ++i) {
        EXPECT_TRUE(cache->get("key" + std::to_string(i), value));
        EXPECT_EQ(value, i * 100);
    }
    
    // 插入第6个key，应该触发淘汰
    cache->put("key6", 600);
    EXPECT_EQ(cache->size(), 5);
    
    // 验证新key存在
    EXPECT_TRUE(cache->get("key6", value));
    EXPECT_EQ(value, 600);
    
    // 验证某个旧key被淘汰了
    int existing_keys = 0;
    for (int i = 1; i <= 5; ++i) {
        if (cache->get("key" + std::to_string(i), value)) {
            existing_keys++;
        }
    }
    EXPECT_EQ(existing_keys, 4);  // 应该有4个旧key还存在
}

TEST_F(ClockCacheTest, ClockSecondChanceAlgorithm) {
    int value;
    
    // 填满缓存
    for (int i = 1; i <= 5; ++i) {
        cache->put("key" + std::to_string(i), i * 100);
    }
    
    // 访问前4个key，给它们"第二次机会"（设置clock_bit = 1）
    for (int i = 1; i <= 4; ++i) {
        cache->get("key" + std::to_string(i), value);
    }
    
    // 不访问key5，它的clock_bit应该为0
    
    // 插入新key，应该淘汰key5（因为它的clock_bit为0）
    cache->put("new_key", 999);
    
    // key5应该被淘汰
    EXPECT_FALSE(cache->get("key5", value));
    
    // 前4个key应该还存在（因为有第二次机会）
    for (int i = 1; i <= 4; ++i) {
        EXPECT_TRUE(cache->get("key" + std::to_string(i), value));
        EXPECT_EQ(value, i * 100);
    }
    
    // 新key应该存在
    EXPECT_TRUE(cache->get("new_key", value));
    EXPECT_EQ(value, 999);
}

TEST_F(ClockCacheTest, MultipleEvictionRounds) {
    int value;
    
    // 填满缓存
    for (int i = 1; i <= 5; ++i) {
        cache->put("key" + std::to_string(i), i * 100);
    }
    
    // 访问所有key，给它们第二次机会
    for (int i = 1; i <= 5; ++i) {
        cache->get("key" + std::to_string(i), value);
    }
    
    // 插入新key，由于所有现有key都有第二次机会，
    // clock算法应该清除所有clock_bit并选择一个淘汰
    cache->put("new_key1", 1001);
    EXPECT_EQ(cache->size(), 5);
    
    // 再插入一个新key
    cache->put("new_key2", 1002);
    EXPECT_EQ(cache->size(), 5);
    
    // 验证新key存在
    EXPECT_TRUE(cache->get("new_key1", value));
    EXPECT_EQ(value, 1001);
    EXPECT_TRUE(cache->get("new_key2", value));
    EXPECT_EQ(value, 1002);
    
    // 应该有3个原始key被保留
    int remaining_original_keys = 0;
    for (int i = 1; i <= 5; ++i) {
        if (cache->get("key" + std::to_string(i), value)) {
            remaining_original_keys++;
        }
    }
    EXPECT_EQ(remaining_original_keys, 3);
}

// ================== 边界条件测试 ==================

TEST_F(ClockCacheTest, SingleElementCache) {
    auto small_cache = std::make_unique<ClockCache<std::string, int>>(1);
    int value;
    
    small_cache->put("key1", 100);
    EXPECT_TRUE(small_cache->get("key1", value));
    EXPECT_EQ(value, 100);
    EXPECT_EQ(small_cache->size(), 1);
    
    // 插入第二个key应该淘汰第一个
    small_cache->put("key2", 200);
    EXPECT_FALSE(small_cache->get("key1", value));
    EXPECT_TRUE(small_cache->get("key2", value));
    EXPECT_EQ(value, 200);
    EXPECT_EQ(small_cache->size(), 1);
}

TEST_F(ClockCacheTest, ZeroCapacityThrows) {
    bool exception_thrown = false;
    try {
        ClockCache<std::string, int> invalid_cache(0);
    } catch (const std::invalid_argument& e) {
        exception_thrown = true;
    } catch (const std::exception& e) {
        exception_thrown = true;
    }
    EXPECT_TRUE(exception_thrown);
}

TEST_F(ClockCacheTest, EmptyCacheOperations) {
    int value;
    
    // 对空缓存的操作
    EXPECT_FALSE(cache->get("nonexistent", value));
    EXPECT_FALSE(cache->contains("nonexistent"));
    EXPECT_EQ(cache->size(), 0);
    
    // 删除不存在的key
    cache->remove("nonexistent");  // 应该不会崩溃
    EXPECT_EQ(cache->size(), 0);
    
    // 清空空缓存
    cache->clear();  // 应该不会崩溃
    EXPECT_EQ(cache->size(), 0);
}

// ================== 并发安全性测试 ==================

TEST_F(ClockCacheTest, ConcurrentReadWrite) {
    const int num_threads = 4;
    const int ops_per_thread = 100;
    std::vector<std::thread> threads;
    
    // 先填充一些初始数据
    for (int i = 0; i < 10; ++i) {
        cache->put("init_key" + std::to_string(i), i);
    }
    
    // 创建多个线程同时进行读写操作
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, ops_per_thread]() {
            int value;
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                
                // 写操作
                cache->put(key, t * 1000 + i);
                
                // 读操作
                if (cache->get(key, value)) {
                    EXPECT_EQ(value, t * 1000 + i);
                }
                
                // 读取初始数据
                std::string init_key = "init_key" + std::to_string(i % 10);
                cache->get(init_key, value);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证缓存仍然正常工作
    EXPECT_LE(cache->size(), 5);  // 不应超过容量
}

TEST_F(ClockCacheTest, ConcurrentSameKeyOperations) {
    const int num_threads = 8;
    const int iterations = 50;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // 多个线程对同一个key进行操作
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, iterations, &success_count]() {
            int value;
            for (int i = 0; i < iterations; ++i) {
                // 所有线程操作同一个key
                cache->put("shared_key", t * 100 + i);
                
                if (cache->get("shared_key", value)) {
                    success_count.fetch_add(1);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证最终状态
    int final_value;
    EXPECT_TRUE(cache->get("shared_key", final_value));
    EXPECT_GT(success_count.load(), 0);
}

// ================== 性能测试 ==================

TEST_F(ClockCacheTest, PerformanceTest) {
    auto large_cache = std::make_unique<ClockCache<std::string, int>>(1000);
    const int num_operations = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 执行大量操作
    for (int i = 0; i < num_operations; ++i) {
        std::string key = "perf_key" + std::to_string(i % 500);  // 50%重复率
        large_cache->put(key, i);
        
        int value;
        large_cache->get(key, value);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Clock Cache Performance: " << num_operations << " operations in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average: " << (double)duration.count() / num_operations 
              << " microseconds per operation" << std::endl;
    
    // 性能要求：平均每个操作不应超过100微秒
    EXPECT_LT((double)duration.count() / num_operations, 100.0);
}

// ================== 压力测试 ==================

TEST_F(ClockCacheTest, StressTest) {
    const int num_keys = 1000;
    const int num_iterations = 5;
    std::vector<std::string> keys;
    
    // 生成测试key
    for (int i = 0; i < num_keys; ++i) {
        keys.push_back("stress_key" + std::to_string(i));
    }
    
    // 多轮插入和查询
    for (int iter = 0; iter < num_iterations; ++iter) {
        // 插入大量数据
        for (int i = 0; i < num_keys; ++i) {
            cache->put(keys[i], iter * 1000 + i);
        }
        
        // 随机访问，触发Clock算法
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(keys.begin(), keys.end(), gen);
        
        int value;
        for (int i = 0; i < std::min(100, num_keys); ++i) {
            cache->get(keys[i], value);
        }
    }
    
    // 验证缓存保持在容量限制内
    EXPECT_LE(cache->size(), 5);
    
    // 验证缓存仍然可以正常工作
    cache->put("final_test", 9999);
    int value;
    EXPECT_TRUE(cache->get("final_test", value));
    EXPECT_EQ(value, 9999);
}

// ================== 内存泄漏检测辅助测试 ==================

TEST_F(ClockCacheTest, MemoryLeakDetection) {
    // 这个测试主要用于配合valgrind等工具检测内存泄漏
    const int iterations = 100;
    
    for (int i = 0; i < iterations; ++i) {
        // 创建和销毁大量节点
        for (int j = 0; j < 20; ++j) {
            cache->put("temp_key" + std::to_string(j), i * 100 + j);
        }
        
        // 清空缓存
        cache->clear();
    }
    
    EXPECT_EQ(cache->size(), 0);
}

// ================== 主函数 ==================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "==================== Clock Cache Unit Tests ====================" << std::endl;
    std::cout << "Testing Clock cache replacement algorithm implementation..." << std::endl;
    
    int result = RUN_ALL_TESTS();
    
    if (result == 0) {
        std::cout << "✅ All Clock Cache tests passed!" << std::endl;
    } else {
        std::cout << "❌ Some Clock Cache tests failed!" << std::endl;
    }
    
    return result;
} 