#ifndef MGLRU_CORE_H
#define MGLRU_CORE_H

#include "mglru_types.h"
#include "generation.h"
#include "pid_controller.h"

#include <vector>
#include <optional>

struct ReclaimResult {
    std::vector<PageFrameId> evicted_pages;
    size_t promoted_pages_count;
    size_t scanned_pages_count;
}


class MGLRU {
public:
    MGLRU(size_t num_generations, size_t tracker_size_per_gen);
    ~MGLRU() = default;

    void on_page_added(PageFrameId page_id);

    void on_page_accessed(PageFrameId page_id);

    ReclaimResult scan_and_reclaim(size_t pages_to_scan);

    void on_page_removed(const PageMetadata& page_meta);

private:
    void age_generations();
    void promote_page(PageMetadata& page_meta);


    std::vector<Generation> generations_;
    PidController pid_controller_;
    size_t max_generations_;
}