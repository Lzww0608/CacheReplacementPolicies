#ifndef ACCESS_TRACKER_H
#define ACCESS_TRACKER_H

#include "mglru_types.h"

#include <vector>
#include <atomic>

using BitSet = std::atomic<uint64_t>;

class AccessTracker {
public:
    // size is the power of 2
    explicit AccessTracker(size_t size);
    ~AccessTracker() = default;

    // set visited bit for the page frame
    void set(PageFrameId page_id);

    // check if the page frame is visited and clear the visited bit
    // atomic operation
    bool check_and_clear(PageFrameId page_id);

    // clear all the visited bits
    void clear();

private:
    std::vector<BitSet> bitset_;
    size_t size_mask_; // quick mask to get the index of the bit set
};

#endif