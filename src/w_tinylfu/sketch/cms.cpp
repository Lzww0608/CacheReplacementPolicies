/*
@Author: Lzww
@LastEditTime: 2025-7-22 21:42:10
@Description: Count-Min Sketch implementation for W-TinyLFU
@Language: C++17
*/

#include "../../../include/w_tinylfu/sketch/cms.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>

namespace CPR {
namespace W_Tinylfu {

//===================================================================
// CMSHash Implementation
//===================================================================

uint32_t CMSHash::rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

uint32_t CMSHash::fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

uint32_t CMSHash::murmur3_32(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = static_cast<const uint8_t*>(key);
    const int nblocks = static_cast<int>(len) / 4;
    
    uint32_t h1 = seed;
    
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    
    // Body
    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data + nblocks * 4);
    
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];
        
        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;
        
        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }
    
    // Tail
    const uint8_t* tail = data + nblocks * 4;
    uint32_t k1 = 0;
    
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1; k1 = rotl32(k1, 15); k1 *= c2; h1 ^= k1;
    }
    
    // Finalization
    h1 ^= static_cast<uint32_t>(len);
    h1 = fmix32(h1);
    
    return h1;
}

uint32_t CMSHash::fnv1a_32(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = static_cast<const uint8_t*>(key);
    uint32_t hash = 0x811c9dc5 ^ seed;  // FNV offset basis
    
    for (size_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= 0x01000193;  // FNV prime
    }
    
    return hash;
}

std::vector<uint32_t> CMSHash::doubleHash(const void* key, size_t len, 
                                         size_t count, uint32_t seed) {
    std::vector<uint32_t> hashes;
    hashes.reserve(count);
    
    // 使用双重哈希技术：h1 + i * h2
    auto hash128 = [](const void* key, size_t len, uint32_t seed) -> std::pair<uint32_t, uint32_t> {
        uint32_t h1 = murmur3_32(key, len, seed);
        uint32_t h2 = fnv1a_32(key, len, seed + 0x9747b28c);
        
        // 确保h2是奇数以获得更好的分布
        if (h2 % 2 == 0) h2 += 1;
        
        return {h1, h2};
    };
    
    auto [h1, h2] = hash128(key, len, seed);
    
    for (size_t i = 0; i < count; ++i) {
        uint32_t hash = h1 + i * h2;
        hashes.push_back(hash);
    }
    
    return hashes;
}

//===================================================================
// CountMinSketch Implementation
//===================================================================


CountMinSketch::CountMinSketch(CountMinSketch&& other) noexcept
    : config_(std::move(other.config_))
    , counter_array_(std::move(other.counter_array_))
    , seeds_(std::move(other.seeds_))
    , access_count_(other.access_count_.load())
    , total_increments_(other.total_increments_.load())
    , total_decays_(other.total_decays_.load()) {
    
    // 重置other的原子变量
    other.access_count_ = 0;
    other.total_increments_ = 0;
    other.total_decays_ = 0;
}

CountMinSketch& CountMinSketch::operator=(CountMinSketch&& other) noexcept {
    if (this != &other) {
        // 使用写锁保护当前对象
        std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);
        
        // 移动配置和资源
        config_ = std::move(other.config_);
        counter_array_ = std::move(other.counter_array_);
        seeds_ = std::move(other.seeds_);
        
        // 原子变量需要特殊处理
        access_count_ = other.access_count_.load();
        total_increments_ = other.total_increments_.load();
        total_decays_ = other.total_decays_.load();
        
        // 重置other的原子变量
        other.access_count_ = 0;
        other.total_increments_ = 0;
        other.total_decays_ = 0;
    }
    return *this;
}

CountMinSketch::CountMinSketch(const CMSConfig& config)
    : config_(config)
    , access_count_(0)
    , total_increments_(0)
    , total_decays_(0) {
    
    assert(config_.isValid());
    
    // 分配计数器数组
    size_t total_bytes = config_.memoryUsage();
    counter_array_ = std::make_unique<uint8_t[]>(total_bytes);
    
    // 初始化为零
    std::memset(counter_array_.get(), 0, total_bytes);
    
    // 初始化种子
    initializeSeeds();
}

CountMinSketch::~CountMinSketch() = default;

void CountMinSketch::initializeSeeds() {
    seeds_.reserve(config_.depth);
    
    // 使用不同的种子确保哈希函数独立性
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0x12345678, 0x87654321);
    
    for (size_t i = 0; i < config_.depth; ++i) {
        seeds_.push_back(dis(gen));
    }
}

size_t CountMinSketch::getByteIndex(size_t row, size_t col) const {
    size_t counter_index = row * config_.width + col;
    return (counter_index * config_.bits_per_counter) / 8;
}

size_t CountMinSketch::getBitOffset(size_t row, size_t col) const {
    size_t counter_index = row * config_.width + col;
    return (counter_index * config_.bits_per_counter) % 8;
}

uint32_t CountMinSketch::getCounterMask() const {
    return (1U << config_.bits_per_counter) - 1;
}

uint32_t CountMinSketch::getCounter(size_t row, size_t col) const {
    assert(row < config_.depth && col < config_.width);
    
    size_t byte_index = getByteIndex(row, col);
    size_t bit_offset = getBitOffset(row, col);
    uint32_t mask = getCounterMask();
    
    if (bit_offset + config_.bits_per_counter <= 8) {
        // 计数器在单个字节内
        return (counter_array_[byte_index] >> bit_offset) & mask;
    } else {
        // 计数器跨越多个字节
        uint32_t result = 0;
        size_t bits_read = 0;
        
        while (bits_read < config_.bits_per_counter) {
            size_t bits_in_byte = std::min(config_.bits_per_counter - bits_read, 
                                          8 - bit_offset);
            uint8_t byte_mask = (1U << bits_in_byte) - 1;
            
            result |= ((counter_array_[byte_index] >> bit_offset) & byte_mask) << bits_read;
            
            bits_read += bits_in_byte;
            byte_index++;
            bit_offset = 0;
        }
        
        return result;
    }
}

void CountMinSketch::setCounter(size_t row, size_t col, uint32_t value) {
    assert(row < config_.depth && col < config_.width);
    assert(value <= config_.maxCount());
    
    size_t byte_index = getByteIndex(row, col);
    size_t bit_offset = getBitOffset(row, col);
    uint32_t mask = getCounterMask();
    
    if (bit_offset + config_.bits_per_counter <= 8) {
        // 计数器在单个字节内
        counter_array_[byte_index] &= ~(mask << bit_offset);
        counter_array_[byte_index] |= (value << bit_offset);
    } else {
        // 计数器跨越多个字节
        size_t bits_written = 0;
        
        while (bits_written < config_.bits_per_counter) {
            size_t bits_in_byte = std::min(config_.bits_per_counter - bits_written, 
                                          8 - bit_offset);
            uint8_t byte_mask = (1U << bits_in_byte) - 1;
            
            counter_array_[byte_index] &= ~(byte_mask << bit_offset);
            counter_array_[byte_index] |= ((value >> bits_written) & byte_mask) << bit_offset;
            
            bits_written += bits_in_byte;
            byte_index++;
            bit_offset = 0;
        }
    }
}

bool CountMinSketch::incrementCounter(size_t row, size_t col) {
    uint32_t current = getCounter(row, col);
    if (current < config_.maxCount()) {
        setCounter(row, col, current + 1);
        return true;
    }
    return false;
}

std::vector<uint32_t> CountMinSketch::generateHashes(const void* key, size_t key_len) const {
    std::vector<uint32_t> hashes;
    hashes.reserve(config_.depth);
    
    for (size_t i = 0; i < config_.depth; ++i) {
        uint32_t hash = CMSHash::murmur3_32(key, key_len, seeds_[i]);
        hashes.push_back(hash % config_.width);
    }
    
    return hashes;
}

void CountMinSketch::increment(const void* key, size_t key_len) {
    std::shared_lock<std::shared_mutex> read_lock(rw_mutex_);
    
    auto hashes = generateHashes(key, key_len);
    
    for (uint32_t hash : hashes) {
        size_t row = &hash - &hashes[0];
        size_t col = hash;
        incrementCounter(row, col);
    }
    
    total_increments_++;
    uint64_t current_access = access_count_.fetch_add(1) + 1;
    
    // 检查是否需要触发衰减
    if (current_access % config_.decay_threshold == 0) {
        read_lock.unlock();
        decay();
    }
}

void CountMinSketch::increment(const std::string& key) {
    increment(key.data(), key.length());
}

uint32_t CountMinSketch::estimate(const void* key, size_t key_len) const {
    std::shared_lock<std::shared_mutex> read_lock(rw_mutex_);
    
    auto hashes = generateHashes(key, key_len);
    
    uint32_t min_count = std::numeric_limits<uint32_t>::max();
    
    for (uint32_t hash : hashes) {
        size_t row = &hash - &hashes[0];
        size_t col = hash;
        uint32_t count = getCounter(row, col);
        min_count = std::min(min_count, count);
    }
    
    return min_count;
}

uint32_t CountMinSketch::estimate(const std::string& key) const {
    return estimate(key.data(), key.length());
}

void CountMinSketch::decay() {
    std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);
    
    // 对所有计数器执行右移操作（频率衰减）
    for (size_t row = 0; row < config_.depth; ++row) {
        for (size_t col = 0; col < config_.width; ++col) {
            uint32_t current = getCounter(row, col);
            setCounter(row, col, current >> 1);  // 右移一位
        }
    }
    
    total_decays_++;
}

void CountMinSketch::reset() {
    std::unique_lock<std::shared_mutex> write_lock(rw_mutex_);
    
    // 重置所有计数器为0
    size_t total_bytes = config_.memoryUsage();
    std::memset(counter_array_.get(), 0, total_bytes);
    
    // 重置统计信息
    access_count_ = 0;
    total_increments_ = 0;
    total_decays_ = 0;
}

void CountMinSketch::clear() {
    reset();
}

CountMinSketch::Stats CountMinSketch::getStats() const {
    std::shared_lock<std::shared_mutex> read_lock(rw_mutex_);
    
    Stats stats;
    stats.total_increments = total_increments_.load();
    stats.total_decays = total_decays_.load();
    stats.current_access_count = access_count_.load();
    stats.max_counter_value = config_.maxCount();
    stats.memory_usage = config_.memoryUsage();
    
    return stats;
}

//===================================================================
// CMSFactory Implementation
//===================================================================

std::unique_ptr<CountMinSketch> CMSFactory::createStandard(size_t sample_size) {
    // 标准配置：width=16384, depth=4, bits_per_counter=4
    CMSConfig config(16384, 4, 4, 15);
    return std::make_unique<CountMinSketch>(config);
}

std::unique_ptr<CountMinSketch> CMSFactory::createCustom(const CMSConfig& config) {
    if (!config.isValid()) {
        throw std::invalid_argument("Invalid CMS configuration");
    }
    return std::make_unique<CountMinSketch>(config);
}

std::unique_ptr<CountMinSketch> CMSFactory::createOptimized(size_t sample_size, 
                                                           double error_rate) {
    CMSConfig config = calculateOptimalParams(sample_size, error_rate);
    return std::make_unique<CountMinSketch>(config);
}

CMSConfig CMSFactory::calculateOptimalParams(size_t sample_size, 
                                            double error_rate,
                                            uint8_t bits_per_counter) {
    // 计算最优宽度和深度
    auto [width, depth] = calculateOptimalDimensions(sample_size, error_rate);
    
    return CMSConfig(width, depth, bits_per_counter, 15);
}

std::unique_ptr<CountMinSketch> CMSFactory::createFrequencySketch(size_t cache_size) {
    // W-TinyLFU专用配置：样本大小为缓存大小的8-16倍
    size_t sample_size = cache_size * 12;  // 12倍缓存大小
    return createOptimized(sample_size, 0.01);
}

//===================================================================
// Utility Functions Implementation
//===================================================================

double estimateErrorRate(size_t sample_size, size_t width, size_t depth) {
    // 估计错误率：e^(-depth * width / sample_size)
    double exponent = -static_cast<double>(depth * width) / sample_size;
    return std::exp(exponent);
}

std::pair<size_t, size_t> calculateOptimalDimensions(size_t sample_size, 
                                                    double error_rate) {
    // 计算最优宽度：width = e / error_rate
    size_t width = static_cast<size_t>(std::exp(1.0) / error_rate);
    
    // 计算最优深度：depth = ln(1/error_rate)
    size_t depth = static_cast<size_t>(std::log(1.0 / error_rate));
    
    // 确保最小值
    width = std::max(width, static_cast<size_t>(64));
    depth = std::max(depth, static_cast<size_t>(2));
    
    // 限制最大值以避免内存过度使用
    width = std::min(width, static_cast<size_t>(1024 * 1024));  // 1M
    depth = std::min(depth, static_cast<size_t>(10));
    
    return {width, depth};
}

bool isValidCMSConfig(const CMSConfig& config) {
    return config.isValid();
}

} // namespace W_Tinylfu
} // namespace CPR
