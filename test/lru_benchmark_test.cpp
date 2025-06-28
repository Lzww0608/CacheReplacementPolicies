/*
@Author: Lzww
@LastEditTime: 2025-6-28 21:00:49
@Description: LRU缓存性能基准测试
@Language: C++17
*/

#include "../include/lru/lru_cache.h"
#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <random>
#include <vector>
#include <atomic>
#include <algorithm>
#include <numeric>

class BenchmarkTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        return duration.count() / 1000.0; // 返回毫秒
    }
};

void benchmarkReadHeavy() {
    std::cout << "=== 读密集型负载测试 ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(10000, 8);
    
    // 预填充缓存
    for (int i = 0; i < 5000; ++i) {
        cache.put("key" + std::to_string(i), i, 300000); // 5分钟过期
    }
    
    const int num_threads = 8;
    const int ops_per_thread = 50000;
    std::atomic<long long> total_ops{0};
    
    BenchmarkTimer timer;
    timer.start();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, &total_ops, ops_per_thread, t]() {
            std::random_device rd;
            std::mt19937 gen(rd() + t);
            std::uniform_int_distribution<> dis(0, 4999);
            
            int value;
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key" + std::to_string(dis(gen));
                cache.get(key, value);
                total_ops.fetch_add(1);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    double elapsed = timer.stop();
    double ops_per_sec = (total_ops.load() * 1000.0) / elapsed;
    
    std::cout << "读密集型测试结果:" << std::endl;
    std::cout << "  总操作数: " << total_ops.load() << std::endl;
    std::cout << "  耗时: " << elapsed << " ms" << std::endl;
    std::cout << "  吞吐量: " << static_cast<long long>(ops_per_sec) << " ops/sec" << std::endl;
    
    auto stats = cache.getStats();
    std::cout << "  命中率: " << stats.hit_rate() * 100 << "%" << std::endl;
    std::cout << std::endl;
}

void benchmarkMixedWorkload() {
    std::cout << "=== 混合负载测试 (70%读 30%写) ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(10000, 8);
    
    const int num_threads = 8;
    const int ops_per_thread = 25000;
    std::atomic<long long> total_ops{0};
    std::atomic<long long> read_ops{0};
    std::atomic<long long> write_ops{0};
    
    BenchmarkTimer timer;
    timer.start();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, &total_ops, &read_ops, &write_ops, ops_per_thread, t]() {
            std::random_device rd;
            std::mt19937 gen(rd() + t);
            std::uniform_int_distribution<> key_dis(0, 9999);
            std::uniform_int_distribution<> op_dis(1, 100);
            
            int value;
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key" + std::to_string(key_dis(gen));
                
                if (op_dis(gen) <= 70) {
                    // 70% 读操作
                    cache.get(key, value);
                    read_ops.fetch_add(1);
                } else {
                    // 30% 写操作
                    cache.put(key, key_dis(gen), 300000);
                    write_ops.fetch_add(1);
                }
                total_ops.fetch_add(1);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    double elapsed = timer.stop();
    double ops_per_sec = (total_ops.load() * 1000.0) / elapsed;
    
    std::cout << "混合负载测试结果:" << std::endl;
    std::cout << "  总操作数: " << total_ops.load() << std::endl;
    std::cout << "  读操作: " << read_ops.load() << " (" 
              << (read_ops.load() * 100.0 / total_ops.load()) << "%)" << std::endl;
    std::cout << "  写操作: " << write_ops.load() << " (" 
              << (write_ops.load() * 100.0 / total_ops.load()) << "%)" << std::endl;
    std::cout << "  耗时: " << elapsed << " ms" << std::endl;
    std::cout << "  吞吐量: " << static_cast<long long>(ops_per_sec) << " ops/sec" << std::endl;
    
    auto stats = cache.getStats();
    std::cout << "  命中率: " << stats.hit_rate() * 100 << "%" << std::endl;
    std::cout << std::endl;
}

void benchmarkConcurrentReads() {
    std::cout << "=== 并发读取压力测试 ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(1000, 16);
    
    // 预填充缓存
    for (int i = 0; i < 1000; ++i) {
        cache.put("hotkey" + std::to_string(i), i, 600000); // 10分钟过期
    }
    
    const int num_threads = 16;  // 更多线程测试并发读取
    const int ops_per_thread = 100000;
    std::atomic<long long> total_ops{0};
    
    BenchmarkTimer timer;
    timer.start();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache, &total_ops, ops_per_thread, t]() {
            std::random_device rd;
            std::mt19937 gen(rd() + t);
            std::uniform_int_distribution<> dis(0, 999);
            
            int value;
            for (int i = 0; i < ops_per_thread; ++i) {
                // 集中访问热点数据
                std::string key = "hotkey" + std::to_string(dis(gen));
                cache.get(key, value);
                total_ops.fetch_add(1);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    double elapsed = timer.stop();
    double ops_per_sec = (total_ops.load() * 1000.0) / elapsed;
    
    std::cout << "并发读取测试结果:" << std::endl;
    std::cout << "  线程数: " << num_threads << std::endl;
    std::cout << "  总操作数: " << total_ops.load() << std::endl;
    std::cout << "  耗时: " << elapsed << " ms" << std::endl;
    std::cout << "  吞吐量: " << static_cast<long long>(ops_per_sec) << " ops/sec" << std::endl;
    std::cout << "  平均延迟: " << (elapsed * 1000.0 / total_ops.load()) << " μs/op" << std::endl;
    
    auto stats = cache.getStats();
    std::cout << "  命中率: " << stats.hit_rate() * 100 << "%" << std::endl;
    std::cout << std::endl;
}

void benchmarkLatency() {
    std::cout << "=== 延迟测试 ===" << std::endl;
    
    LRUCache<std::string, int, std::hash<std::string>> cache(1000, 4);
    
    // 预填充
    for (int i = 0; i < 500; ++i) {
        cache.put("key" + std::to_string(i), i, 300000);
    }
    
    const int test_count = 10000;
    std::vector<double> latencies;
    latencies.reserve(test_count);
    
    // 单线程延迟测试
    for (int i = 0; i < test_count; ++i) {
        std::string key = "key" + std::to_string(i % 500);
        
        auto start = std::chrono::high_resolution_clock::now();
        int value;
        cache.get(key, value);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        latencies.push_back(duration.count() / 1000.0); // 转换为微秒
    }
    
    // 计算延迟统计
    std::sort(latencies.begin(), latencies.end());
    double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    double p50 = latencies[latencies.size() * 0.5];
    double p95 = latencies[latencies.size() * 0.95];
    double p99 = latencies[latencies.size() * 0.99];
    
    std::cout << "延迟统计 (单线程):" << std::endl;
    std::cout << "  平均延迟: " << avg << " μs" << std::endl;
    std::cout << "  P50延迟: " << p50 << " μs" << std::endl;
    std::cout << "  P95延迟: " << p95 << " μs" << std::endl;
    std::cout << "  P99延迟: " << p99 << " μs" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "LRU缓存性能基准测试 (使用 std::shared_mutex)" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << std::endl;
    
    try {
        benchmarkLatency();
        benchmarkReadHeavy();
        benchmarkConcurrentReads();
        benchmarkMixedWorkload();
        
        std::cout << "✅ 所有性能测试完成！" << std::endl;
        std::cout << std::endl;
        std::cout << "性能优化说明:" << std::endl;
        std::cout << "- 使用 std::shared_mutex 实现读写分离锁" << std::endl;
        std::cout << "- 读操作使用共享锁，允许多线程并发读取" << std::endl;
        std::cout << "- 写操作使用独占锁，确保数据一致性" << std::endl;
        std::cout << "- 分片架构减少锁争用，提高并发性能" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 性能测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 