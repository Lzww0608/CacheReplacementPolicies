/*
@Author: Lzww
@Description: LFU缓存单元测试 (Google Test)
@Language: C++17
*/

#include <gtest/gtest.h>
#include "../include/lfu/lfu_cache.h"
#include "../include/lfu/lfu_shard.h"
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <unordered_set>

class LFUCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前的设置
    }

    void TearDown() override {
        // 每个测试后的清理
    }
};

// ================== LFUShard 基础功能测试 ==================

class LFUShardTest : public ::testing::Test {
protected:
    std::unique_ptr<LFUShard<std::string, int>> shard;

    void SetUp() override {
        // 每个测试都创建新的shard实例，确保统计信息从0开始
        shard = std::make_unique<LFUShard<std::string, int>>(3); // 容量为3
    }
    
    void TearDown() override {
        // 清理shard实例
        shard.reset();
    }
};

TEST_F(LFUShardTest, BasicPutAndGet) {
    int value;
    
    // 测试put和get
    shard->put("key1", 100);
    shard->put("key2", 200);
    shard->put("key3", 300);
    
    EXPECT_TRUE(shard->get("key1", value));
    EXPECT_EQ(value, 100);
    
    EXPECT_TRUE(shard->get("key2", value));
    EXPECT_EQ(value, 200);
    
    EXPECT_TRUE(shard->get("key3", value));
    EXPECT_EQ(value, 300);
    
    // 测试不存在的key
    EXPECT_FALSE(shard->get("nonexistent", value));
}

TEST_F(LFUShardTest, UpdateExistingKey) {
    int value;
    
    // 插入初始值
    shard->put("key1", 100);
    EXPECT_TRUE(shard->get("key1", value));
    EXPECT_EQ(value, 100);
    
    // 更新值
    shard->put("key1", 999);
    EXPECT_TRUE(shard->get("key1", value));
    EXPECT_EQ(value, 999);
}

TEST_F(LFUShardTest, LFUEvictionPolicy) {
    int value;
    
    // 填满缓存
    shard->put("key1", 100);  // 频率: 1
    shard->put("key2", 200);  // 频率: 1  
    shard->put("key3", 300);  // 频率: 1
    
    // 访问key1和key2，增加它们的频率
    shard->get("key1", value);  // key1频率: 2
    shard->get("key1", value);  // key1频率: 3
    shard->get("key2", value);  // key2频率: 2
    
    // key3频率仍为1，应该被淘汰
    shard->put("key4", 400);  // 触发LFU淘汰
    
    // key3应该被淘汰
    EXPECT_FALSE(shard->get("key3", value));
    
    // 其他key应该仍然存在
    EXPECT_TRUE(shard->get("key1", value));
    EXPECT_EQ(value, 100);
    EXPECT_TRUE(shard->get("key2", value));
    EXPECT_EQ(value, 200);
    EXPECT_TRUE(shard->get("key4", value));
    EXPECT_EQ(value, 400);
}

TEST_F(LFUShardTest, TTLExpiration) {
    int value;
    
    // 插入会过期的key
    shard->put("short_ttl", 100, 100);  // 100ms过期
    shard->put("long_ttl", 200, 5000);  // 5秒过期
    
    // 立即访问应该成功
    EXPECT_TRUE(shard->get("short_ttl", value));
    EXPECT_EQ(value, 100);
    EXPECT_TRUE(shard->get("long_ttl", value));
    EXPECT_EQ(value, 200);
    
    // 等待短TTL过期
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // 短TTL应该过期，长TTL仍然有效
    EXPECT_FALSE(shard->get("short_ttl", value));
    EXPECT_TRUE(shard->get("long_ttl", value));
    EXPECT_EQ(value, 200);
}

TEST_F(LFUShardTest, ManualCleanupExpired) {
    // 插入多个会过期的key
    for (int i = 0; i < 3; ++i) {
        shard->put("temp" + std::to_string(i), i, 50);  // 50ms过期
    }
    
    // 等待过期
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 手动清理
    shard->cleanupExpired();
    
    // 检查统计信息
    auto stats = shard->getStats();
    EXPECT_GT(stats.expired_count, 0);
}

TEST_F(LFUShardTest, RemoveKey) {
    int value;
    
    shard->put("key1", 100);
    shard->put("key2", 200);
    
    // 删除存在的key
    EXPECT_TRUE(shard->remove("key1"));
    EXPECT_FALSE(shard->get("key1", value));
    
    // 删除不存在的key
    EXPECT_FALSE(shard->remove("nonexistent"));
    
    // key2应该仍然存在
    EXPECT_TRUE(shard->get("key2", value));
    EXPECT_EQ(value, 200);
}

TEST_F(LFUShardTest, Statistics) {
    int value;
    
    // 执行一些操作
    shard->put("key1", 100);
    shard->put("key2", 200);
    
    shard->get("key1", value);  // 命中
    shard->get("key1", value);  // 命中
    shard->get("nonexistent", value);  // 未命中
    
    auto stats = shard->getStats();
    EXPECT_EQ(stats.hits, 2);
    EXPECT_EQ(stats.misses, 1);
}

// ================== LFUCache 完整功能测试 ==================

TEST_F(LFUCacheTest, BasicFunctionality) {
    LFUCache<std::string, int> cache(100);  // 使用默认分片
    int value;
    
    // 基本put/get操作
    cache.put("key1", 100);
    cache.put("key2", 200);
    cache.put("key3", 300);
    
    EXPECT_TRUE(cache.get("key1", value));
    EXPECT_EQ(value, 100);
    EXPECT_TRUE(cache.get("key2", value));
    EXPECT_EQ(value, 200);
    EXPECT_TRUE(cache.get("key3", value));
    EXPECT_EQ(value, 300);
    
    EXPECT_FALSE(cache.get("nonexistent", value));
}

TEST_F(LFUCacheTest, MultiShardFunctionality) {
    LFUCache<std::string, int> cache(64, 4);  // 64容量，4分片
    int value;
    
    // 插入多个key，它们会分布到不同分片
    for (int i = 0; i < 20; ++i) {
        cache.put("key" + std::to_string(i), i);
    }
    
    // 验证所有key都能正确获取
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(cache.get("key" + std::to_string(i), value));
        EXPECT_EQ(value, i);
    }
}

TEST_F(LFUCacheTest, TTLFunctionality) {
    LFUCache<std::string, int> cache(100);
    int value;
    
    // 测试TTL功能
    cache.put("short", 100, 100);   // 100ms过期
    cache.put("long", 200, 5000);   // 5秒过期
    
    EXPECT_TRUE(cache.get("short", value));
    EXPECT_TRUE(cache.get("long", value));
    
    // 等待短TTL过期
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    EXPECT_FALSE(cache.get("short", value));
    EXPECT_TRUE(cache.get("long", value));
}

TEST_F(LFUCacheTest, TTLControlInterface) {
    LFUCache<std::string, int> cache(100);
    
    // 测试TTL控制接口
    cache.enableTTL(true);
    cache.put("key1", 100, 50);  // 短过期时间
    
    cache.disableTTL();
    cache.put("key2", 200, 50);  // 短过期时间，但TTL被禁用
    
    // 启用TTL的控制接口不会影响已经设置的过期检查
    // 主要影响后台清理线程
}

TEST_F(LFUCacheTest, ConcurrentAccess) {
    LFUCache<std::string, int> cache(1000, 8);  // 8分片支持高并发
    
    const int num_threads = 4;
    const int ops_per_thread = 500;
    std::vector<std::thread> threads;
    
    // 启动多个线程并发读写
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i);
                cache.put(key, i);
                
                int value;
                cache.get(key, value);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证统计信息
    auto stats = cache.getStats();
    EXPECT_GT(stats.total_hits, 0);
    EXPECT_LE(stats.hit_rate(), 1.0);
    EXPECT_GE(stats.hit_rate(), 0.0);
}

TEST_F(LFUCacheTest, LargeScaleOperations) {
    LFUCache<int, std::string> cache(1000);
    
    // 大规模操作测试
    const int num_keys = 2000;  // 超过容量，测试淘汰
    
    for (int i = 0; i < num_keys; ++i) {
        cache.put(i, "value" + std::to_string(i));
    }
    
    // 统计信息应该合理
    auto stats = cache.getStats();
    EXPECT_GT(stats.total_evictions, 0);  // 应该有淘汰发生
}

TEST_F(LFUCacheTest, EdgeCases) {
    // 测试边界条件
    LFUCache<std::string, int> cache(1);  // 最小容量
    int value;
    
    cache.put("key1", 100);
    EXPECT_TRUE(cache.get("key1", value));
    EXPECT_EQ(value, 100);
    
    // 添加第二个key应该淘汰第一个
    cache.put("key2", 200);
    EXPECT_FALSE(cache.get("key1", value));
    EXPECT_TRUE(cache.get("key2", value));
    EXPECT_EQ(value, 200);
}

TEST_F(LFUCacheTest, RemoveOperation) {
    LFUCache<std::string, int> cache(100);
    int value;
    
    cache.put("key1", 100);
    cache.put("key2", 200);
    
    EXPECT_TRUE(cache.remove("key1"));
    EXPECT_FALSE(cache.get("key1", value));
    EXPECT_TRUE(cache.get("key2", value));
    
    EXPECT_FALSE(cache.remove("nonexistent"));
}

// ================== 性能和压力测试 ==================

TEST_F(LFUCacheTest, PerformanceTest) {
    LFUCache<int, std::string> cache(10000, 16);  // 大容量，多分片
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 写入测试
    for (int i = 0; i < 10000; ++i) {
        cache.put(i, "value" + std::to_string(i));
    }
    
    // 读取测试
    std::string value;
    for (int i = 0; i < 10000; ++i) {
        cache.get(i, value);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Performance test: 20000 operations in " << duration.count() << "ms" << std::endl;
    
    // 性能应该在合理范围内（这个阈值可以根据硬件调整）
    EXPECT_LT(duration.count(), 5000);  // 应该在5秒内完成
}

// ================== 主函数 ==================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // 设置测试输出格式
    ::testing::FLAGS_gtest_color = "yes";
    
    std::cout << "=== LFU Cache Unit Tests ===" << std::endl;
    
    int result = RUN_ALL_TESTS();
    
    if (result == 0) {
        std::cout << "\n✅ All LFU Cache tests passed!" << std::endl;
    } else {
        std::cout << "\n❌ Some LFU Cache tests failed!" << std::endl;
    }
    
    return result;
} 