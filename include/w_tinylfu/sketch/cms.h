/*
@Author: Lzww
@LastEditTime: 2025-7-22 21:34:40
@Description: Count-Min Sketch for W-TinyLFU frequency estimation
@Language: C++17
*/

#ifndef W_TINYLFU_SKETCH_CMS_H_
#define W_TINYLFU_SKETCH_CMS_H_

#include <cstdint>
#include <cstring>
#include <memory>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cassert>
#include <string>
#include <mutex>
#include <shared_mutex>

namespace CPR {
namespace W_Tinylfu {

//===================================================================
// CMS Configuration
//===================================================================

struct CMSConfig {
    size_t width;              // 计数器矩阵宽度（列数）
    size_t depth;              // 计数器矩阵深度（行数，哈希函数数量）
    uint8_t bits_per_counter;  // 每个计数器的位数（2-8位）
    uint32_t decay_threshold;  // 衰减触发阈值（默认15）
    
    CMSConfig(size_t w = 16384, size_t d = 4, uint8_t bpc = 4, uint32_t dt = 15)
        : width(w), depth(d), bits_per_counter(bpc), decay_threshold(dt) {
        assert(width > 0 && depth > 0);
        assert(bits_per_counter >= 2 && bits_per_counter <= 8);
        assert(decay_threshold > 0);
    }
    
    bool isValid() const {
        return width > 0 && depth > 0 && 
               bits_per_counter >= 2 && bits_per_counter <= 8 &&
               decay_threshold > 0;
    }
    
    // 计算内存使用量（字节）
    size_t memoryUsage() const {
        size_t total_bits = width * depth * bits_per_counter;
        return (total_bits + 7) / 8;
    }
    
    // 计算最大计数器值
    uint32_t maxCount() const {
        return (1U << bits_per_counter) - 1;
    }
};

//===================================================================
// Hash Functions
//===================================================================

class CMSHash {
public:
    // MurmurHash3 32-bit
    static uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);
    
    // FNV-1a 32-bit
    static uint32_t fnv1a_32(const void* key, size_t len, uint32_t seed);
    
    // 双重哈希：h1 + i * h2
    static std::vector<uint32_t> doubleHash(const void* key, size_t len, 
                                           size_t count, uint32_t seed);
    
private:
    static uint32_t rotl32(uint32_t x, int8_t r);
    static uint32_t fmix32(uint32_t h);
};

//===================================================================
// Count-Min Sketch Implementation
//===================================================================

class CountMinSketch {
public:
    // 构造函数
    explicit CountMinSketch(const CMSConfig& config);
    
    // 析构函数
    ~CountMinSketch();
    
    // 禁止拷贝，允许移动
    CountMinSketch(const CountMinSketch&) = delete;
    CountMinSketch& operator=(const CountMinSketch&) = delete;
    CountMinSketch(CountMinSketch&&) = default;
    CountMinSketch& operator=(CountMinSketch&&) = default;
    
    // 核心操作
    void increment(const void* key, size_t key_len);
    void increment(const std::string& key);
    
    template<typename T>
    void increment(const T& value) {
        increment(&value, sizeof(T));
    }
    
    uint32_t estimate(const void* key, size_t key_len) const;
    uint32_t estimate(const std::string& key) const;
    
    template<typename T>
    uint32_t estimate(const T& value) const {
        return estimate(&value, sizeof(T));
    }
    
    // 频率衰减（核心特性）
    void decay();
    
    // 重置所有计数器
    void reset();
    
    // 清空所有计数器
    void clear();
    
    // 统计信息
    struct Stats {
        uint64_t total_increments = 0;
        uint64_t total_decays = 0;
        uint64_t current_access_count = 0;
        uint32_t max_counter_value = 0;
        size_t memory_usage = 0;
    };
    
    Stats getStats() const;
    
    // 配置信息
    const CMSConfig& getConfig() const { return config_; }
    size_t width() const { return config_.width; }
    size_t depth() const { return config_.depth; }
    uint8_t bitsPerCounter() const { return config_.bits_per_counter; }
    uint32_t maxCount() const { return config_.maxCount(); }
    size_t memoryUsage() const { return config_.memoryUsage(); }

private:
    CMSConfig config_;
    std::unique_ptr<uint8_t[]> counter_array_;
    std::vector<uint32_t> seeds_;
    
    // 线程安全
    mutable std::shared_mutex rw_mutex_;
    std::atomic<uint64_t> access_count_;
    std::atomic<uint64_t> total_increments_;
    std::atomic<uint64_t> total_decays_;
    
    // 计数器操作
    uint32_t getCounter(size_t row, size_t col) const;
    void setCounter(size_t row, size_t col, uint32_t value);
    bool incrementCounter(size_t row, size_t col);
    
    // 辅助函数
    size_t getByteIndex(size_t row, size_t col) const;
    size_t getBitOffset(size_t row, size_t col) const;
    uint32_t getCounterMask() const;
    
    // 哈希函数
    std::vector<uint32_t> generateHashes(const void* key, size_t key_len) const;
    
    // 初始化种子
    void initializeSeeds();
};

//===================================================================
// CMS Factory
//===================================================================

class CMSFactory {
public:
    // 创建标准配置的CMS
    static std::unique_ptr<CountMinSketch> createStandard(size_t sample_size);
    
    // 创建自定义配置的CMS
    static std::unique_ptr<CountMinSketch> createCustom(const CMSConfig& config);
    
    // 创建优化的CMS（根据样本大小自动计算参数）
    static std::unique_ptr<CountMinSketch> createOptimized(size_t sample_size, 
                                                          double error_rate = 0.01);
    
    // 计算最优参数
    static CMSConfig calculateOptimalParams(size_t sample_size, 
                                           double error_rate = 0.01,
                                           uint8_t bits_per_counter = 4);
    
    // 创建W-TinyLFU专用的频率sketch
    static std::unique_ptr<CountMinSketch> createFrequencySketch(size_t cache_size);
};

//===================================================================
// Utility Functions
//===================================================================

// 估计错误率
double estimateErrorRate(size_t sample_size, size_t width, size_t depth);

// 计算最优宽度和深度
std::pair<size_t, size_t> calculateOptimalDimensions(size_t sample_size, 
                                                    double error_rate);

// 检查CMS配置是否合理
bool isValidCMSConfig(const CMSConfig& config);

} // namespace W_Tinylfu
} // namespace CPR

#endif // W_TINYLFU_SKETCH_CMS_H_
