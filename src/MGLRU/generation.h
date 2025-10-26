/*
@Author: Lzww
@LastEditTime: 2025-10-26 21:15:36
@Description: MGLRU缓存实现
@Language: C++17
*/

#ifndef GENERATION_H
#define GENERATION_H

#include "access_tracker.h"

#include <list>
#include <memory>

class Generation {
public:
    Generation(size_t index, size_t tracker_size);
    ~Generation() = default;
    
    // Disable copy (contains non-copyable AccessTracker)
    Generation(const Generation&) = delete;
    Generation& operator=(const Generation&) = delete;
    
    // Enable move
    Generation(Generation&&) = default;
    Generation& operator=(Generation&&) = default;
    
    // add a page to the head of the current generation
    std::list<PageFrameId>::iterator add_page(PageFrameId page_id);

    // remove a page from the current generation
    void remove_page(std::list<PageFrameId>::iterator it);

    // get the access tracker of the current generation
    AccessTracker& get_tracker();

    // get the pages of the current generation
    // to scan asynchronously
    std::list<PageFrameId>& get_pages();

private:
    size_t index_;
    std::list<PageFrameId> pages_;
    AccessTracker tracker_;
};

#endif