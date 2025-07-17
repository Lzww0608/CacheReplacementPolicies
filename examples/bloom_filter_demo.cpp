/*
 * Bloom Filter Demo for W-TinyLFU Cache
 * 
 * This demo shows how to use:
 * - Standard Bloom Filter as Doorkeeper
 * - Counting Bloom Filter as Frequency Sketch
 * - TinyLFU reset operation
 * - Performance characteristics
 * 
 * Author: Generated for CRP Project
 * License: MIT
 */

#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include "../include/utils/bloom_filter.h"

using namespace crp::utils;

// Colors for output
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

void printHeader(const std::string& title) {
    std::cout << "\n" << CYAN << "========================================" << RESET << std::endl;
    std::cout << CYAN << title << RESET << std::endl;
    std::cout << CYAN << "========================================" << RESET << std::endl;
}

void printSubheader(const std::string& title) {
    std::cout << "\n" << YELLOW << "--- " << title << " ---" << RESET << std::endl;
}

void printSuccess(const std::string& message) {
    std::cout << GREEN << "✓ " << message << RESET << std::endl;
}

void printInfo(const std::string& message) {
    std::cout << BLUE << "ℹ " << message << RESET << std::endl;
}

void printWarning(const std::string& message) {
    std::cout << YELLOW << "⚠ " << message << RESET << std::endl;
}

void printError(const std::string& message) {
    std::cout << RED << "✗ " << message << RESET << std::endl;
}

// Demo 1: MurmurHash3 demonstration
void demonstrateMurmurHash3() {
    printHeader("MurmurHash3 Hash Function Demo");
    
    std::vector<std::string> test_strings = {
        "hello", "world", "bloom", "filter", "tinylfu", "cache"
    };
    
    printSubheader("32-bit Hash Results");
    for (const auto& str : test_strings) {
        uint32_t hash = MurmurHash3::hash32(str);
        std::cout << "Hash32(\"" << str << "\") = 0x" 
                  << std::hex << std::setfill('0') << std::setw(8) << hash 
                  << std::dec << std::endl;
    }
    
    printSubheader("128-bit Hash Results");
    for (const auto& str : test_strings) {
        auto hash = MurmurHash3::hash128(str);
        std::cout << "Hash128(\"" << str << "\") = 0x" 
                  << std::hex << std::setfill('0') << std::setw(16) << hash.h1 
                  << std::setw(16) << hash.h2 << std::dec << std::endl;
    }
    
    printSubheader("Consistency Check");
    std::string test_key = "consistency_test";
    uint32_t hash1 = MurmurHash3::hash32(test_key);
    uint32_t hash2 = MurmurHash3::hash32(test_key);
    
    if (hash1 == hash2) {
        printSuccess("Hash function is consistent across calls");
    } else {
        printError("Hash function is inconsistent!");
    }
}

// Demo 2: Standard Bloom Filter (Doorkeeper) demonstration
void demonstrateBloomFilter() {
    printHeader("Standard Bloom Filter (Doorkeeper) Demo");
    
    // Create a Doorkeeper for cache size 1000
    auto doorkeeper = BloomFilterFactory::createDoorkeeper(1000);
    
    printInfo("Created Doorkeeper Bloom Filter for cache size 1000");
    printInfo("Memory usage: " + std::to_string(doorkeeper->memory_usage()) + " bytes");
    printInfo("Hash functions: " + std::to_string(doorkeeper->num_hash_functions()));
    printInfo("Bit array size: " + std::to_string(doorkeeper->size()) + " bits");
    
    printSubheader("Adding Elements");
    std::vector<std::string> elements = {
        "user_123", "session_456", "product_789", "order_012", "payment_345"
    };
    
    for (const auto& elem : elements) {
        doorkeeper->add(elem);
        printSuccess("Added: " + elem);
    }
    
    printSubheader("Membership Test");
    for (const auto& elem : elements) {
        bool contains = doorkeeper->contains(elem);
        if (contains) {
            printSuccess("Found: " + elem);
        } else {
            printError("Missing: " + elem);
        }
    }
    
    printSubheader("False Positive Test");
    std::vector<std::string> non_existent = {
        "user_999", "session_888", "product_777", "order_666", "payment_555"
    };
    
    int false_positives = 0;
    for (const auto& elem : non_existent) {
        bool contains = doorkeeper->contains(elem);
        if (contains) {
            false_positives++;
            printWarning("False positive: " + elem);
        } else {
            printInfo("Correctly rejected: " + elem);
        }
    }
    
    double fp_rate = static_cast<double>(false_positives) / non_existent.size();
    printInfo("False positive rate: " + std::to_string(fp_rate * 100) + "%");
    
    printSubheader("Doorkeeper Reset");
    doorkeeper->clear();
    printSuccess("Doorkeeper cleared");
    
    bool still_contains = doorkeeper->contains(elements[0]);
    if (!still_contains) {
        printSuccess("Element correctly removed after clear");
    } else {
        printError("Element still present after clear!");
    }
}

// Demo 3: Counting Bloom Filter (Frequency Sketch) demonstration
void demonstrateCountingBloomFilter() {
    printHeader("Counting Bloom Filter (Frequency Sketch) Demo");
    
    // Create a frequency sketch for sample size 10000, cache size 1000
    auto frequency_sketch = BloomFilterFactory::createFrequencySketch(10000, 1000);
    
    printInfo("Created Frequency Sketch for sample size 10000, cache size 1000");
    printInfo("Memory usage: " + std::to_string(frequency_sketch->memory_usage()) + " bytes");
    printInfo("Counter bits: " + std::to_string(frequency_sketch->counter_bits()));
    printInfo("Max count: " + std::to_string(frequency_sketch->max_count()));
    
    printSubheader("Adding Elements with Different Frequencies");
    std::vector<std::pair<std::string, int>> elements = {
        {"hot_key_1", 10},
        {"hot_key_2", 8},
        {"warm_key_1", 5},
        {"warm_key_2", 3},
        {"cold_key_1", 1},
        {"cold_key_2", 1}
    };
    
    for (const auto& [key, frequency] : elements) {
        for (int i = 0; i < frequency; ++i) {
            frequency_sketch->add(key);
        }
        printSuccess("Added '" + key + "' " + std::to_string(frequency) + " times");
    }
    
    printSubheader("Frequency Estimation");
    for (const auto& [key, expected_freq] : elements) {
        uint32_t estimated_freq = frequency_sketch->estimate(key);
        std::cout << "Key: " << key 
                  << ", Expected: " << expected_freq 
                  << ", Estimated: " << estimated_freq;
        
        if (estimated_freq >= expected_freq) {
            std::cout << GREEN << " ✓" << RESET << std::endl;
        } else {
            std::cout << RED << " ✗" << RESET << std::endl;
        }
    }
    
    printSubheader("Non-existent Key Test");
    uint32_t non_existent_freq = frequency_sketch->estimate("non_existent_key");
    if (non_existent_freq == 0) {
        printSuccess("Non-existent key correctly has 0 frequency");
    } else {
        printWarning("Non-existent key has frequency: " + std::to_string(non_existent_freq));
    }
    
    printSubheader("TinyLFU Reset Operation");
    uint64_t total_before = frequency_sketch->total_count();
    printInfo("Total count before reset: " + std::to_string(total_before));
    
    frequency_sketch->reset();
    printSuccess("Performed TinyLFU reset (divide all counters by 2)");
    
    uint64_t total_after = frequency_sketch->total_count();
    printInfo("Total count after reset: " + std::to_string(total_after));
    
    printSubheader("Frequency After Reset");
    for (const auto& [key, expected_freq] : elements) {
        uint32_t estimated_freq = frequency_sketch->estimate(key);
        std::cout << "Key: " << key 
                  << ", Original: " << expected_freq 
                  << ", After reset: " << estimated_freq << std::endl;
    }
}

// Demo 4: W-TinyLFU Integration demonstration
void demonstrateWTinyLFUIntegration() {
    printHeader("W-TinyLFU Integration Demo");
    
    const size_t cache_size = 1000;
    const size_t sample_size = cache_size * 10;
    
    // Create components
    auto doorkeeper = BloomFilterFactory::createDoorkeeper(cache_size);
    auto frequency_sketch = BloomFilterFactory::createFrequencySketch(sample_size, cache_size);
    
    printInfo("Created W-TinyLFU components:");
    printInfo("  - Doorkeeper (cache size: " + std::to_string(cache_size) + ")");
    printInfo("  - Frequency Sketch (sample size: " + std::to_string(sample_size) + ")");
    
    printSubheader("Simulating Cache Access Pattern");
    
    // Simulate access pattern
    std::vector<std::string> keys = {
        "key_1", "key_2", "key_3", "key_4", "key_5"
    };
    
    auto simulateAccess = [&](const std::string& key) {
        std::cout << "\nAccessing key: " << key << std::endl;
        
        // Check if in doorkeeper (first time access)
        if (!doorkeeper->contains(key)) {
            printInfo("  First time access - adding to doorkeeper");
            doorkeeper->add(key);
        } else {
            printInfo("  Repeat access - adding to frequency sketch");
            frequency_sketch->add(key);
        }
        
        uint32_t frequency = frequency_sketch->estimate(key);
        printInfo("  Estimated frequency: " + std::to_string(frequency));
    };
    
    // Simulate different access patterns
    printSubheader("First Access to All Keys");
    for (const auto& key : keys) {
        simulateAccess(key);
    }
    
    printSubheader("Repeated Access to Some Keys");
    for (int i = 0; i < 3; ++i) {
        simulateAccess("key_1");  // Hot key
        simulateAccess("key_2");  // Hot key
    }
    
    simulateAccess("key_3");  // Warm key
    
    printSubheader("Final Frequency Estimates");
    for (const auto& key : keys) {
        uint32_t frequency = frequency_sketch->estimate(key);
        std::cout << "Key: " << key << ", Frequency: " << frequency << std::endl;
    }
    
    printSubheader("Simulating Sample Size Limit Reached");
    // Simulate reaching sample size limit
    for (int i = 0; i < 1000; ++i) {
        frequency_sketch->add("bulk_key_" + std::to_string(i));
    }
    
    uint64_t total_before = frequency_sketch->total_count();
    printInfo("Total count before reset: " + std::to_string(total_before));
    
    // Reset when sample size is reached
    frequency_sketch->reset();
    doorkeeper->clear();
    printSuccess("Performed aging: reset frequency sketch and clear doorkeeper");
    
    uint64_t total_after = frequency_sketch->total_count();
    printInfo("Total count after reset: " + std::to_string(total_after));
}

// Demo 5: Performance benchmark
void demonstratePerformance() {
    printHeader("Performance Benchmark");
    
    const int num_elements = 100000;
    const int num_queries = 50000;
    
    // Create filters
    auto bloom_filter = BloomFilterFactory::createBloomFilter(num_elements, 0.01);
    auto counting_filter = BloomFilterFactory::createCountingBloomFilter(num_elements, 0.01, 4);
    
    printSubheader("Standard Bloom Filter Performance");
    
    // Measure insert performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_elements; ++i) {
        bloom_filter->add("key_" + std::to_string(i));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double insert_throughput = static_cast<double>(num_elements) / insert_duration.count() * 1000000;
    printInfo("Insert throughput: " + std::to_string(static_cast<int>(insert_throughput)) + " ops/sec");
    
    // Measure query performance
    start = std::chrono::high_resolution_clock::now();
    int hits = 0;
    for (int i = 0; i < num_queries; ++i) {
        if (bloom_filter->contains("key_" + std::to_string(i))) {
            hits++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto query_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double query_throughput = static_cast<double>(num_queries) / query_duration.count() * 1000000;
    printInfo("Query throughput: " + std::to_string(static_cast<int>(query_throughput)) + " ops/sec");
    printInfo("Hit rate: " + std::to_string(static_cast<double>(hits) / num_queries * 100) + "%");
    
    printSubheader("Counting Bloom Filter Performance");
    
    // Measure insert performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_elements; ++i) {
        counting_filter->add("key_" + std::to_string(i));
    }
    end = std::chrono::high_resolution_clock::now();
    insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    insert_throughput = static_cast<double>(num_elements) / insert_duration.count() * 1000000;
    printInfo("Insert throughput: " + std::to_string(static_cast<int>(insert_throughput)) + " ops/sec");
    
    // Measure estimate performance
    start = std::chrono::high_resolution_clock::now();
    uint64_t total_estimates = 0;
    for (int i = 0; i < num_queries; ++i) {
        total_estimates += counting_filter->estimate("key_" + std::to_string(i));
    }
    end = std::chrono::high_resolution_clock::now();
    query_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    query_throughput = static_cast<double>(num_queries) / query_duration.count() * 1000000;
    printInfo("Estimate throughput: " + std::to_string(static_cast<int>(query_throughput)) + " ops/sec");
    printInfo("Average estimate: " + std::to_string(static_cast<double>(total_estimates) / num_queries));
    
    printSubheader("Memory Usage Comparison");
    printInfo("Bloom Filter memory: " + std::to_string(bloom_filter->memory_usage()) + " bytes");
    printInfo("Counting Filter memory: " + std::to_string(counting_filter->memory_usage()) + " bytes");
    
    double memory_ratio = static_cast<double>(counting_filter->memory_usage()) / bloom_filter->memory_usage();
    printInfo("Memory overhead ratio: " + std::to_string(memory_ratio) + "x");
}

int main() {
    std::cout << MAGENTA << R"(
    ╔══════════════════════════════════════════════════════════════╗
    ║                                                              ║
    ║        High-Performance Bloom Filter for W-TinyLFU          ║
    ║                       Demonstration                          ║
    ║                                                              ║
    ╚══════════════════════════════════════════════════════════════╝
    )" << RESET << std::endl;
    
    try {
        demonstrateMurmurHash3();
        demonstrateBloomFilter();
        demonstrateCountingBloomFilter();
        demonstrateWTinyLFUIntegration();
        demonstratePerformance();
        
        printHeader("Demo Complete");
        printSuccess("All demonstrations completed successfully!");
        printInfo("The Bloom Filter implementation is ready for use in W-TinyLFU cache.");
        
    } catch (const std::exception& e) {
        printError("Demo failed with exception: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
} 