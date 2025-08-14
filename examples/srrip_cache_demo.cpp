/*
@Author: Lzww
@LastEditTime: 2025-8-14 17:40:00
@Description: SRRIP缓存演示程序
@Language: C++17
*/

#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <chrono>

#include "../include/SRRIP/srrip_cache.h"

using namespace SRRIP;

void printStats(const SRRIPCache<2>& cache) {
    std::cout << "=== 缓存统计信息 ===" << std::endl;
    std::cout << "命中次数: " << cache.getHitCount() << std::endl;
    std::cout << "未命中次数: " << cache.getMissCount() << std::endl;
    std::cout << "替换次数: " << cache.getReplaceCount() << std::endl;
    std::cout << "命中率: " << cache.getHitRate() << "%" << std::endl;
    std::cout << std::endl;
}

void demonstrateBasicOperations() {
    std::cout << "=== 基础操作演示 ===" << std::endl;
    
    // 创建64KB缓存，64字节块，4路组相联
    SRRIPCache<2> cache(64, 64, 4);
    
    std::cout << "访问地址 0x1000..." << std::endl;
    bool hit = cache.access(0x1000);
    std::cout << "结果: " << (hit ? "命中" : "未命中") << std::endl;
    
    std::cout << "再次访问地址 0x1000..." << std::endl;
    hit = cache.access(0x1000);
    std::cout << "结果: " << (hit ? "命中" : "未命中") << std::endl;
    
    printStats(cache);
}

void demonstrateReplacement() {
    std::cout << "=== 替换策略演示 ===" << std::endl;
    
    SRRIPCache<2> cache(64, 64, 4);
    
    // 填充所有way
    std::cout << "填充所有way..." << std::endl;
    for (int i = 0; i < 4; ++i) {
        uint64_t addr = 0x1000 + i * 0x1000;
        cache.access(addr);
        std::cout << "访问 0x" << std::hex << addr << std::dec << std::endl;
    }
    
    std::cout << "访问新地址触发替换..." << std::endl;
    cache.access(0x5000);
    
    printStats(cache);
}

void demonstrateSequentialAccess() {
    std::cout << "=== 顺序访问演示 ===" << std::endl;
    
    SRRIPCache<2> cache(64, 64, 4);
    
    std::cout << "顺序访问100个地址..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        cache.access(i * 64);
    }
    
    std::cout << "再次顺序访问..." << std::endl;
    size_t hits = 0;
    for (int i = 0; i < 100; ++i) {
        if (cache.access(i * 64)) {
            hits++;
        }
    }
    
    std::cout << "第二次访问命中数: " << hits << "/100" << std::endl;
    printStats(cache);
}

void demonstrateRandomAccess() {
    std::cout << "=== 随机访问演示 ===" << std::endl;
    
    SRRIPCache<2> cache(64, 64, 4);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(0, 0x100000);
    
    std::cout << "随机访问1000个地址..." << std::endl;
    for (int i = 0; i < 1000; ++i) {
        cache.access(dis(gen));
    }
    
    std::cout << "再次随机访问..." << std::endl;
    size_t hits = 0;
    for (int i = 0; i < 1000; ++i) {
        if (cache.access(dis(gen))) {
            hits++;
        }
    }
    
    std::cout << "第二次访问命中数: " << hits << "/1000" << std::endl;
    printStats(cache);
}

void performanceBenchmark() {
    std::cout << "=== 性能基准测试 ===" << std::endl;
    
    SRRIPCache<2> cache(64, 64, 4);
    
    const int num_operations = 100000;
    std::vector<uint64_t> addresses;
    addresses.reserve(num_operations);
    
    // 生成地址序列
    for (int i = 0; i < num_operations; ++i) {
        addresses.push_back(i * 64);
    }
    
    // 第一次访问
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        cache.access(addresses[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double ops_per_sec = (double)num_operations / duration.count() * 1000000;
    
    std::cout << "第一次访问: " << num_operations << " 次操作" << std::endl;
    std::cout << "耗时: " << duration.count() << " 微秒" << std::endl;
    std::cout << "性能: " << std::fixed << std::setprecision(2) << ops_per_sec << " ops/sec" << std::endl;
    
    // 第二次访问（应该有很多命中）
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        cache.access(addresses[i]);
    }
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    ops_per_sec = (double)num_operations / duration.count() * 1000000;
    
    std::cout << "第二次访问: " << num_operations << " 次操作" << std::endl;
    std::cout << "耗时: " << duration.count() << " 微秒" << std::endl;
    std::cout << "性能: " << std::fixed << std::setprecision(2) << ops_per_sec << " ops/sec" << std::endl;
    
    printStats(cache);
}

int main() {
    std::cout << "SRRIP缓存演示程序" << std::endl;
    std::cout << "==================" << std::endl << std::endl;
    
    try {
        demonstrateBasicOperations();
        demonstrateReplacement();
        demonstrateSequentialAccess();
        demonstrateRandomAccess();
        performanceBenchmark();
        
        std::cout << "演示完成！" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
