/*
@Author: Lzww
@Description: GTest测试用例 - GDSF缓存
@Language: C++17
*/

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <iostream>

// 添加CRP命名空间声明
namespace CRP {}

#include "../include/GDSF/cache.h"
#include "../src/GDSF/cache.cpp"

using namespace GDSF;
using StringCache = GDSFCache<std::string, std::string, std::hash<std::string>>;
using IntCache = GDSFCache<int, std::string, std::hash<int>>;

class GDSFCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<StringCache>(100);  // 容量为100字节
        int_cache = std::make_unique<IntCache>(50);  // 容量为50字节
    }

    void TearDown() override {
        cache.reset();
        int_cache.reset();
    }

    std::unique_ptr<StringCache> cache;
    std::unique_ptr<IntCache> int_cache;
};

// ============== 基本功能测试 ==============

TEST_F(GDSFCacheTest, BasicPutAndGet) {
    // 测试基本的put和get操作
    EXPECT_TRUE(cache->put("key1", "value1", 10));
    
    auto result = cache->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");
    
    // 测试不存在的key
    auto no_result = cache->get("non_existent");
    EXPECT_FALSE(no_result.has_value());
}

TEST_F(GDSFCacheTest, Contains) {
    cache->put("key1", "value1", 10);
    
    EXPECT_TRUE(cache->contains("key1"));
    EXPECT_FALSE(cache->contains("non_existent"));
}

TEST_F(GDSFCacheTest, SizeAndCapacity) {
    EXPECT_EQ(cache->size(), 0);
    EXPECT_EQ(cache->capacity(), 100);
    EXPECT_EQ(cache->count(), 0);
    
    cache->put("key1", "value1", 10);
    EXPECT_EQ(cache->size(), 10);
    EXPECT_EQ(cache->count(), 1);
    
    cache->put("key2", "value2", 20);
    EXPECT_EQ(cache->size(), 30);
    EXPECT_EQ(cache->count(), 2);
}

TEST_F(GDSFCacheTest, UpdateExistingKey) {
    // 插入初始值
    cache->put("key1", "value1", 10);
    EXPECT_EQ(cache->size(), 10);
    
    // 更新值
    cache->put("key1", "new_value1", 15);
    EXPECT_EQ(cache->size(), 15);  // 大小应该更新
    
    auto result = cache->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "new_value1");
}

// ============== 容量限制和驱逐测试 ==============

TEST_F(GDSFCacheTest, RejectOversizedItem) {
    // 测试放入超过容量的单个项目
    EXPECT_FALSE(cache->put("big_key", "big_value", 150));  // 超过容量100
    EXPECT_EQ(cache->size(), 0);
}

TEST_F(GDSFCacheTest, EvictionBasic) {
    // 填满缓存
    cache->put("key1", "value1", 30);  
    cache->put("key2", "value2", 30);  
    cache->put("key3", "value3", 30);  
    EXPECT_EQ(cache->size(), 90);
    
    // 插入新项目，应该触发驱逐
    cache->put("key4", "value4", 30);  // 总共需要120，超过容量
    
    // 至少有一个项目被驱逐
    EXPECT_LE(cache->size(), 100);
    EXPECT_TRUE(cache->contains("key4"));  // 新插入的应该存在
}

// ============== GDSF算法测试 ==============

TEST_F(GDSFCacheTest, GDSFPriorityCalculation) {
    StringCache test_cache(100, 0.0);  // L值为0，便于测试
    
    // 插入不同大小的项目
    test_cache.put("small", "s", 1);   // 优先级 = 0 + 1/1 = 1.0
    test_cache.put("medium", "m", 5);  // 优先级 = 0 + 1/5 = 0.2
    test_cache.put("large", "l", 10);  // 优先级 = 0 + 1/10 = 0.1
    
    EXPECT_EQ(test_cache.size(), 16);
    
    // 访问medium和large，增加它们的频率
    test_cache.get("medium");  // 频率变为2，优先级 = 0 + 2/5 = 0.4
    test_cache.get("large");   // 频率变为2，优先级 = 0 + 2/10 = 0.2
    test_cache.get("large");   // 频率变为3，优先级 = 0 + 3/10 = 0.3
    
    // 插入大项目，触发驱逐
    test_cache.put("huge", "h", 90);
    
    // small应该被驱逐（最高优先级但访问频率低）
    // 这个测试验证GDSF算法的基本逻辑
    EXPECT_LE(test_cache.count(), 3);
}

TEST_F(GDSFCacheTest, FrequencyUpdateOnGet) {
    StringCache test_cache(50, 0.0);
    
    // 插入两个相同大小的项目
    test_cache.put("key1", "value1", 10);  // 优先级 = 0 + 1/10 = 0.1
    test_cache.put("key2", "value2", 10);  // 优先级 = 0 + 1/10 = 0.1
    
    // 多次访问key1，增加其频率
    for(int i = 0; i < 5; i++) {
        test_cache.get("key1");
    }
    // key1频率现在为6，优先级 = 0 + 6/10 = 0.6
    
    // 插入大项目，应该驱逐key2而不是key1
    test_cache.put("large", "value", 35);
    
    EXPECT_TRUE(test_cache.contains("key1"));  // 高频访问的应该保留
    EXPECT_TRUE(test_cache.contains("large"));
    // key2可能被驱逐
}

// ============== 边界条件测试 ==============

TEST_F(GDSFCacheTest, ZeroCapacity) {
    StringCache zero_cache(0);
    EXPECT_FALSE(zero_cache.put("key", "value", 1));
    EXPECT_EQ(zero_cache.size(), 0);
}

TEST_F(GDSFCacheTest, ZeroSizeItem) {
    EXPECT_TRUE(cache->put("empty", "value", 0));
    EXPECT_EQ(cache->size(), 0);
    
    auto result = cache->get("empty");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value");
}

TEST_F(GDSFCacheTest, EmptyCache) {
    EXPECT_EQ(cache->size(), 0);
    EXPECT_EQ(cache->count(), 0);
    EXPECT_FALSE(cache->contains("any_key"));
    
    auto result = cache->get("any_key");
    EXPECT_FALSE(result.has_value());
}

TEST_F(GDSFCacheTest, FillExactCapacity) {
    cache->put("key", "value", 100);  // 精确填满
    EXPECT_EQ(cache->size(), 100);
    
    // 再插入任何东西都应该触发驱逐或失败
    cache->put("new", "val", 1);
    EXPECT_LE(cache->size(), 100);
}

// ============== 不同键类型测试 ==============

TEST_F(GDSFCacheTest, IntegerKeys) {
    int_cache->put(1, "value1", 10);
    int_cache->put(2, "value2", 10);
    int_cache->put(100, "value100", 10);
    
    EXPECT_EQ(int_cache->count(), 3);
    
    auto result = int_cache->get(100);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value100");
    
    EXPECT_TRUE(int_cache->contains(1));
    EXPECT_FALSE(int_cache->contains(999));
}

// ============== L值影响测试 ==============

TEST_F(GDSFCacheTest, LValueEffect) {
    StringCache high_l_cache(100, 10.0);  // 高L值
    StringCache low_l_cache(100, 0.1);    // 低L值
    
    // 在两个缓存中插入相同数据
    high_l_cache.put("key1", "value1", 10);
    high_l_cache.put("key2", "value2", 20);
    
    low_l_cache.put("key1", "value1", 10);
    low_l_cache.put("key2", "value2", 20);
    
    // 都应该成功插入
    EXPECT_EQ(high_l_cache.count(), 2);
    EXPECT_EQ(low_l_cache.count(), 2);
}

// ============== 性能和压力测试 ==============

TEST_F(GDSFCacheTest, ManyOperations) {
    const int num_operations = 1000;
    StringCache large_cache(10000);  // 大缓存
    
    // 插入大量数据
    for(int i = 0; i < num_operations; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        large_cache.put(key, value, 10);
    }
    
    // 验证部分数据
    auto result = large_cache.get("key500");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value500");
    
    // 缓存应该有数据
    EXPECT_GT(large_cache.count(), 0);
    EXPECT_LE(large_cache.size(), 10000);
}

// ============== 复杂驱逐场景测试 ==============

TEST_F(GDSFCacheTest, ComplexEvictionScenario) {
    StringCache test_cache(100, 0.0);
    
    // 第一批：小而频繁访问的项目
    test_cache.put("freq1", "v1", 5);
    test_cache.put("freq2", "v2", 5);
    for(int i = 0; i < 10; i++) {
        test_cache.get("freq1");
        test_cache.get("freq2");
    }
    
    // 第二批：中等大小，中等频率
    test_cache.put("med1", "v3", 15);
    test_cache.put("med2", "v4", 15);
    for(int i = 0; i < 3; i++) {
        test_cache.get("med1");
        test_cache.get("med2");
    }
    
    // 第三批：大而很少访问
    test_cache.put("large1", "v5", 25);
    test_cache.put("large2", "v6", 25);
    
    // 当前total: 10 + 30 + 50 = 90
    
    // 插入一个大项目，应该优先驱逐大而少访问的项目
    test_cache.put("new_large", "new", 30);
    
    // 高频小项目应该保留
    EXPECT_TRUE(test_cache.contains("freq1"));
    EXPECT_TRUE(test_cache.contains("freq2"));
    
    // 新项目应该成功插入
    EXPECT_TRUE(test_cache.contains("new_large"));
}

// ============== 线程安全测试 ==============

TEST_F(GDSFCacheTest, ConcurrentAccess) {
    StringCache thread_cache(1000);
    const int num_threads = 4;
    const int ops_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // 启动多个线程同时操作缓存
    for(int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for(int i = 0; i < ops_per_thread; i++) {
                std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                std::string value = "value" + std::to_string(i);
                
                if(thread_cache.put(key, value, 5)) {
                    success_count++;
                }
                
                // 随机读取
                if(i % 2 == 0) {
                    thread_cache.get(key);
                }
            }
        });
    }
    
    // 等待所有线程完成
    for(auto& thread : threads) {
        thread.join();
    }
    
    // 验证没有崩溃，且有数据
    EXPECT_GT(success_count.load(), 0);
    EXPECT_LE(thread_cache.size(), 1000);
    std::cout << "Concurrent test: " << success_count.load() 
              << " successful operations, final size: " << thread_cache.size() 
              << ", count: " << thread_cache.count() << std::endl;
}

// ============== 驱逐后L值更新测试 ==============

TEST_F(GDSFCacheTest, LValueUpdateAfterEviction) {
    StringCache test_cache(50, 0.0);  // 初始L=0
    
    // 插入项目
    test_cache.put("item1", "v1", 25);  // 优先级 = 0 + 1/25 = 0.04
    test_cache.put("item2", "v2", 20);  // 优先级 = 0 + 1/20 = 0.05
    
    // 访问item1几次，提高其优先级
    test_cache.get("item1");  // 频率=2，优先级 = 0 + 2/25 = 0.08
    test_cache.get("item1");  // 频率=3，优先级 = 0 + 3/25 = 0.12
    
    EXPECT_EQ(test_cache.size(), 45);
    
    // 插入大项目，触发驱逐
    test_cache.put("big", "big_value", 30);  // 需要驱逐一些项目
    
    // 验证驱逐发生
    EXPECT_LE(test_cache.size(), 50);
    EXPECT_TRUE(test_cache.contains("big"));
    
    // 更频繁访问的item1更可能被保留
    // 这个测试主要验证驱逐逻辑不会崩溃
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
