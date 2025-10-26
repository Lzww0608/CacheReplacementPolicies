/*
@Author: Lzww
@LastEditTime: 2025-10-26 22:01:58
@Description: MGLRU Memory Manager Implementation
@Language: C++17
*/

#include "memory_manager.h"
#include <algorithm>

MemoryManager::MemoryManager(size_t capacity, size_t num_generations)
    : capacity_(capacity),
      high_watermark_(capacity * 90 / 100),  // 90% capacity
      min_watermark_(capacity * 70 / 100),   // 70% capacity
      mglru_(num_generations, 64) {
    
    // Initialize physical frame pool
    physical_frames_list_.reserve(capacity);
    for (size_t i = 0; i < capacity; ++i) {
        physical_frames_list_.push_back(i);
        free_list_.push_back(i);
    }
    
    // Start background reclaim thread
    reclaim_thread_ = std::thread(&MemoryManager::reclaimed_loop, this);
}

MemoryManager::~MemoryManager() {
    // Stop the reclaim thread
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_thread_ = true;
    }
    cv_.notify_all();
    
    if (reclaim_thread_.joinable()) {
        reclaim_thread_.join();
    }
}

std::optional<PageFrameId> MemoryManager::allocate_page() {
    std::unique_lock<std::mutex> lock(mtx_);
    
    // If no free pages, try direct reclaim
    if (free_list_.empty()) {
        lock.unlock();
        direct_reclaim(10);  // Try to reclaim 10 pages
        lock.lock();
        
        if (free_list_.empty()) {
            return std::nullopt;  // OOM
        }
    }
    
    // Allocate from free list
    PageFrameId page_id = free_list_.front();
    free_list_.pop_front();
    
    // Add to MGLRU (it will manage the metadata internally)
    mglru_.on_page_added(page_id);
    
    // Wake up background reclaim if above high watermark
    size_t used = capacity_ - free_list_.size();
    if (used > high_watermark_) {
        cv_.notify_one();
    }
    
    return page_id;
}

void MemoryManager::access_page(PageFrameId page_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // Mark as accessed in MGLRU
    // MGLRU will check internally if page exists
    mglru_.on_page_accessed(page_id);
}

void MemoryManager::free_page(PageFrameId page_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // For explicit free, we need to get metadata from MGLRU
    // For now, just add back to free list
    // MGLRU will handle removal during reclaim
    add_to_free_list(page_id);
}

double MemoryManager::get_memory_usage() const {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t used = capacity_ - free_list_.size();
    return static_cast<double>(used) / static_cast<double>(capacity_);
}

void MemoryManager::reclaimed_loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        // Wait until we need to reclaim or thread is stopped
        cv_.wait(lock, [this] {
            size_t used = capacity_ - free_list_.size();
            return stop_thread_ || used > high_watermark_;
        });
        
        if (stop_thread_) {
            break;
        }
        
        // Reclaim pages until below min watermark
        size_t used = capacity_ - free_list_.size();
        if (used > min_watermark_) {
            size_t target_reclaim = used - min_watermark_;
            
            // Unlock while scanning to avoid holding lock too long
            lock.unlock();
            
            // Scan and reclaim pages
            ReclaimResult result = mglru_.scan_and_reclaim(target_reclaim);
            
            // Relock and add evicted pages to free list
            lock.lock();
            for (auto page_id : result.evicted_pages) {
                add_to_free_list(page_id);
            }
        }
    }
}

size_t MemoryManager::direct_reclaim(size_t pages_to_reclaim) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // Scan and reclaim
    ReclaimResult result = mglru_.scan_and_reclaim(pages_to_reclaim);
    
    // Add evicted pages to free list
    for (auto page_id : result.evicted_pages) {
        add_to_free_list(page_id);
    }
    
    return result.evicted_pages.size();
}

void MemoryManager::add_to_free_list(PageFrameId page_id) {
    free_list_.push_back(page_id);
}

