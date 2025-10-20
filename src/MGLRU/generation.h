#ifndef GENERATION_H
#define GENERATION_H

#include "access_tracker.h"

#include <list>
#include <memory>

class Generation {
public:
    Generation(size_t index, size_t tracker_size);
    ~Generation() = default;
    
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