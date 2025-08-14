/*
@Author: Lzww
@LastEditTime: 2025-8-14 17:35:00
@Description: SRRIP缓存测试套件
@Language: C++17
*/

#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>
#include <chrono>

#include "../include/SRRIP/srrip_cache.h"

using namespace SRRIP;

class SRRIPCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::srand(42);
    }

    uint64_t generateRandomAddress() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis(0, 0xFFFFFFFFFFFFFFFFULL);
        return dis(gen);
    }
};

TEST_F(SRRIPCacheTest, ConstructorTest) {
    EXPECT_NO_THROW({
        SRRIPCache<2> cache(64, 64, 4);
    });

    EXPECT_THROW({
        SRRIPCache<2> cache(0, 64, 4);
    }, std::invalid_argument);

    EXPECT_THROW({
        SRRIPCache<2> cache(64, 63, 4);
    }, std::invalid_argument);
}

TEST_F(SRRIPCacheTest, BasicAccessTest) {
    SRRIPCache<2> cache(64, 64, 4);
    
    EXPECT_FALSE(cache.access(0x1000));
    EXPECT_EQ(cache.getMissCount(), 1);
    
    EXPECT_TRUE(cache.access(0x1000));
    EXPECT_EQ(cache.getHitCount(), 1);
}

TEST_F(SRRIPCacheTest, HitRateTest) {
    SRRIPCache<2> cache(64, 64, 4);
    
    cache.access(0x1000);
    cache.access(0x1000);
    EXPECT_EQ(cache.getHitRate(), 50);
}

TEST_F(SRRIPCacheTest, ReplacementTest) {
    SRRIPCache<2> cache(64, 64, 4);
    
    // 从地址映射测试可知：
    // 地址0x0000 → 组0
    // 地址0x0040 → 组1  
    // 地址0x0080 → 组2
    // 地址0x00C0 → 组3
    
    // 要触发替换，需要使用映射到同一组的地址
    // 由于64字节块大小，同一组内的地址间隔是 64 * 256 = 16384 字节
    
    cache.access(0x0000);      // 组0，way 0
    cache.access(0x4000);      // 组0，way 1  (0x0000 + 16384)
    cache.access(0x8000);      // 组0，way 2  (0x0000 + 32768)
    cache.access(0xC000);      // 组0，way 3  (0x0000 + 49152)
    
    // 现在组0已满，访问组0内的新地址应该触发替换
    EXPECT_FALSE(cache.access(0x14000));  // 组0，需要替换 (0x0000 + 65536)
    EXPECT_EQ(cache.getReplaceCount(), 1);
}

TEST_F(SRRIPCacheTest, ConcurrentAccessTest) {
    SRRIPCache<2> cache(64, 64, 4);
    
    const int num_threads = 4;
    const int ops_per_thread = 1000;
    
    auto worker = [&](int thread_id) {
        for (int i = 0; i < ops_per_thread; ++i) {
            uint64_t addr = (thread_id * ops_per_thread + i) * 64;
            cache.access(addr);
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    uint64_t total = cache.getHitCount() + cache.getMissCount();
    EXPECT_EQ(total, num_threads * ops_per_thread);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
