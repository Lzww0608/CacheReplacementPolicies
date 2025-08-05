
#include "../../../include/w_tinylfu/api/cache.h"


namespace CRP {
namespace w_tinylfu {

template <typename K, typename V, typename Hash>
Cache<K, V, Hash>::Cache(size_t capacity, double decay_factor, uint32_t decay_interval)
    : capacity_(capacity), decay_factor_(decay_factor), 
      decay_interval_(std::chrono::milliseconds(decay_interval)),
      loading_cache_(capacity * 0.1, 1),  // 窗口缓存占10%
      slru_(capacity * 0.8, capacity * 0.1),  // probation 80%, protection 10%
      cms_(capacity) {
    start_decay_thread();
}

template <typename K, typename V, typename Hash>
Cache<K, V, Hash>::~Cache() {
    stop_decay_thread();
}

template <typename K, typename V, typename Hash = std::hash<std::string>>
void Cache<K, V, Hash>::start_decay_thread() {
    is_running_ = true;
    decay_thread_ = std::thread(&Cache<K, V, Hash>::decay_loop, this);
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
    while (is_running_) {
        // wait for decay period
        std::unique_lock<std::mutex> lock(decay_mutex_);
        decay_cv_.wait_for(lock, decay_interval_, [this]() {
            return !is_running_;
        });
        if (!is_running_) {
            break;
        }

        // decay all nodes in protected_
        slru_.decay_all_frequencies(decay_factor_);  // 遍历所有条目，频率 *= 衰减因子
    }
}

// 查询缓存：返回是否命中，若命中则通过value输出
template <typename K, typename V, typename Hash = std::hash<std::string>>
bool Cache<K, V, Hash>::get(const K& key, V& value) {
    // 首先检查loading_cache_（窗口缓存）
    if (loading_cache_.contains(key)) {
        auto node = loading_cache_.get(key);
        if (loading_cache_.full(key) || node->frequency >= 5) {
            // 窗口缓存命中，需要升级到主缓存
            // 创建Node对象并添加到SLRU的probation区
            auto new_node = new typename SLRU<K, V, Hash>::Node(key, value);
            if (slru_.onAdd(new_node)) {
                loading_cache_.remove(key);
            } else {
                delete new_node;
            }
        }
        
        return true;
    }

    // 检查主缓存（SLRU）
    if (slru_.get(key, value)) {
        return true;
    }

    return false;
}

// 插入缓存：若key已存在则更新value，否则插入新条目
template <typename K, typename V, typename Hash = std::hash<std::string>>
void Cache<K, V, Hash>::put(const K& key, const V& value) {
    // 首先检查是否在主缓存中
    if (slru_.contains(key)) {
        slru_.put(key, value);
        return;
    }

    // 新数据先进入窗口缓存
    loading_cache_.put(key, value);
}

// 删除缓存条目
template <typename K, typename V, typename Hash = std::hash<std::string>>
bool Cache<K, V, Hash>::erase(const K& key) {
    if (slru_.contains(key)) {
        slru_.erase(key);
        return true;
    }

    if (loading_cache_.contains(key)) {
        loading_cache_.remove(key);
        return true;
    }

    return false;
}

// 获取当前缓存总大小
template <typename K, typename V, typename Hash = std::hash<std::string>>
size_t Cache<K, V, Hash>::size() const {
    return slru_.size() + loading_cache_.size();
}

}
}
