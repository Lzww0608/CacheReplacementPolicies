/*
@Author: Lzww
@LastEditTime: 2025-10-26 21:59:40
@Description: MGLRU Core Implementation
@Language: C++17
*/

#include "mglru_core.h"

MGLRU::MGLRU(size_t num_generations, size_t tracker_size_per_gen)
    : pid_controller_(0.1, 0.01, 0.05), max_generations_(num_generations) {
    
    // Initialize generations (from youngest to oldest)
    for (size_t i = 0; i < num_generations; ++i) {
        generations_.emplace_back(i, tracker_size_per_gen);
    }
}

void MGLRU::on_page_added(PageFrameId page_id) {
    // New page starts at the youngest generation (generation 0)
    auto it = generations_[0].add_page(page_id);
    
    PageMetadata metadata;
    metadata.id = page_id;
    metadata.generation_index = 0;
    metadata.lru_iterator = it;
    
    page_table_[page_id] = metadata;
}

void MGLRU::on_page_accessed(PageFrameId page_id) {
    // Mark the page as accessed in its generation's tracker
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        size_t gen_idx = it->second.generation_index;
        generations_[gen_idx].get_tracker().set(page_id);
    }
}

ReclaimResult MGLRU::scan_and_reclaim(size_t pages_to_scan) {
    ReclaimResult result;
    result.promoted_pages_count = 0;
    result.scanned_pages_count = 0;
    
    // Start scanning from the oldest generation
    size_t oldest_gen_idx = max_generations_ - 1;
    auto& oldest_gen = generations_[oldest_gen_idx];
    auto& pages = oldest_gen.get_pages();
    
    if (pages.empty()) {
        // No pages in oldest generation, age them
        age_generations();
        return result;
    }
    
    // Collect pages to scan first (to avoid iterator invalidation)
    std::vector<PageFrameId> pages_to_process;
    size_t scanned = 0;
    
    for (auto rit = pages.rbegin(); rit != pages.rend() && scanned < pages_to_scan; ++rit) {
        pages_to_process.push_back(*rit);
        ++scanned;
    }
    
    // Process each page
    for (auto page_id : pages_to_process) {
        auto meta_it = page_table_.find(page_id);
        if (meta_it == page_table_.end()) {
            continue;
        }
        
        PageMetadata& meta = meta_it->second;
        
        // Check if page was accessed
        if (oldest_gen.get_tracker().check_and_clear(page_id)) {
            // Page was accessed, promote it
            promote_page(meta);
            result.promoted_pages_count++;
        } else {
            // Page was not accessed, evict it
            oldest_gen.remove_page(meta.lru_iterator);
            page_table_.erase(meta_it);
            result.evicted_pages.push_back(page_id);
        }
    }
    
    result.scanned_pages_count = scanned;
    
    // Age generations if oldest generation is getting small
    if (pages.size() < pages_to_scan / 4) {
        age_generations();
    }
    
    return result;
}

void MGLRU::on_page_removed(const PageMetadata& page_meta) {
    // Remove page from its generation
    size_t gen_idx = page_meta.generation_index;
    generations_[gen_idx].remove_page(page_meta.lru_iterator);
    
    // Remove from page table
    page_table_.erase(page_meta.id);
}

void MGLRU::age_generations() {
    // Move all pages from younger generations to older generations
    // This simulates time passing
    
    // Start from the oldest generation
    size_t oldest_idx = max_generations_ - 1;
    
    // Age from second oldest to youngest
    for (int i = oldest_idx; i >= 1; --i) {
        auto& younger_gen = generations_[i - 1];
        auto& older_gen = generations_[i];
        auto& younger_pages = younger_gen.get_pages();
        
        // Collect pages first to avoid issues
        std::vector<PageFrameId> pages_to_move;
        for (auto page_id : younger_pages) {
            pages_to_move.push_back(page_id);
        }
        
        // Move all pages from younger to older generation
        for (auto page_id : pages_to_move) {
            auto it = page_table_.find(page_id);
            if (it != page_table_.end()) {
                // Remove from younger generation first
                younger_gen.remove_page(it->second.lru_iterator);
                
                // Add to older generation
                auto new_it = older_gen.add_page(page_id);
                
                // Update metadata
                it->second.generation_index = i;
                it->second.lru_iterator = new_it;
            }
        }
        
        // Clear younger generation tracker
        younger_gen.get_tracker().clear();
    }
    
    // Clear oldest generation's tracker
    generations_[oldest_idx].get_tracker().clear();
}

void MGLRU::promote_page(PageMetadata& page_meta) {
    size_t current_gen = page_meta.generation_index;
    
    // Can't promote if already in youngest generation
    if (current_gen == 0) {
        return;
    }
    
    // Remove from current generation
    generations_[current_gen].remove_page(page_meta.lru_iterator);
    
    // Add to younger generation (current - 1)
    size_t new_gen = current_gen - 1;
    auto new_it = generations_[new_gen].add_page(page_meta.id);
    
    // Update metadata
    page_meta.generation_index = new_gen;
    page_meta.lru_iterator = new_it;
}

