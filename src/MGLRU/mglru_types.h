#ifndef MGLRU_TYPES_H
#define MGLRU_TYPES_H

#include <cstdint>
#include <list>

// to emulate the physical page frame
using PageFrameId = std::uint64_t;


// core data in page frame
// O(1) access to page frame
struct PageMetadata {
    PageFrameId id;
    size_t generation_index;
    std::list<PageFrameId>::iterator lru_iterator;
}

#endif