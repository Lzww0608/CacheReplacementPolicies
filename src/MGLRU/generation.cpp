/*
@Author: Lzww
@LastEditTime: 2025-10-26 21:57:07
@Description: MGLRU Generation Implementation
@Language: C++17
*/

#include "generation.h"

Generation::Generation(size_t index, size_t tracker_size) 
    : index_(index), tracker_(tracker_size) {
}

std::list<PageFrameId>::iterator Generation::add_page(PageFrameId page_id) {
    // add page to the head (most recently used)
    pages_.push_front(page_id);
    return pages_.begin();
}

void Generation::remove_page(std::list<PageFrameId>::iterator it) {
    pages_.erase(it);
}

AccessTracker& Generation::get_tracker() {
    return tracker_;
}

std::list<PageFrameId>& Generation::get_pages() {
    return pages_;
}

