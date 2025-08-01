#ifndef W_TINYLFU_CACHE_H
#define W_TINYLFU_CACHE_H

#include <cstdint>
#include <chrono>
#include <atomic>

namespace CRP {
namespace w_tinylfu {

constexpr double DEFAULT_DECAY_FACTOR = 0.5; 
constexpr uint32_t DEFAULT_DECAY_INTERVAL = 1000; // 1000 ms

template <typename K, typename V, typename Hash = std::hash<std::string>>
class Cache {
public:
    // 查询缓存：返回是否命中，若命中则通过value输出
    bool get(const K& key, V& value);

    // 插入缓存：若key已存在则更新value，否则插入新条目
    void put(const K& key, const V& value);

    // 删除缓存条目
    bool erase(const K& key);

    // 获取当前缓存总大小
    size_t size() const;
private:
    size_t capacity_;
    double decay_factor_;
    std::chrono::milliseconds decay_interval_
    LoadingCache<K, V, Hash> loading_cache_;
    SLRU<K, V, Hash> slru_;
    CountMinSketch cms_;

    std::thread decay_thread_;          // 后台衰减线程
    std::atomic<bool> is_running_{false};  // 线程运行标志
    std::condition_variable_any decay_cv_; // 用于线程等待衰减周期
    mutable std::mutex decay_mutex_;    // 配合条件变量的锁

    void start_decay_thread();
    void stop_decay_thread();
    void decay_loop();
    void decay_all_frequencies(double factor);
};

}
}

#endif
