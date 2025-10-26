/*
@Author: Lzww
@LastEditTime: 2025-10-26 21:58:12
@Description: MGLRU Memory Manager Implementation
@Language: C++17
*/

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "mglru_core.h"

#include <cstddef>
#include <vector>
#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <optional>
#include <unordered_map>

class MemoryManager {
public:
    MemoryManager(size_t capacity, size_t num_generations);
    ~MemoryManager();

    // emulate the physical page frame allocation
    // may be blocked if it's full
    std::optional<PageFrameId> allocate_page();

    // emulate the physical page frame access
    void access_page(PageFrameId page_id);

    // emulate the physical page frame free
    void free_page(PageFrameId page_id);

    double get_memory_usage() const;

private:

    // async reclaimed loop(emulate kswapd)
    void reclaimed_loop();

    // sync reclaimed loop
    size_t direct_reclaim(size_t pages_to_reclaim);

    void add_to_free_list(PageFrameId page_id);

    const size_t capacity_;
    const size_t high_watermark_;
    const size_t min_watermark_;

    mutable std::mutex mtx_;  // mutable for const member functions
    MGLRU mglru_;

    // emulate the physical page frame pool
    std::vector<PageFrameId> physical_frames_list_;
    // track all free pages
    std::list<PageFrameId> free_list_;

    // background reclaim thread (emulate kswapd)
    std::thread reclaim_thread_;
    std::condition_variable cv_;
    bool stop_thread_ = false;
};

#endif
