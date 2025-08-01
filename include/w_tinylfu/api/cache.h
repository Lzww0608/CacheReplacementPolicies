#ifndef W_TINYLFU_CACHE_H
#define W_TINYLFU_CACHE_H

#include <cstdint>
#include <chrono>
#include <atomic>

namespace CRP {
namespace w_tineylfu {

constexpr double DEFAULT_DECAY_FACTOR = 0.5; 
constexpr uint32_t DEFAULT_DECAY_INTERVAL = 1000; // 1000 ms

template <typename K, typename V, typename Hash = std::hash<std::string>>
class Cache {
public:

private:
    uint64_t capacity_;
    double decay_factor_;
    std::chrono::milliseconds decay_interval_
    LoadingCache<K, V, Hash> loading_cache_;
    SLRU<K, V, Hash> slru_;
    CountMinSketch cms_;

    std::thread decay_thread_;          // 后台衰减线程
    std::atomic<bool> is_running_{false};  // 线程运行标志
    std::condition_variable_any decay_cv_; // 用于线程等待衰减周期
    mutable std::mutex decay_mutex_;    // 配合条件变量的锁
};

}
}

#endif
