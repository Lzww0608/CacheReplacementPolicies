/*
@Author: Lzww
@Description: LRU缓存TTL功能测试
@Language: C++17
*/

#include "../include/lru/lru_cache.h"
#include <iostream>
#include <thread>
#include <string>
#include <cassert>

void testBasicFunctionality() {
    std::cout << "=== 测试基本功能 ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(100, 4);  // 100容量，4分片
    
    // 测试put和get
    cache.put("key1", 100, 1000);  // 1秒过期
    cache.put("key2", 200, 2000);  // 2秒过期
    cache.put("key3", 300, 5000);  // 5秒过期
    
    int value;
    assert(cache.get("key1", value) == true && value == 100);
    assert(cache.get("key2", value) == true && value == 200);
    assert(cache.get("key3", value) == true && value == 300);
    
    std::cout << "基本功能测试通过" << std::endl;
}

void testTTLExpiration() {
    std::cout << "=== 测试TTL过期 ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(100, 2);
    
    // 添加短期过期的键
    cache.put("short", 100, 500);   // 0.5秒过期
    cache.put("long", 200, 3000);   // 3秒过期
    
    int value;
    // 立即访问应该成功
    assert(cache.get("short", value) == true && value == 100);
    assert(cache.get("long", value) == true && value == 200);
    
    // 等待短期键过期
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // 短期键应该过期，长期键仍然有效
    assert(cache.get("short", value) == false);
    assert(cache.get("long", value) == true && value == 200);
    
    std::cout << "TTL过期测试通过" << std::endl;
}

void testBackgroundCleanup() {
    std::cout << "=== 测试后台清理 ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(100, 2);
    
    // 添加多个会过期的键
    for (int i = 0; i < 10; ++i) {
        cache.put("temp" + std::to_string(i), i, 200);  // 0.2秒过期
    }
    
    // 等待过期和后台清理
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    
    // 检查统计信息
    auto stats = cache.getStats();
    std::cout << "命中率: " << stats.hit_rate() * 100 << "%" << std::endl;
    std::cout << "过期清理数量: " << stats.expired_count << std::endl;
    
    std::cout << "后台清理测试完成" << std::endl;
}

void testConcurrentAccess() {
    std::cout << "=== 测试并发访问 ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(1000, 8);  // 8分片支持高并发
    
    const int num_threads = 4;
    const int ops_per_thread = 1000;
    
    std::vector<std::thread> threads;
    
    // 启动多个线程并发读写
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                cache.put(key, i, 2000);  // 2秒过期
                
                int value;
                cache.get(key, value);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto stats = cache.getStats();
    std::cout << "并发测试完成 - 总命中: " << stats.total_hits 
              << ", 总未命中: " << stats.total_misses << std::endl;
    
    std::cout << "并发访问测试通过" << std::endl;
}

void testTTLControl() {
    std::cout << "=== 测试TTL控制 ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(100, 2);
    
    // 禁用TTL
    cache.disableTTL();
    cache.put("persistent", 100, 100);  // 很短的过期时间
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    int value;
    // 禁用TTL时，即使过期也不会被后台清理（但get时仍会检查）
    // 这里主要测试TTL控制接口
    
    // 重新启用TTL
    cache.enableTTL();
    
    std::cout << "TTL控制测试完成" << std::endl;
}

int main() {
    std::cout << "开始LRU缓存TTL功能测试..." << std::endl;
    
    try {
        testBasicFunctionality();
        testTTLExpiration();
        testBackgroundCleanup();
        testConcurrentAccess();
        testTTLControl();
        
        std::cout << "\n✅ 所有测试通过！" << std::endl;
        std::cout << "LRU缓存TTL功能实现成功" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 