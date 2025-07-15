/*
@Author: Lzww
@Description: ARC缓存测试程序
@Language: C++17
*/

#include "../include/arc/arc_cache.h"
#include <iostream>
#include <string>
#include <cassert>
#include <chrono>

void testBasicFunctionality() {
    std::cout << "=== 测试基本功能 ===" << std::endl;
    
    ARCCache<std::string, int> cache(5, 10, 1);  // p=5, c=10, 1分片
    
    // 测试put和get
    cache.put("key1", 100);
    cache.put("key2", 200);
    cache.put("key3", 300);
    
    int value;
    assert(cache.get("key1", value) == true && value == 100);
    assert(cache.get("key2", value) == true && value == 200);
    assert(cache.get("key3", value) == true && value == 300);
    
    std::cout << "✓ 基本功能测试通过" << std::endl;
}

void testARCBehavior() {
    std::cout << "=== 测试ARC算法行为 ===" << std::endl;
    
    ARCCache<std::string, int> cache(3, 6, 1);  // p=3, c=6, 1分片
    
    // 填充T1
    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);
    
    // 访问一次，应该从T1移动到T2
    int value;
    assert(cache.get("a", value) == true && value == 1);
    
    // 添加更多数据
    cache.put("d", 4);
    cache.put("e", 5);
    cache.put("f", 6);
    
    // 验证包含性
    assert(cache.contains("a") == true);  // 应该在T2中
    
    std::cout << "✓ ARC算法行为测试通过" << std::endl;
}

void testCapacityLimits() {
    std::cout << "=== 测试容量限制 ===" << std::endl;
    
    ARCCache<std::string, int> cache(2, 4, 1);  // p=2, c=4, 1分片
    
    // 添加超过容量的数据
    for (int i = 0; i < 10; ++i) {
        cache.put("key" + std::to_string(i), i);
    }
    
    // 验证统计信息
    auto stats = cache.getStats();
    std::cout << "T1 大小: " << stats.t1_size << std::endl;
    std::cout << "T2 大小: " << stats.t2_size << std::endl;
    std::cout << "B1 大小: " << stats.b1_size << std::endl;
    std::cout << "B2 大小: " << stats.b2_size << std::endl;
    std::cout << "目标P: " << stats.target_p << std::endl;
    
    // 验证总容量不超过限制
    assert(stats.t1_size + stats.t2_size <= stats.total_capacity);
    
    std::cout << "✓ 容量限制测试通过" << std::endl;
}

void testRemoveOperation() {
    std::cout << "=== 测试删除操作 ===" << std::endl;
    
    ARCCache<std::string, int> cache(3, 6, 1);
    
    cache.put("key1", 100);
    cache.put("key2", 200);
    
    assert(cache.contains("key1") == true);
    assert(cache.remove("key1") == true);
    assert(cache.contains("key1") == false);
    assert(cache.remove("key1") == false);  // 二次删除应该返回false
    
    std::cout << "✓ 删除操作测试通过" << std::endl;
}

void testParameterValidation() {
    std::cout << "=== 测试参数验证 ===" << std::endl;
    
    try {
        ARCCache<std::string, int> cache(0, 0, 1);  // 无效容量
        assert(false && "应该抛出异常");
    } catch (const std::invalid_argument& e) {
        std::cout << "✓ 捕获到期望的异常: " << e.what() << std::endl;
    }
    
    try {
        ARCCache<std::string, int> cache(10, 5, 1);  // p > c
        assert(false && "应该抛出异常");
    } catch (const std::invalid_argument& e) {
        std::cout << "✓ 捕获到期望的异常: " << e.what() << std::endl;
    }
    
    std::cout << "✓ 参数验证测试通过" << std::endl;
}

void testMultiShardBehavior() {
    std::cout << "=== 测试多分片行为 ===" << std::endl;
    
    ARCCache<std::string, int> cache(8, 16, 4);  // p=8, c=16, 4分片
    
    // 添加大量数据
    for (int i = 0; i < 50; ++i) {
        cache.put("key" + std::to_string(i), i);
    }
    
    // 验证一些数据仍然可访问
    int value;
    int found_count = 0;
    for (int i = 0; i < 50; ++i) {
        if (cache.get("key" + std::to_string(i), value)) {
            found_count++;
        }
    }
    
    std::cout << "在50个键中找到: " << found_count << "个" << std::endl;
    assert(found_count > 0 && found_count <= 16);  // 应该在合理范围内
    
    std::cout << "✓ 多分片行为测试通过" << std::endl;
}

void performanceBenchmark() {
    std::cout << "=== 性能基准测试 ===" << std::endl;
    
    ARCCache<std::string, int> cache(512, 1024, 8);
    
    const uint64_t num_operations = 100000;
    auto start = std::chrono::high_resolution_clock::now();
    
    // 写入测试
    for (int i = 0; i < num_operations; ++i) {
        cache.put("key" + std::to_string(i), i);
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    // 读取测试 - 只读取后面的数据（应该还在缓存中）
    int value;
    int hit_count = 0;
    int test_start = num_operations - 2000;  // 测试最后2000个数据
    int test_count = 2000;
    
    for (int i = test_start; i < num_operations; ++i) {
        if (cache.get("key" + std::to_string(i), value)) {
            hit_count++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto write_time = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto read_time = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
    
    std::cout << "写入性能: " << num_operations * 1000000 / write_time.count() << " ops/sec" << std::endl;
    std::cout << "读取性能: " << test_count * 1000000 / read_time.count() << " ops/sec" << std::endl;
    std::cout << "命中率: " << (hit_count * 100.0 / test_count) << "%" << std::endl;
    std::cout << "测试说明: 在" << test_count << "个最新键中测试命中率" << std::endl;
    
    std::cout << "✓ 性能基准测试完成" << std::endl;
}

void realisticWorkloadTest() {
    std::cout << "=== 真实工作负载测试 ===" << std::endl;
    
    ARCCache<std::string, int> cache(256, 512, 4);
    
    // 模拟真实访问模式：80/20原则
    const int total_keys = 1000;
    const int hot_keys = 200;  // 20%的热点数据
    const int operations = 50000;
    
    // 初始化数据
    for (int i = 0; i < total_keys; ++i) {
        cache.put("key" + std::to_string(i), i);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int hit_count = 0;
    int value;
    
    // 模拟80/20访问模式
    for (int i = 0; i < operations; ++i) {
        std::string key;
        if (i % 5 < 4) {  // 80%的请求访问热点数据
            key = "key" + std::to_string(i % hot_keys);
        } else {  // 20%的请求访问冷数据
            key = "key" + std::to_string(hot_keys + (i % (total_keys - hot_keys)));
        }
        
        if (cache.get(key, value)) {
            hit_count++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "总操作数: " << operations << std::endl;
    std::cout << "命中次数: " << hit_count << std::endl;
    std::cout << "命中率: " << (hit_count * 100.0 / operations) << "%" << std::endl;
    std::cout << "查询性能: " << operations * 1000000 / duration.count() << " ops/sec" << std::endl;
    
    auto stats = cache.getStats();
    std::cout << "缓存状态 - T1:" << stats.t1_size << ", T2:" << stats.t2_size 
              << ", B1:" << stats.b1_size << ", B2:" << stats.b2_size << std::endl;
    
    std::cout << "✓ 真实工作负载测试完成" << std::endl;
}

int main() {
    std::cout << "开始ARC缓存测试..." << std::endl;
    
    try {
        testBasicFunctionality();
        testARCBehavior();
        testCapacityLimits();
        testRemoveOperation();
        testParameterValidation();
        testMultiShardBehavior();
        performanceBenchmark();
        realisticWorkloadTest();
        
        std::cout << "\n🎉 所有测试通过！ARC缓存修复成功！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 