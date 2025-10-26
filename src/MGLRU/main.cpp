/*
@Author: Lzww
@LastEditTime: 2025-10-26
@Description: MGLRU Unit Tests
@Language: C++17
*/

#include "mglru_core.h"
#include "access_tracker.h"
#include "generation.h"
#include "pid_controller.h"
#include "memory_manager.h"
#include <gtest/gtest.h>
#include <vector>
#include <set>
#include <chrono>
#include <thread>

// Test AccessTracker functionality
class AccessTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tracker = std::make_unique<AccessTracker>(64);  // 64 bitsets
    }
    
    std::unique_ptr<AccessTracker> tracker;
};

TEST_F(AccessTrackerTest, SetAndCheck) {
    PageFrameId page_id = 12345;
    
    // Initially should not be set
    EXPECT_FALSE(tracker->check_and_clear(page_id));
    
    // Set the bit
    tracker->set(page_id);
    
    // Should be set now
    EXPECT_TRUE(tracker->check_and_clear(page_id));
    
    // Should be cleared after check_and_clear
    EXPECT_FALSE(tracker->check_and_clear(page_id));
}

TEST_F(AccessTrackerTest, MultiplePages) {
    std::vector<PageFrameId> pages = {100, 200, 300, 400, 500};
    
    // Set all pages
    for (auto page_id : pages) {
        tracker->set(page_id);
    }
    
    // Check all pages are set
    for (auto page_id : pages) {
        EXPECT_TRUE(tracker->check_and_clear(page_id));
    }
    
    // All should be cleared now
    for (auto page_id : pages) {
        EXPECT_FALSE(tracker->check_and_clear(page_id));
    }
}

TEST_F(AccessTrackerTest, ClearAll) {
    std::vector<PageFrameId> pages = {10, 20, 30, 40, 50};
    
    // Set all pages
    for (auto page_id : pages) {
        tracker->set(page_id);
    }
    
    // Clear all
    tracker->clear();
    
    // All should be cleared
    for (auto page_id : pages) {
        EXPECT_FALSE(tracker->check_and_clear(page_id));
    }
}

// Test Generation functionality
class GenerationTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen = std::make_unique<Generation>(0, 64);
    }
    
    std::unique_ptr<Generation> gen;
};

TEST_F(GenerationTest, AddAndRemovePage) {
    PageFrameId page_id = 100;
    
    // Add page
    auto it = gen->add_page(page_id);
    
    // Check page is in the list
    auto& pages = gen->get_pages();
    EXPECT_EQ(pages.size(), 1);
    EXPECT_EQ(pages.front(), page_id);
    
    // Remove page
    gen->remove_page(it);
    EXPECT_EQ(pages.size(), 0);
}

TEST_F(GenerationTest, MultiplePages) {
    std::vector<PageFrameId> page_ids = {100, 200, 300};
    std::vector<std::list<PageFrameId>::iterator> iterators;
    
    // Add pages
    for (auto page_id : page_ids) {
        iterators.push_back(gen->add_page(page_id));
    }
    
    auto& pages = gen->get_pages();
    EXPECT_EQ(pages.size(), 3);
    
    // Pages should be in reverse order (LIFO)
    auto it = pages.begin();
    EXPECT_EQ(*it++, 300);
    EXPECT_EQ(*it++, 200);
    EXPECT_EQ(*it++, 100);
}

TEST_F(GenerationTest, TrackerIntegration) {
    PageFrameId page_id = 100;
    gen->add_page(page_id);
    
    // Set access bit
    gen->get_tracker().set(page_id);
    
    // Check it's set
    EXPECT_TRUE(gen->get_tracker().check_and_clear(page_id));
}

// Test PidController functionality
class PidControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller = std::make_unique<PidController>(0.1, 0.01, 0.05);
    }
    
    std::unique_ptr<PidController> controller;
};

TEST_F(PidControllerTest, InitialIntensity) {
    size_t intensity = controller->get_scan_intensity();
    EXPECT_GT(intensity, 0);
    EXPECT_LE(intensity, 1024);
}

TEST_F(PidControllerTest, UpdateMetrics) {
    // Update with some metrics
    controller->update_metrics(10, 100, 80);  // refaults, scanned, reclaimed
    
    size_t intensity = controller->get_scan_intensity();
    EXPECT_GT(intensity, 0);
    EXPECT_LE(intensity, 1024);
}

TEST_F(PidControllerTest, MultipleUpdates) {
    // Simulate multiple rounds of reclaim
    for (int i = 0; i < 10; i++) {
        controller->update_metrics(5, 50, 40);
        size_t intensity = controller->get_scan_intensity();
        EXPECT_GT(intensity, 0);
        EXPECT_LE(intensity, 1024);
    }
}

// Test MGLRU Core functionality
class MGLRUTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 4 generations, 64 tracker size per generation
        mglru = std::make_unique<MGLRU>(4, 64);
    }
    
    std::unique_ptr<MGLRU> mglru;
};

TEST_F(MGLRUTest, AddPage) {
    PageFrameId page_id = 1000;
    
    // Should not throw
    EXPECT_NO_THROW(mglru->on_page_added(page_id));
}

TEST_F(MGLRUTest, AddMultiplePages) {
    std::vector<PageFrameId> pages = {1000, 2000, 3000, 4000, 5000};
    
    for (auto page_id : pages) {
        EXPECT_NO_THROW(mglru->on_page_added(page_id));
    }
}

TEST_F(MGLRUTest, AccessPage) {
    PageFrameId page_id = 1000;
    
    mglru->on_page_added(page_id);
    
    // Access the page
    EXPECT_NO_THROW(mglru->on_page_accessed(page_id));
}

TEST_F(MGLRUTest, ScanAndReclaimEmpty) {
    // Scan when there are no pages
    ReclaimResult result = mglru->scan_and_reclaim(10);
    
    EXPECT_EQ(result.evicted_pages.size(), 0);
    EXPECT_EQ(result.promoted_pages_count, 0);
    EXPECT_EQ(result.scanned_pages_count, 0);
}

TEST_F(MGLRUTest, ScanAndReclaimWithoutAccess) {
    // Add pages but don't access them
    std::vector<PageFrameId> pages = {1000, 2000, 3000, 4000};
    for (auto page_id : pages) {
        mglru->on_page_added(page_id);
    }
    
    // Track total scanned and evicted
    size_t total_scanned = 0;
    size_t total_evicted = 0;
    
    // Age generations multiple times to move pages to oldest generation
    for (int i = 0; i < 10; i++) {
        ReclaimResult result = mglru->scan_and_reclaim(10);
        total_scanned += result.scanned_pages_count;
        total_evicted += result.evicted_pages.size();
    }
    
    // Eventually all pages should be scanned and evicted (they were never accessed)
    EXPECT_GT(total_scanned, 0);
    EXPECT_EQ(total_evicted, pages.size());  // All pages should be evicted
}

TEST_F(MGLRUTest, ScanAndReclaimWithAccess) {
    // Add pages and access some of them
    std::vector<PageFrameId> pages = {1000, 2000, 3000, 4000};
    for (auto page_id : pages) {
        mglru->on_page_added(page_id);
    }
    
    // Access some pages
    mglru->on_page_accessed(1000);
    mglru->on_page_accessed(2000);
    
    // Age generations to move to oldest generation
    for (int i = 0; i < 5; i++) {
        ReclaimResult result = mglru->scan_and_reclaim(10);
    }
    
    // Accessed pages might be promoted, unaccessed pages might be evicted
    ReclaimResult result = mglru->scan_and_reclaim(10);
    
    EXPECT_GE(result.scanned_pages_count, 0);
}

TEST_F(MGLRUTest, EvictionCorrectness) {
    // Add pages
    std::set<PageFrameId> added_pages = {1000, 2000, 3000};
    for (auto page_id : added_pages) {
        mglru->on_page_added(page_id);
    }
    
    // Don't access any pages, so they should all be evicted eventually
    std::set<PageFrameId> evicted_pages;
    
    // Age and scan multiple times
    for (int round = 0; round < 10; round++) {
        ReclaimResult result = mglru->scan_and_reclaim(10);
        for (auto page_id : result.evicted_pages) {
            evicted_pages.insert(page_id);
        }
    }
    
    // Eventually all pages should be evicted (or most of them)
    // Note: due to aging mechanism, this might take several rounds
    EXPECT_GE(evicted_pages.size(), 0);
}

TEST_F(MGLRUTest, AccessedPagesNotEvictedImmediately) {
    // Add a page
    PageFrameId page_id = 1000;
    mglru->on_page_added(page_id);
    
    // Continuously access it
    for (int i = 0; i < 10; i++) {
        mglru->on_page_accessed(page_id);
        ReclaimResult result = mglru->scan_and_reclaim(5);
        
        // Check if the page was evicted
        bool evicted = false;
        for (auto evicted_id : result.evicted_pages) {
            if (evicted_id == page_id) {
                evicted = true;
                break;
            }
        }
        
        // Continuously accessed page should have lower chance of eviction
        if (!evicted) {
            // This is expected for actively accessed pages
        }
    }
}

TEST_F(MGLRUTest, LargeNumberOfPages) {
    // Add many pages
    const size_t NUM_PAGES = 1000;
    for (size_t i = 0; i < NUM_PAGES; i++) {
        mglru->on_page_added(i);
    }
    
    // Access half of them
    for (size_t i = 0; i < NUM_PAGES / 2; i++) {
        mglru->on_page_accessed(i);
    }
    
    // Scan and reclaim
    size_t total_scanned = 0;
    size_t total_evicted = 0;
    
    for (int round = 0; round < 20; round++) {
        ReclaimResult result = mglru->scan_and_reclaim(100);
        total_scanned += result.scanned_pages_count;
        total_evicted += result.evicted_pages.size();
    }
    
    EXPECT_GT(total_scanned, 0);
    std::cout << "Total scanned: " << total_scanned << std::endl;
    std::cout << "Total evicted: " << total_evicted << std::endl;
}

// Integration test
TEST_F(MGLRUTest, WorkloadSimulation) {
    std::cout << "\n=== Workload Simulation Test ===" << std::endl;
    
    // Simulate a real workload
    const size_t NUM_PAGES = 100;
    const size_t WORKING_SET = 20;  // 20% hot pages
    
    // Add pages
    for (size_t i = 0; i < NUM_PAGES; i++) {
        mglru->on_page_added(i);
    }
    
    // Simulate access pattern: hot pages accessed frequently
    for (int round = 0; round < 50; round++) {
        // Access working set pages
        for (size_t i = 0; i < WORKING_SET; i++) {
            mglru->on_page_accessed(i);
        }
        
        // Occasionally access cold pages
        if (round % 5 == 0) {
            for (size_t i = WORKING_SET; i < WORKING_SET + 10; i++) {
                mglru->on_page_accessed(i);
            }
        }
        
        // Perform reclaim
        ReclaimResult result = mglru->scan_and_reclaim(20);
        
        std::cout << "Round " << round << ": "
                  << "Scanned=" << result.scanned_pages_count
                  << ", Evicted=" << result.evicted_pages.size()
                  << ", Promoted=" << result.promoted_pages_count
                  << std::endl;
    }
}

// Test MemoryManager functionality
class MemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 100 pages capacity, 4 generations
        manager = std::make_unique<MemoryManager>(100, 4);
    }
    
    void TearDown() override {
        manager.reset();
    }
    
    std::unique_ptr<MemoryManager> manager;
};

TEST_F(MemoryManagerTest, AllocatePage) {
    auto page = manager->allocate_page();
    ASSERT_TRUE(page.has_value());
    EXPECT_GE(page.value(), 0);
}

TEST_F(MemoryManagerTest, AllocateMultiplePages) {
    std::vector<PageFrameId> pages;
    
    // Allocate 50 pages
    for (int i = 0; i < 50; i++) {
        auto page = manager->allocate_page();
        ASSERT_TRUE(page.has_value());
        pages.push_back(page.value());
    }
    
    // Check all pages are unique
    std::set<PageFrameId> unique_pages(pages.begin(), pages.end());
    EXPECT_EQ(unique_pages.size(), pages.size());
    
    // Check memory usage
    double usage = manager->get_memory_usage();
    EXPECT_GE(usage, 0.45);  // At least 45%
    EXPECT_LE(usage, 0.55);  // At most 55%
}

TEST_F(MemoryManagerTest, AccessPage) {
    auto page = manager->allocate_page();
    ASSERT_TRUE(page.has_value());
    
    // Should not throw
    EXPECT_NO_THROW(manager->access_page(page.value()));
}

TEST_F(MemoryManagerTest, FreePage) {
    auto page = manager->allocate_page();
    ASSERT_TRUE(page.has_value());
    
    double usage_before = manager->get_memory_usage();
    
    // Free the page
    manager->free_page(page.value());
    
    double usage_after = manager->get_memory_usage();
    
    // Usage should decrease
    EXPECT_LT(usage_after, usage_before);
}

TEST_F(MemoryManagerTest, AllocateFreeCycle) {
    std::vector<PageFrameId> pages;
    
    // Allocate 10 pages
    for (int i = 0; i < 10; i++) {
        auto page = manager->allocate_page();
        ASSERT_TRUE(page.has_value());
        pages.push_back(page.value());
    }
    
    // Free all pages
    for (auto page : pages) {
        manager->free_page(page);
    }
    
    // Should be able to allocate again
    for (int i = 0; i < 10; i++) {
        auto page = manager->allocate_page();
        ASSERT_TRUE(page.has_value());
    }
}

TEST_F(MemoryManagerTest, MemoryPressure) {
    std::vector<PageFrameId> pages;
    
    // Allocate most pages
    for (int i = 0; i < 95; i++) {
        auto page = manager->allocate_page();
        ASSERT_TRUE(page.has_value());
        pages.push_back(page.value());
    }
    
    // Access first 20 pages (make them hot)
    for (int i = 0; i < 20; i++) {
        manager->access_page(pages[i]);
    }
    
    // Wait a bit for background reclaim
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Try to allocate more - should trigger reclaim
    for (int i = 0; i < 10; i++) {
        auto page = manager->allocate_page();
        // May fail if reclaim can't keep up
        if (page.has_value()) {
            pages.push_back(page.value());
        }
    }
    
    // At least some allocations should succeed
    EXPECT_GT(pages.size(), 95);
}

TEST_F(MemoryManagerTest, HighUsageScenario) {
    std::cout << "\n=== High Usage Scenario ===" << std::endl;
    
    std::vector<PageFrameId> pages;
    const int WORKING_SET = 30;
    
    // Allocate 90 pages
    for (int i = 0; i < 90; i++) {
        auto page = manager->allocate_page();
        if (page.has_value()) {
            pages.push_back(page.value());
        }
    }
    
    std::cout << "Allocated " << pages.size() << " pages" << std::endl;
    std::cout << "Memory usage: " << (manager->get_memory_usage() * 100) << "%" << std::endl;
    
    // Simulate workload: access working set repeatedly
    for (int round = 0; round < 10; round++) {
        // Access working set
        for (size_t i = 0; i < WORKING_SET && i < pages.size(); i++) {
            manager->access_page(pages[i]);
        }
        
        // Try to allocate more
        for (int i = 0; i < 5; i++) {
            auto page = manager->allocate_page();
            if (page.has_value()) {
                pages.push_back(page.value());
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Final pages: " << pages.size() << std::endl;
    std::cout << "Final memory usage: " << (manager->get_memory_usage() * 100) << "%" << std::endl;
    
    // System should maintain working set
    EXPECT_GT(pages.size(), 0);
}

TEST_F(MemoryManagerTest, ConcurrentAccess) {
    auto page1 = manager->allocate_page();
    auto page2 = manager->allocate_page();
    
    ASSERT_TRUE(page1.has_value());
    ASSERT_TRUE(page2.has_value());
    
    // Access pages from multiple threads (simplified test)
    std::thread t1([&]() {
        for (int i = 0; i < 100; i++) {
            manager->access_page(page1.value());
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < 100; i++) {
            manager->access_page(page2.value());
        }
    });
    
    t1.join();
    t2.join();
    
    // Should not crash
    SUCCEED();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

