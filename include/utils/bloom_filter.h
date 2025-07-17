/*
@Author: Lzww
@LastEditTime: 2025-7-17 21:50:54
@Description: Bloom Filter for CRP
@Language: C++17
*/


#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cassert>
#include <string>
#include <immintrin.h>  // For SIMD instructions

namespace crp {
namespace utils {

// Forward declarations
class BloomFilter;
class CountingBloomFilter;
class MurmurHash3;

//===================================================================
// MurmurHash3 Implementation
//===================================================================

class MurmurHash3 {
public:
    static constexpr uint32_t SEED = 0x9747b28c;
    
    // 128-bit hash for better distribution
    struct Hash128 {
        uint64_t h1;
        uint64_t h2;
        
        Hash128(uint64_t h1 = 0, uint64_t h2 = 0) : h1(h1), h2(h2) {}
    };
    
    // Primary hash function
    static uint32_t hash32(const void* key, int len, uint32_t seed = SEED);
    
    // 128-bit hash for double hashing
    static Hash128 hash128(const void* key, int len, uint32_t seed = SEED);
    
    // Convenience overloads
    static uint32_t hash32(const std::string& str, uint32_t seed = SEED) {
        return hash32(str.data(), static_cast<int>(str.length()), seed);
    }
    
    static Hash128 hash128(const std::string& str, uint32_t seed = SEED) {
        return hash128(str.data(), static_cast<int>(str.length()), seed);
    }
    
    // Template for POD types
    template<typename T>
    static uint32_t hash32(const T& value, uint32_t seed = SEED) {
        return hash32(&value, sizeof(T), seed);
    }
    
    template<typename T>
    static Hash128 hash128(const T& value, uint32_t seed = SEED) {
        return hash128(&value, sizeof(T), seed);
    }

private:
    static uint32_t rotl32(uint32_t x, int8_t r);
    static uint64_t rotl64(uint64_t x, int8_t r);
    static uint32_t fmix32(uint32_t h);
    static uint64_t fmix64(uint64_t k);
};

//===================================================================
// Bloom Filter Parameters
//===================================================================

struct BloomFilterParams {
    size_t expected_elements;    // Expected number of elements
    double false_positive_rate;  // Desired false positive rate
    size_t num_hash_functions;   // Number of hash functions
    size_t bit_array_size;       // Size of bit array in bits
    
    BloomFilterParams(size_t expected = 1000, double fpr = 0.01)
        : expected_elements(expected), false_positive_rate(fpr) {
        calculateOptimalParams();
    }
    
    void calculateOptimalParams();
    
    // Validate parameters
    bool isValid() const {
        return expected_elements > 0 && 
               false_positive_rate > 0 && false_positive_rate < 1.0 &&
               num_hash_functions > 0 && bit_array_size > 0;
    }
};

//===================================================================
// Standard Bloom Filter (for Doorkeeper)
//===================================================================

class BloomFilter {
public:
    // Constructor with parameters
    explicit BloomFilter(const BloomFilterParams& params);
    
    // Constructor with explicit size and hash count
    BloomFilter(size_t bit_array_size, size_t num_hash_functions);
    
    // Destructor
    ~BloomFilter();
    
    // Non-copyable but movable
    BloomFilter(const BloomFilter&) = delete;
    BloomFilter& operator=(const BloomFilter&) = delete;
    BloomFilter(BloomFilter&&) = default;
    BloomFilter& operator=(BloomFilter&&) = default;
    
    // Add an element to the filter
    void add(const void* key, size_t key_len);
    void add(const std::string& key);
    
    template<typename T>
    void add(const T& value) {
        add(&value, sizeof(T));
    }
    
    // Check if an element might be in the set
    bool contains(const void* key, size_t key_len) const;
    bool contains(const std::string& key) const;
    
    template<typename T>
    bool contains(const T& value) const {
        return contains(&value, sizeof(T));
    }
    
    // Clear all elements
    void clear();
    
    // Reset all bits to 0
    void reset();
    
    // Get filter statistics
    size_t size() const { return bit_array_size_; }
    size_t num_hash_functions() const { return num_hash_functions_; }
    double current_false_positive_rate() const;
    
    // Get memory usage in bytes
    size_t memory_usage() const {
        return (bit_array_size_ + 7) / 8;  // Convert bits to bytes
    }
    
    // Check if filter is empty
    bool empty() const {
        return element_count_.load() == 0;
    }
    
    // Get element count (approximate)
    size_t element_count() const {
        return element_count_.load();
    }

private:
    size_t bit_array_size_;      // Size in bits
    size_t num_hash_functions_;  // Number of hash functions
    std::unique_ptr<uint64_t[]> bit_array_;  // Bit array using 64-bit words
    std::atomic<size_t> element_count_;      // Approximate element count
    
    // Hash functions
    std::vector<uint32_t> generateHashes(const void* key, size_t key_len) const;
    
    // Bit manipulation helpers
    void setBit(size_t index);
    bool getBit(size_t index) const;
    size_t getWordIndex(size_t bit_index) const { return bit_index / 64; }
    size_t getBitOffset(size_t bit_index) const { return bit_index % 64; }
    
    // SIMD optimized operations
    void prefetchBit(size_t index) const;
};

//===================================================================
// Counting Bloom Filter (for Frequency Sketching)
//===================================================================

class CountingBloomFilter {
public:
    // Constructor with parameters
    explicit CountingBloomFilter(const BloomFilterParams& params, 
                                uint8_t counter_bits = 4);
    
    // Constructor with explicit parameters
    CountingBloomFilter(size_t counter_array_size, 
                       size_t num_hash_functions,
                       uint8_t counter_bits = 4);
    
    // Destructor
    ~CountingBloomFilter();
    
    // Non-copyable but movable
    CountingBloomFilter(const CountingBloomFilter&) = delete;
    CountingBloomFilter& operator=(const CountingBloomFilter&) = delete;
    CountingBloomFilter(CountingBloomFilter&&) = default;
    CountingBloomFilter& operator=(CountingBloomFilter&&) = default;
    
    // Add an element (increment counters)
    void add(const void* key, size_t key_len);
    void add(const std::string& key);
    
    template<typename T>
    void add(const T& value) {
        add(&value, sizeof(T));
    }
    
    // Remove an element (decrement counters) - only if supported
    bool remove(const void* key, size_t key_len);
    bool remove(const std::string& key);
    
    template<typename T>
    bool remove(const T& value) {
        return remove(&value, sizeof(T));
    }
    
    // Get estimated count
    uint32_t estimate(const void* key, size_t key_len) const;
    uint32_t estimate(const std::string& key) const;
    
    template<typename T>
    uint32_t estimate(const T& value) const {
        return estimate(&value, sizeof(T));
    }
    
    // Check if element might be in the set
    bool contains(const void* key, size_t key_len) const {
        return estimate(key, key_len) > 0;
    }
    
    bool contains(const std::string& key) const {
        return estimate(key) > 0;
    }
    
    template<typename T>
    bool contains(const T& value) const {
        return estimate(value) > 0;
    }
    
    // TinyLFU specific operations
    void reset();  // Divide all counters by 2 (TinyLFU reset operation)
    void clear();  // Set all counters to 0
    
    // Get filter statistics
    size_t size() const { return counter_array_size_; }
    size_t num_hash_functions() const { return num_hash_functions_; }
    uint8_t counter_bits() const { return counter_bits_; }
    uint32_t max_count() const { return max_count_; }
    
    // Memory usage in bytes
    size_t memory_usage() const {
        return (counter_array_size_ * counter_bits_ + 7) / 8;
    }
    
    // Get total count (sum of all counters)
    uint64_t total_count() const;

private:
    size_t counter_array_size_;  // Number of counters
    size_t num_hash_functions_;  // Number of hash functions
    uint8_t counter_bits_;       // Bits per counter
    uint32_t max_count_;         // Maximum counter value
    std::unique_ptr<uint8_t[]> counter_array_;  // Packed counter array
    
    // Hash functions
    std::vector<uint32_t> generateHashes(const void* key, size_t key_len) const;
    
    // Counter manipulation
    uint32_t getCounter(size_t index) const;
    void setCounter(size_t index, uint32_t value);
    bool incrementCounter(size_t index);
    bool decrementCounter(size_t index);
    
    // Helper functions
    size_t getByteIndex(size_t counter_index) const;
    size_t getBitOffset(size_t counter_index) const;
    uint32_t getCounterMask() const;
};

//===================================================================
// Optimized Bloom Filter Factory
//===================================================================

class BloomFilterFactory {
public:
    // Create optimal Bloom filter for given parameters
    static std::unique_ptr<BloomFilter> createBloomFilter(
        size_t expected_elements, 
        double false_positive_rate = 0.01);
    
    // Create counting Bloom filter for TinyLFU
    static std::unique_ptr<CountingBloomFilter> createCountingBloomFilter(
        size_t expected_elements,
        double false_positive_rate = 0.01,
        uint8_t counter_bits = 4);
    
    // Create Doorkeeper Bloom filter for W-TinyLFU
    static std::unique_ptr<BloomFilter> createDoorkeeper(
        size_t cache_size);
    
    // Create frequency sketch for W-TinyLFU
    static std::unique_ptr<CountingBloomFilter> createFrequencySketch(
        size_t sample_size, 
        size_t cache_size);
};

//===================================================================
// Utility Functions
//===================================================================

// Calculate optimal Bloom filter parameters
BloomFilterParams calculateOptimalParams(size_t expected_elements, 
                                       double false_positive_rate);

// Estimate false positive rate for given parameters
double estimateFalsePositiveRate(size_t num_elements, 
                               size_t bit_array_size, 
                               size_t num_hash_functions);

// Check if CPU supports required SIMD instructions
bool isSimdSupported();

// Prefetch cache line for better performance
inline void prefetch(const void* addr) {
#ifdef __SSE__
    _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
#endif
}

} // namespace utils
} // namespace crp

#endif // BLOOM_FILTER_H
