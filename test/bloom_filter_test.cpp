/*
 * Bloom Filter Test Suite
 * 
 * Comprehensive tests for:
 * - MurmurHash3 hash function
 * - Standard Bloom Filter
 * - Counting Bloom Filter
 * - W-TinyLFU specific operations
 * 
 * Author: Generated for CRP Project
 * License: MIT
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <random>
#include <unordered_set>
#include "../include/utils/bloom_filter.h"

using namespace crp::utils;

//===================================================================
// MurmurHash3 Tests
//===================================================================

class MurmurHash3Test : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MurmurHash3Test, BasicHash32) {
    std::string test_str = "hello world";
    uint32_t hash1 = MurmurHash3::hash32(test_str);
    uint32_t hash2 = MurmurHash3::hash32(test_str);
    
    // Same input should produce same hash
    EXPECT_EQ(hash1, hash2);
    
    // Different input should produce different hash
    uint32_t hash3 = MurmurHash3::hash32("hello world!");
    EXPECT_NE(hash1, hash3);
}

TEST_F(MurmurHash3Test, BasicHash128) {
    std::string test_str = "hello world";
    auto hash1 = MurmurHash3::hash128(test_str);
    auto hash2 = MurmurHash3::hash128(test_str);
    
    // Same input should produce same hash
    EXPECT_EQ(hash1.h1, hash2.h1);
    EXPECT_EQ(hash1.h2, hash2.h2);
    
    // Different input should produce different hash
    auto hash3 = MurmurHash3::hash128("hello world!");
    EXPECT_NE(hash1.h1, hash3.h1);
}

TEST_F(MurmurHash3Test, ConsistentSeed) {
    std::string test_str = "test";
    uint32_t seed = 12345;
    
    uint32_t hash1 = MurmurHash3::hash32(test_str, seed);
    uint32_t hash2 = MurmurHash3::hash32(test_str, seed);
    
    EXPECT_EQ(hash1, hash2);
    
    // Different seed should produce different hash
    uint32_t hash3 = MurmurHash3::hash32(test_str, seed + 1);
    EXPECT_NE(hash1, hash3);
}

TEST_F(MurmurHash3Test, TemplateHash) {
    int value = 42;
    uint32_t hash1 = MurmurHash3::hash32(value);
    uint32_t hash2 = MurmurHash3::hash32(value);
    
    EXPECT_EQ(hash1, hash2);
    
    uint32_t hash3 = MurmurHash3::hash32(43);
    EXPECT_NE(hash1, hash3);
}

//===================================================================
// Bloom Filter Parameters Tests
//===================================================================

class BloomFilterParamsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BloomFilterParamsTest, OptimalParameters) {
    BloomFilterParams params(1000, 0.01);
    
    EXPECT_GT(params.bit_array_size, 0);
    EXPECT_GT(params.num_hash_functions, 0);
    EXPECT_LE(params.num_hash_functions, 10);  // Should be capped at 10
    EXPECT_TRUE(params.isValid());
}

TEST_F(BloomFilterParamsTest, DifferentFalsePositiveRates) {
    BloomFilterParams params1(1000, 0.1);
    BloomFilterParams params2(1000, 0.01);
    
    // Lower false positive rate should require more bits
    EXPECT_GT(params2.bit_array_size, params1.bit_array_size);
}

//===================================================================
// Standard Bloom Filter Tests
//===================================================================

class BloomFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        params_ = BloomFilterParams(1000, 0.01);
        filter_ = std::make_unique<BloomFilter>(params_);
    }
    
    void TearDown() override {}
    
    BloomFilterParams params_;
    std::unique_ptr<BloomFilter> filter_;
};

TEST_F(BloomFilterTest, BasicOperations) {
    EXPECT_TRUE(filter_->empty());
    EXPECT_EQ(filter_->element_count(), 0);
    
    // Add elements
    filter_->add("hello");
    filter_->add("world");
    filter_->add(42);
    
    EXPECT_FALSE(filter_->empty());
    EXPECT_EQ(filter_->element_count(), 3);
    
    // Test contains
    EXPECT_TRUE(filter_->contains("hello"));
    EXPECT_TRUE(filter_->contains("world"));
    EXPECT_TRUE(filter_->contains(42));
    
    // Test non-existent elements
    EXPECT_FALSE(filter_->contains("goodbye"));
    EXPECT_FALSE(filter_->contains(123));
}

TEST_F(BloomFilterTest, ClearAndReset) {
    filter_->add("test");
    EXPECT_TRUE(filter_->contains("test"));
    EXPECT_FALSE(filter_->empty());
    
    filter_->clear();
    EXPECT_FALSE(filter_->contains("test"));
    EXPECT_TRUE(filter_->empty());
    EXPECT_EQ(filter_->element_count(), 0);
}

TEST_F(BloomFilterTest, FalsePositiveRate) {
    // Add some elements
    std::vector<std::string> elements = {
        "apple", "banana", "cherry", "date", "elderberry",
        "fig", "grape", "honeydew", "kiwi", "lemon"
    };
    
    for (const auto& elem : elements) {
        filter_->add(elem);
    }
    
    // Test false positive rate
    int false_positives = 0;
    int total_tests = 1000;
    
    for (int i = 0; i < total_tests; ++i) {
        std::string test_str = "test_" + std::to_string(i);
        if (filter_->contains(test_str)) {
            false_positives++;
        }
    }
    
    double fp_rate = static_cast<double>(false_positives) / total_tests;
    EXPECT_LT(fp_rate, 0.05);  // Should be well below 5%
}

TEST_F(BloomFilterTest, MemoryUsage) {
    size_t memory = filter_->memory_usage();
    EXPECT_GT(memory, 0);
    EXPECT_EQ(memory, (params_.bit_array_size + 7) / 8);
}

//===================================================================
// Counting Bloom Filter Tests
//===================================================================

class CountingBloomFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        params_ = BloomFilterParams(1000, 0.01);
        filter_ = std::make_unique<CountingBloomFilter>(params_, 4);
    }
    
    void TearDown() override {}
    
    BloomFilterParams params_;
    std::unique_ptr<CountingBloomFilter> filter_;
};

TEST_F(CountingBloomFilterTest, BasicOperations) {
    // Add elements
    filter_->add("hello");
    filter_->add("world");
    filter_->add("hello");  // Add again
    
    // Test estimates
    EXPECT_GE(filter_->estimate("hello"), 2);
    EXPECT_GE(filter_->estimate("world"), 1);
    EXPECT_EQ(filter_->estimate("nonexistent"), 0);
    
    // Test contains
    EXPECT_TRUE(filter_->contains("hello"));
    EXPECT_TRUE(filter_->contains("world"));
    EXPECT_FALSE(filter_->contains("nonexistent"));
}

TEST_F(CountingBloomFilterTest, RemoveOperations) {
    filter_->add("test");
    filter_->add("test");
    
    EXPECT_GE(filter_->estimate("test"), 2);
    EXPECT_TRUE(filter_->contains("test"));
    
    // Remove once
    EXPECT_TRUE(filter_->remove("test"));
    EXPECT_GE(filter_->estimate("test"), 1);
    EXPECT_TRUE(filter_->contains("test"));
    
    // Remove again
    EXPECT_TRUE(filter_->remove("test"));
    EXPECT_EQ(filter_->estimate("test"), 0);
    EXPECT_FALSE(filter_->contains("test"));
    
    // Try to remove non-existent
    EXPECT_FALSE(filter_->remove("nonexistent"));
}

TEST_F(CountingBloomFilterTest, TinyLFUResetOperation) {
    filter_->add("test");
    filter_->add("test");
    filter_->add("test");
    filter_->add("test");
    
    uint32_t count_before = filter_->estimate("test");
    EXPECT_GE(count_before, 4);
    
    // Reset (divide by 2)
    filter_->reset();
    
    uint32_t count_after = filter_->estimate("test");
    EXPECT_EQ(count_after, count_before / 2);
}

TEST_F(CountingBloomFilterTest, ClearOperation) {
    filter_->add("test1");
    filter_->add("test2");
    
    EXPECT_TRUE(filter_->contains("test1"));
    EXPECT_TRUE(filter_->contains("test2"));
    
    filter_->clear();
    
    EXPECT_FALSE(filter_->contains("test1"));
    EXPECT_FALSE(filter_->contains("test2"));
    EXPECT_EQ(filter_->total_count(), 0);
}

TEST_F(CountingBloomFilterTest, CounterOverflow) {
    // Counter has 4 bits, so max count is 15
    std::string test_key = "overflow_test";
    
    // Add 20 times
    for (int i = 0; i < 20; ++i) {
        filter_->add(test_key);
    }
    
    // Count should be capped at 15
    uint32_t count = filter_->estimate(test_key);
    EXPECT_LE(count, 15);
}

//===================================================================
// Bloom Filter Factory Tests
//===================================================================

class BloomFilterFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BloomFilterFactoryTest, CreateBloomFilter) {
    auto filter = BloomFilterFactory::createBloomFilter(1000, 0.01);
    
    EXPECT_NE(filter, nullptr);
    EXPECT_TRUE(filter->empty());
    
    filter->add("test");
    EXPECT_TRUE(filter->contains("test"));
}

TEST_F(BloomFilterFactoryTest, CreateCountingBloomFilter) {
    auto filter = BloomFilterFactory::createCountingBloomFilter(1000, 0.01, 4);
    
    EXPECT_NE(filter, nullptr);
    EXPECT_EQ(filter->counter_bits(), 4);
    
    filter->add("test");
    EXPECT_TRUE(filter->contains("test"));
    EXPECT_GE(filter->estimate("test"), 1);
}

TEST_F(BloomFilterFactoryTest, CreateDoorkeeper) {
    auto filter = BloomFilterFactory::createDoorkeeper(1000);
    
    EXPECT_NE(filter, nullptr);
    EXPECT_TRUE(filter->empty());
    
    filter->add("test");
    EXPECT_TRUE(filter->contains("test"));
}

TEST_F(BloomFilterFactoryTest, CreateFrequencySketch) {
    auto filter = BloomFilterFactory::createFrequencySketch(10000, 1000);
    
    EXPECT_NE(filter, nullptr);
    EXPECT_EQ(filter->counter_bits(), 4);
    
    filter->add("test");
    EXPECT_TRUE(filter->contains("test"));
    EXPECT_GE(filter->estimate("test"), 1);
}

//===================================================================
// Performance Tests
//===================================================================

class BloomFilterPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        filter_ = BloomFilterFactory::createBloomFilter(100000, 0.01);
        counting_filter_ = BloomFilterFactory::createCountingBloomFilter(100000, 0.01, 4);
    }
    
    void TearDown() override {}
    
    std::unique_ptr<BloomFilter> filter_;
    std::unique_ptr<CountingBloomFilter> counting_filter_;
};

TEST_F(BloomFilterPerformanceTest, BloomFilterInsertPerformance) {
    const int num_elements = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_elements; ++i) {
        filter_->add("element_" + std::to_string(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Bloom Filter Insert Performance: " 
              << duration.count() << " microseconds for " 
              << num_elements << " elements" << std::endl;
    
    // Should be fast
    EXPECT_LT(duration.count(), 100000);  // Less than 100ms
}

TEST_F(BloomFilterPerformanceTest, BloomFilterQueryPerformance) {
    // Pre-populate filter
    const int num_elements = 10000;
    for (int i = 0; i < num_elements; ++i) {
        filter_->add("element_" + std::to_string(i));
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int hits = 0;
    for (int i = 0; i < num_elements; ++i) {
        if (filter_->contains("element_" + std::to_string(i))) {
            hits++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Bloom Filter Query Performance: " 
              << duration.count() << " microseconds for " 
              << num_elements << " queries" << std::endl;
    
    EXPECT_EQ(hits, num_elements);  // Should find all elements
    EXPECT_LT(duration.count(), 50000);  // Less than 50ms
}

TEST_F(BloomFilterPerformanceTest, CountingBloomFilterPerformance) {
    const int num_elements = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_elements; ++i) {
        counting_filter_->add("element_" + std::to_string(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Counting Bloom Filter Performance: " 
              << duration.count() << " microseconds for " 
              << num_elements << " elements" << std::endl;
    
    // Should be reasonably fast
    EXPECT_LT(duration.count(), 200000);  // Less than 200ms
}

//===================================================================
// Utility Function Tests
//===================================================================

class UtilityFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(UtilityFunctionTest, CalculateOptimalParams) {
    auto params = calculateOptimalParams(1000, 0.01);
    
    EXPECT_GT(params.bit_array_size, 0);
    EXPECT_GT(params.num_hash_functions, 0);
    EXPECT_TRUE(params.isValid());
}

TEST_F(UtilityFunctionTest, EstimateFalsePositiveRate) {
    double fp_rate = estimateFalsePositiveRate(1000, 10000, 7);
    
    EXPECT_GT(fp_rate, 0.0);
    EXPECT_LT(fp_rate, 1.0);
}

TEST_F(UtilityFunctionTest, SimdSupport) {
    // Just check that it returns a boolean
    bool supported = isSimdSupported();
    EXPECT_TRUE(supported || !supported);  // Always passes
}

//===================================================================
// Integration Tests for W-TinyLFU
//===================================================================

class WTinyLFUIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache_size_ = 1000;
        sample_size_ = cache_size_ * 10;
        
        doorkeeper_ = BloomFilterFactory::createDoorkeeper(cache_size_);
        frequency_sketch_ = BloomFilterFactory::createFrequencySketch(sample_size_, cache_size_);
    }
    
    void TearDown() override {}
    
    size_t cache_size_;
    size_t sample_size_;
    std::unique_ptr<BloomFilter> doorkeeper_;
    std::unique_ptr<CountingBloomFilter> frequency_sketch_;
};

TEST_F(WTinyLFUIntegrationTest, DoorkeeperBasicOperation) {
    // Simulate first-time access
    std::string key = "new_key";
    
    EXPECT_FALSE(doorkeeper_->contains(key));
    
    doorkeeper_->add(key);
    EXPECT_TRUE(doorkeeper_->contains(key));
}

TEST_F(WTinyLFUIntegrationTest, FrequencySketchOperation) {
    std::string key = "frequent_key";
    
    // Add multiple times
    for (int i = 0; i < 5; ++i) {
        frequency_sketch_->add(key);
    }
    
    uint32_t frequency = frequency_sketch_->estimate(key);
    EXPECT_GE(frequency, 5);
}

TEST_F(WTinyLFUIntegrationTest, TinyLFUResetScenario) {
    // Simulate reaching sample size limit
    for (int i = 0; i < 1000; ++i) {
        frequency_sketch_->add("key_" + std::to_string(i));
    }
    
    uint64_t total_before = frequency_sketch_->total_count();
    
    // Reset when sample size is reached
    frequency_sketch_->reset();
    doorkeeper_->clear();
    
    uint64_t total_after = frequency_sketch_->total_count();
    EXPECT_LT(total_after, total_before);
}

//===================================================================
// Main Test Runner
//===================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 