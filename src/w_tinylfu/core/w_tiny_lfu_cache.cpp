
#include "../../../include/w_tinylfu/api/cache.h"


namespace CRP {
namespace w_tinylfu {

template <typename K, typename V, typename Hash = std::hash<std::string>>
void Cache<K, V, Hash>::start_decay_thread() {
    is_running_ = true;
    decay_thread_ = std::thread(&WTinyLFUCache::decay_loop, this);
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void Cache<K, V, Hash>::stop_decay_thread() {
    is_running_ = false;
    decay_cv_.notify_one();  // 唤醒等待的线程
    if (decay_thread_.joinable()) {
        decay_thread_.join();
    }
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void Cache<K, V, Hash>::decay_loop() {
    // wait for decay period
    std::unique_lock<std::mutex> lock(decay_mutex_);
    decay_cv_.wait_for(lock, decay_interval_, [this]() {
        return !is_running_;
    });
    if (!is_running_) {
        break;
    }

    // decay all nodes in protected_
    protected_.decay_all_frequencies(decay_factor_);  // 遍历所有条目，频率 *= 衰减因子
}

}
}
