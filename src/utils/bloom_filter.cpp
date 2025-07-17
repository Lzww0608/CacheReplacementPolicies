/*
@Author: Lzww
@LastEditTime: 2025-7-17 21:50:54
@Description: Bloom Filter for CRP
@Language: C++17
*/

#include "../../include/utils/bloom_filter.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <limits>

namespace crp {
namespace utils {

//===================================================================
// MurmurHash3 Implementation
//===================================================================

uint32_t MurmurHash3::rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

uint64_t MurmurHash3::rotl64(uint64_t x, int8_t r) {
    return (x << r) | (x >> (64 - r));
}

uint32_t MurmurHash3::fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

uint64_t MurmurHash3::fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

uint32_t MurmurHash3::hash32(const void* key, int len, uint32_t seed) {
    const uint8_t* data = static_cast<const uint8_t*>(key);
    const int nblocks = len / 4;
    
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
    h1 ^= len;
    h1 = fmix32(h1);
    
    return h1;
}

MurmurHash3::Hash128 MurmurHash3::hash128(const void* key, int len, uint32_t seed) {
    const uint8_t* data = static_cast<const uint8_t*>(key);
    const int nblocks = len / 16;
    
    uint64_t h1 = seed;
    uint64_t h2 = seed;
    
    const uint64_t c1 = 0x87c37b91114253d5ULL;
    const uint64_t c2 = 0x4cf5ad432745937fULL;
    
    // Body
    const uint64_t* blocks = reinterpret_cast<const uint64_t*>(data);
    
    for (int i = 0; i < nblocks; i++) {
        uint64_t k1 = blocks[i * 2 + 0];
        uint64_t k2 = blocks[i * 2 + 1];
        
        k1 *= c1; k1 = rotl64(k1, 31); k1 *= c2; h1 ^= k1;
        
        h1 = rotl64(h1, 27); h1 += h2; h1 = h1 * 5 + 0x52dce729;
        
        k2 *= c2; k2 = rotl64(k2, 33); k2 *= c1; h2 ^= k2;
        
        h2 = rotl64(h2, 31); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
    }
    
    // Tail
    const uint8_t* tail = data + nblocks * 16;
    
    uint64_t k1 = 0;
    uint64_t k2 = 0;
    
    switch (len & 15) {
        case 15: k2 ^= static_cast<uint64_t>(tail[14]) << 48;
        case 14: k2 ^= static_cast<uint64_t>(tail[13]) << 40;
        case 13: k2 ^= static_cast<uint64_t>(tail[12]) << 32;
        case 12: k2 ^= static_cast<uint64_t>(tail[11]) << 24;
        case 11: k2 ^= static_cast<uint64_t>(tail[10]) << 16;
        case 10: k2 ^= static_cast<uint64_t>(tail[9]) << 8;
        case  9: k2 ^= static_cast<uint64_t>(tail[8]) << 0;
                 k2 *= c2; k2 = rotl64(k2, 33); k2 *= c1; h2 ^= k2;
        
        case  8: k1 ^= static_cast<uint64_t>(tail[7]) << 56;
        case  7: k1 ^= static_cast<uint64_t>(tail[6]) << 48;
        case  6: k1 ^= static_cast<uint64_t>(tail[5]) << 40;
        case  5: k1 ^= static_cast<uint64_t>(tail[4]) << 32;
        case  4: k1 ^= static_cast<uint64_t>(tail[3]) << 24;
        case  3: k1 ^= static_cast<uint64_t>(tail[2]) << 16;
        case  2: k1 ^= static_cast<uint64_t>(tail[1]) << 8;
        case  1: k1 ^= static_cast<uint64_t>(tail[0]) << 0;
                 k1 *= c1; k1 = rotl64(k1, 31); k1 *= c2; h1 ^= k1;
    }
    
    // Finalization
    h1 ^= len; h2 ^= len;
    
    h1 += h2;
    h2 += h1;
    
    h1 = fmix64(h1);
    h2 = fmix64(h2);
    
    h1 += h2;
    h2 += h1;
    
    return Hash128(h1, h2);
}

//===================================================================
// Bloom Filter Parameters Implementation
//===================================================================

void BloomFilterParams::calculateOptimalParams() {
    // Calculate optimal bit array size
    // m = -n * ln(p) / (ln(2)^2)
    double optimal_size = -expected_elements * std::log(false_positive_rate) / (std::log(2.0) * std::log(2.0));
    
    // Apply reasonable bounds to prevent memory exhaustion
    const size_t MAX_BITS = 1024 * 1024 * 1024;  // 1 billion bits (128MB)
    const size_t MIN_BITS = 64;
    
    if (optimal_size < MIN_BITS) optimal_size = MIN_BITS;
    if (optimal_size > MAX_BITS) optimal_size = MAX_BITS;
    
    bit_array_size = static_cast<size_t>(optimal_size);
    
    // Calculate optimal number of hash functions
    // k = (m/n) * ln(2)
    num_hash_functions = static_cast<size_t>((static_cast<double>(bit_array_size) / expected_elements) * std::log(2.0));
    
    // Ensure minimum values
    if (num_hash_functions == 0) num_hash_functions = 1;
    if (bit_array_size == 0) bit_array_size = 1;
    
    // Cap maximum hash functions for performance
    if (num_hash_functions > 10) num_hash_functions = 10;
}

//===================================================================
// Standard Bloom Filter Implementation
//===================================================================

BloomFilter::BloomFilter(const BloomFilterParams& params) 
    : bit_array_size_(params.bit_array_size)
    , num_hash_functions_(params.num_hash_functions)
    , element_count_(0) {
    
    assert(params.isValid());
    
    // Allocate bit array (aligned to 64-bit words)
    size_t word_count = (bit_array_size_ + 63) / 64;
    bit_array_ = std::make_unique<uint64_t[]>(word_count);
    
    // Initialize to zero
    std::memset(bit_array_.get(), 0, word_count * sizeof(uint64_t));
}

BloomFilter::BloomFilter(size_t bit_array_size, size_t num_hash_functions)
    : bit_array_size_(bit_array_size)
    , num_hash_functions_(num_hash_functions)
    , element_count_(0) {
    
    assert(bit_array_size > 0 && num_hash_functions > 0);
    
    // Allocate bit array
    size_t word_count = (bit_array_size_ + 63) / 64;
    bit_array_ = std::make_unique<uint64_t[]>(word_count);
    
    // Initialize to zero
    std::memset(bit_array_.get(), 0, word_count * sizeof(uint64_t));
}

BloomFilter::~BloomFilter() = default;

std::vector<uint32_t> BloomFilter::generateHashes(const void* key, size_t key_len) const {
    std::vector<uint32_t> hashes;
    hashes.reserve(num_hash_functions_);
    
    // Use double hashing technique: h1 + i * h2
    auto hash128 = MurmurHash3::hash128(key, static_cast<int>(key_len));
    uint32_t h1 = static_cast<uint32_t>(hash128.h1);
    uint32_t h2 = static_cast<uint32_t>(hash128.h2);
    
    // Ensure h2 is odd for better distribution
    if (h2 % 2 == 0) h2 += 1;
    
    for (size_t i = 0; i < num_hash_functions_; ++i) {
        uint32_t hash = h1 + i * h2;
        hashes.push_back(hash % bit_array_size_);
    }
    
    return hashes;
}

void BloomFilter::setBit(size_t index) {
    assert(index < bit_array_size_);
    
    size_t word_index = getWordIndex(index);
    size_t bit_offset = getBitOffset(index);
    
    bit_array_[word_index] |= (1ULL << bit_offset);
}

bool BloomFilter::getBit(size_t index) const {
    assert(index < bit_array_size_);
    
    size_t word_index = getWordIndex(index);
    size_t bit_offset = getBitOffset(index);
    
    return (bit_array_[word_index] & (1ULL << bit_offset)) != 0;
}

void BloomFilter::prefetchBit(size_t index) const {
    size_t word_index = getWordIndex(index);
    prefetch(&bit_array_[word_index]);
}

void BloomFilter::add(const void* key, size_t key_len) {
    auto hashes = generateHashes(key, key_len);
    
    for (uint32_t hash : hashes) {
        prefetchBit(hash);
        setBit(hash);
    }
    
    element_count_++;
}

void BloomFilter::add(const std::string& key) {
    add(key.data(), key.length());
}

bool BloomFilter::contains(const void* key, size_t key_len) const {
    auto hashes = generateHashes(key, key_len);
    
    for (uint32_t hash : hashes) {
        prefetchBit(hash);
        if (!getBit(hash)) {
            return false;
        }
    }
    
    return true;
}

bool BloomFilter::contains(const std::string& key) const {
    return contains(key.data(), key.length());
}

void BloomFilter::clear() {
    size_t word_count = (bit_array_size_ + 63) / 64;
    std::memset(bit_array_.get(), 0, word_count * sizeof(uint64_t));
    element_count_ = 0;
}

void BloomFilter::reset() {
    clear();
}

double BloomFilter::current_false_positive_rate() const {
    if (element_count_ == 0) return 0.0;
    
    // Estimate based on current occupancy
    size_t bits_set = 0;
    size_t word_count = (bit_array_size_ + 63) / 64;
    
    for (size_t i = 0; i < word_count; ++i) {
        bits_set += __builtin_popcountll(bit_array_[i]);
    }
    
    double occupancy = static_cast<double>(bits_set) / bit_array_size_;
    return std::pow(occupancy, num_hash_functions_);
}

//===================================================================
// Counting Bloom Filter Implementation
//===================================================================

CountingBloomFilter::CountingBloomFilter(const BloomFilterParams& params, uint8_t counter_bits)
    : counter_array_size_(params.bit_array_size)
    , num_hash_functions_(params.num_hash_functions)
    , counter_bits_(counter_bits) {
    
    assert(params.isValid());
    assert(counter_bits >= 1 && counter_bits <= 8);
    
    max_count_ = (1U << counter_bits) - 1;
    
    // Allocate counter array
    size_t total_bits = counter_array_size_ * counter_bits_;
    size_t byte_count = (total_bits + 7) / 8;
    counter_array_ = std::make_unique<uint8_t[]>(byte_count);
    
    // Initialize to zero
    std::memset(counter_array_.get(), 0, byte_count);
}

CountingBloomFilter::CountingBloomFilter(size_t counter_array_size, 
                                       size_t num_hash_functions,
                                       uint8_t counter_bits)
    : counter_array_size_(counter_array_size)
    , num_hash_functions_(num_hash_functions)
    , counter_bits_(counter_bits) {
    
    assert(counter_array_size > 0 && num_hash_functions > 0);
    assert(counter_bits >= 1 && counter_bits <= 8);
    
    max_count_ = (1U << counter_bits) - 1;
    
    // Allocate counter array
    size_t total_bits = counter_array_size_ * counter_bits_;
    size_t byte_count = (total_bits + 7) / 8;
    counter_array_ = std::make_unique<uint8_t[]>(byte_count);
    
    // Initialize to zero
    std::memset(counter_array_.get(), 0, byte_count);
}

CountingBloomFilter::~CountingBloomFilter() = default;

std::vector<uint32_t> CountingBloomFilter::generateHashes(const void* key, size_t key_len) const {
    std::vector<uint32_t> hashes;
    hashes.reserve(num_hash_functions_);
    
    // Use double hashing technique
    auto hash128 = MurmurHash3::hash128(key, static_cast<int>(key_len));
    uint32_t h1 = static_cast<uint32_t>(hash128.h1);
    uint32_t h2 = static_cast<uint32_t>(hash128.h2);
    
    // Ensure h2 is odd
    if (h2 % 2 == 0) h2 += 1;
    
    for (size_t i = 0; i < num_hash_functions_; ++i) {
        uint32_t hash = h1 + i * h2;
        hashes.push_back(hash % counter_array_size_);
    }
    
    return hashes;
}

size_t CountingBloomFilter::getByteIndex(size_t counter_index) const {
    return (counter_index * counter_bits_) / 8;
}

size_t CountingBloomFilter::getBitOffset(size_t counter_index) const {
    return (counter_index * counter_bits_) % 8;
}

uint32_t CountingBloomFilter::getCounterMask() const {
    return (1U << counter_bits_) - 1;
}

uint32_t CountingBloomFilter::getCounter(size_t index) const {
    assert(index < counter_array_size_);
    
    size_t byte_index = getByteIndex(index);
    size_t bit_offset = getBitOffset(index);
    uint32_t mask = getCounterMask();
    
    if (bit_offset + counter_bits_ <= 8) {
        // Counter fits in single byte
        return (counter_array_[byte_index] >> bit_offset) & mask;
    } else {
        // Counter spans multiple bytes
        uint32_t result = 0;
        size_t bits_read = 0;
        
        while (bits_read < counter_bits_) {
            size_t bits_in_byte = std::min(counter_bits_ - bits_read, 8 - bit_offset);
            uint8_t byte_mask = (1U << bits_in_byte) - 1;
            
            result |= ((counter_array_[byte_index] >> bit_offset) & byte_mask) << bits_read;
            
            bits_read += bits_in_byte;
            byte_index++;
            bit_offset = 0;
        }
        
        return result;
    }
}

void CountingBloomFilter::setCounter(size_t index, uint32_t value) {
    assert(index < counter_array_size_);
    assert(value <= max_count_);
    
    size_t byte_index = getByteIndex(index);
    size_t bit_offset = getBitOffset(index);
    uint32_t mask = getCounterMask();
    
    if (bit_offset + counter_bits_ <= 8) {
        // Counter fits in single byte
        counter_array_[byte_index] &= ~(mask << bit_offset);
        counter_array_[byte_index] |= (value << bit_offset);
    } else {
        // Counter spans multiple bytes
        size_t bits_written = 0;
        
        while (bits_written < counter_bits_) {
            size_t bits_in_byte = std::min(counter_bits_ - bits_written, 8 - bit_offset);
            uint8_t byte_mask = (1U << bits_in_byte) - 1;
            
            counter_array_[byte_index] &= ~(byte_mask << bit_offset);
            counter_array_[byte_index] |= ((value >> bits_written) & byte_mask) << bit_offset;
            
            bits_written += bits_in_byte;
            byte_index++;
            bit_offset = 0;
        }
    }
}

bool CountingBloomFilter::incrementCounter(size_t index) {
    uint32_t current = getCounter(index);
    if (current < max_count_) {
        setCounter(index, current + 1);
        return true;
    }
    return false;
}

bool CountingBloomFilter::decrementCounter(size_t index) {
    uint32_t current = getCounter(index);
    if (current > 0) {
        setCounter(index, current - 1);
        return true;
    }
    return false;
}

void CountingBloomFilter::add(const void* key, size_t key_len) {
    auto hashes = generateHashes(key, key_len);
    
    for (uint32_t hash : hashes) {
        incrementCounter(hash);
    }
}

void CountingBloomFilter::add(const std::string& key) {
    add(key.data(), key.length());
}

bool CountingBloomFilter::remove(const void* key, size_t key_len) {
    auto hashes = generateHashes(key, key_len);
    
    // Check if all counters are > 0
    for (uint32_t hash : hashes) {
        if (getCounter(hash) == 0) {
            return false;
        }
    }
    
    // Decrement all counters
    for (uint32_t hash : hashes) {
        decrementCounter(hash);
    }
    
    return true;
}

bool CountingBloomFilter::remove(const std::string& key) {
    return remove(key.data(), key.length());
}

uint32_t CountingBloomFilter::estimate(const void* key, size_t key_len) const {
    auto hashes = generateHashes(key, key_len);
    
    uint32_t min_count = std::numeric_limits<uint32_t>::max();
    
    for (uint32_t hash : hashes) {
        uint32_t count = getCounter(hash);
        min_count = std::min(min_count, count);
    }
    
    return min_count;
}

uint32_t CountingBloomFilter::estimate(const std::string& key) const {
    return estimate(key.data(), key.length());
}

void CountingBloomFilter::reset() {
    // TinyLFU reset: divide all counters by 2
    for (size_t i = 0; i < counter_array_size_; ++i) {
        uint32_t count = getCounter(i);
        setCounter(i, count / 2);
    }
}

void CountingBloomFilter::clear() {
    size_t total_bits = counter_array_size_ * counter_bits_;
    size_t byte_count = (total_bits + 7) / 8;
    std::memset(counter_array_.get(), 0, byte_count);
}

uint64_t CountingBloomFilter::total_count() const {
    uint64_t total = 0;
    for (size_t i = 0; i < counter_array_size_; ++i) {
        total += getCounter(i);
    }
    return total;
}

//===================================================================
// Bloom Filter Factory Implementation
//===================================================================

std::unique_ptr<BloomFilter> BloomFilterFactory::createBloomFilter(
    size_t expected_elements, 
    double false_positive_rate) {
    
    BloomFilterParams params(expected_elements, false_positive_rate);
    return std::make_unique<BloomFilter>(params);
}

std::unique_ptr<CountingBloomFilter> BloomFilterFactory::createCountingBloomFilter(
    size_t expected_elements,
    double false_positive_rate,
    uint8_t counter_bits) {
    
    BloomFilterParams params(expected_elements, false_positive_rate);
    return std::make_unique<CountingBloomFilter>(params, counter_bits);
}

std::unique_ptr<BloomFilter> BloomFilterFactory::createDoorkeeper(size_t cache_size) {
    // Doorkeeper typically needs to track 2-3x cache size
    size_t expected_elements = cache_size * 3;
    double false_positive_rate = 0.01;  // 1% false positive rate
    
    return createBloomFilter(expected_elements, false_positive_rate);
}

std::unique_ptr<CountingBloomFilter> BloomFilterFactory::createFrequencySketch(
    size_t sample_size, 
    size_t cache_size) {
    
    // Frequency sketch size is typically 8-16x cache size
    size_t expected_elements = sample_size;
    double false_positive_rate = 0.01;
    uint8_t counter_bits = 4;  // 4 bits per counter (0-15)
    
    return createCountingBloomFilter(expected_elements, false_positive_rate, counter_bits);
}

//===================================================================
// Utility Functions Implementation
//===================================================================

BloomFilterParams calculateOptimalParams(size_t expected_elements, 
                                        double false_positive_rate) {
    return BloomFilterParams(expected_elements, false_positive_rate);
}

double estimateFalsePositiveRate(size_t num_elements, 
                               size_t bit_array_size, 
                               size_t num_hash_functions) {
    if (num_elements == 0) return 0.0;
    
    // (1 - e^(-k*n/m))^k
    double ratio = static_cast<double>(num_hash_functions * num_elements) / bit_array_size;
    return std::pow(1.0 - std::exp(-ratio), num_hash_functions);
}

bool isSimdSupported() {
#ifdef __SSE__
    return true;
#else
    return false;
#endif
}

} // namespace utils
} // namespace crp 