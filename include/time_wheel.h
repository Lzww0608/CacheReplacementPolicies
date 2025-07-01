/*
@Author: Lzww
@LastEditTime: 2025-06-28 23:10:00
@Description: 多层级高精度时间轮 (Hierarchical High-Precision Timing Wheel)
@Language: C++17
*/

#ifndef TIME_WHEEL_H
#define TIME_WHEEL_H

#include <iostream>
#include <vector>
#include <list>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <stdexcept>
#include <numeric>

// 前向声明
template<typename MutexType>
class TimeWheel;

// 定时器节点定义
struct TimerNode {
    uint64_t id;                                  // 定时器唯一ID
    long long expiration_tick;                    // 预期的到期 tick 值
    std::function<void()> callback;               // 回调函数

    // 用于快速取消定时器时定位节点
    int level;                                    // 所在层级
    std::list<std::unique_ptr<TimerNode>>* slot_list; // 指向所在槽位的链表

    // 使用 list::iterator 代替裸指针来精确定位，更安全
    typename std::list<std::unique_ptr<TimerNode>>::iterator list_iterator;
};


// 默认配置
constexpr long long DEFAULT_TICK_DURATION_MS = 10; // 默认滴答精度：10ms
const std::vector<size_t> DEFAULT_WHEEL_SIZES = {256, 128, 64, 32}; // 4层，总时间跨度约 10ms * 256 * 128 * 64 * 32 ≈ 7.5天


template<typename MutexType = std::mutex>
class TimeWheel {
public:
    // 构造函数
    explicit TimeWheel(
        long long tick_duration_ms = DEFAULT_TICK_DURATION_MS,
        const std::vector<size_t>& wheel_sizes = DEFAULT_WHEEL_SIZES
    );

    // 析构函数
    ~TimeWheel();

    // 禁止拷贝和移动
    TimeWheel(const TimeWheel&) = delete;
    TimeWheel& operator=(const TimeWheel&) = delete;

    // 启动和停止时间轮
    void start();
    void stop();

    // 添加定时器，返回定时器ID
    // delay: 延迟时间 (毫秒)
    uint64_t add_timer(long long delay_ms, std::function<void()> callback);

    // 取消定时器
    bool cancel_timer(uint64_t timer_id);

private:
    // 核心滴答函数
    void tick();
    // 后台工作线程循环
    void worker_loop();
    // 内部添加定时器逻辑
    void add_timer_internal(std::unique_ptr<TimerNode> timer);

    const std::chrono::milliseconds tick_duration_;
    const std::vector<size_t> wheel_sizes_;
    std::vector<long long> level_intervals_; // 每一层代表的时间跨度（滴答数）

    std::vector<std::vector<std::list<std::unique_ptr<TimerNode>>>> wheels_;
    std::unordered_map<uint64_t, TimerNode*> timer_map_;

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> next_timer_id_{1};
    long long current_tick_{0};

    std::thread worker_thread_;
    MutexType mutex_;
    std::condition_variable cv_;

    // 新增 overflow_timers_
    std::list<std::unique_ptr<TimerNode>> overflow_timers_;

    void cascade(int level, long long ticks);
};

// 构造函数实现
template<typename MutexType>
TimeWheel<MutexType>::TimeWheel(long long tick_duration_ms, const std::vector<size_t>& wheel_sizes)
    : tick_duration_(tick_duration_ms), wheel_sizes_(wheel_sizes) {
    if (wheel_sizes_.empty()) {
        throw std::invalid_argument("Time wheel sizes cannot be empty.");
    }
    wheels_.resize(wheel_sizes_.size());
    level_intervals_.resize(wheel_sizes_.size() + 1);
    level_intervals_[0] = 1;
    for (size_t i = 0; i < wheel_sizes_.size(); ++i) {
        wheels_[i].resize(wheel_sizes_[i]);
        level_intervals_[i+1] = level_intervals_[i] * wheel_sizes_[i];
    }
}

// 析构函数实现
template<typename MutexType>
TimeWheel<MutexType>::~TimeWheel() {
    stop();
}

// 启动时间轮
template<typename MutexType>
void TimeWheel<MutexType>::start() {
    if (running_.exchange(true)) {
        return; // 已经启动
    }
    worker_thread_ = std::thread(&TimeWheel::worker_loop, this);
}

// 停止时间轮
template<typename MutexType>
void TimeWheel<MutexType>::stop() {
    if (!running_.exchange(false)) {
        return; // 已经停止
    }
    cv_.notify_one();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

// 后台工作线程
template<typename MutexType>
void TimeWheel<MutexType>::worker_loop() {
    while (running_) {
        auto start_time = std::chrono::steady_clock::now();
        
        {
            std::lock_guard<MutexType> lock(mutex_);
            tick();
        }

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = end_time - start_time;
        
        if (elapsed < tick_duration_) {
            std::unique_lock<MutexType> lock(mutex_);
            cv_.wait_for(lock, tick_duration_ - elapsed, [this] { return !running_; });
        }
    }
}

// 核心滴答函数
template<typename MutexType>
void TimeWheel<MutexType>::tick() {
    current_tick_++;
    
    int current_slot = current_tick_ % wheel_sizes_[0];
    
    if (current_slot == 0) {
        cascade(1, current_tick_ / wheel_sizes_[0]);
    }
    
    auto& slot_list = wheels_[0][current_slot];
    if (slot_list.empty()) return;

    std::list<std::unique_ptr<TimerNode>> remaining_tasks;
    
    for (auto it = slot_list.begin(); it != slot_list.end(); ) {
        if ((*it)->expiration_tick <= current_tick_) {
            auto& timer = *it;
            timer_map_.erase(timer->id);
            if (timer->callback) timer->callback();
            it = slot_list.erase(it);
        } else {
            ++it;
        }
    }
}

// 添加定时器
template<typename MutexType>
uint64_t TimeWheel<MutexType>::add_timer(long long delay_ms, std::function<void()> callback) {
    auto timer = std::make_unique<TimerNode>();
    
    uint64_t timer_id = next_timer_id_.fetch_add(1);
    timer->id = timer_id;
    timer->callback = std::move(callback);
    
    long long ticks_to_expire = delay_ms / tick_duration_.count();
    
    std::lock_guard<MutexType> lock(mutex_);
    timer->expiration_tick = current_tick_ + (ticks_to_expire > 0 ? ticks_to_expire : 1);
    add_timer_internal(std::move(timer));
    
    return timer_id;
}

// 取消定时器
template<typename MutexType>
bool TimeWheel<MutexType>::cancel_timer(uint64_t timer_id) {
    std::lock_guard<MutexType> lock(mutex_);
    auto it = timer_map_.find(timer_id);
    if (it == timer_map_.end()) {
        return false; // 定时器不存在或已执行
    }

    TimerNode* node_ptr = it->second;
    node_ptr->slot_list->erase(node_ptr->list_iterator);
    timer_map_.erase(it);
    
    return true;
}

// 内部添加逻辑 - 最终正确重写
template<typename MutexType>
void TimeWheel<MutexType>::add_timer_internal(std::unique_ptr<TimerNode> timer) {
    long long ticks_to_expire = (timer->expiration_tick - current_tick_);
    if (ticks_to_expire < 0) ticks_to_expire = 0;

    long long target_tick = current_tick_ + ticks_to_expire;

    for (size_t level = 0; level < wheel_sizes_.size(); ++level) {
        if (ticks_to_expire < level_intervals_[level+1]) {
            int slot = (target_tick / level_intervals_[level]) % wheel_sizes_[level];
            timer->level = level;
            auto& target_list = wheels_[level][slot];
            target_list.push_front(std::move(timer));
            TimerNode* node_ptr = target_list.front().get();
            node_ptr->slot_list = &target_list;
            node_ptr->list_iterator = target_list.begin();
            timer_map_[node_ptr->id] = node_ptr;
            return;
        }
    }
    // ... (处理超出范围)
}

template<typename MutexType>
void TimeWheel<MutexType>::cascade(int level, long long ticks) {
    if (level >= wheel_sizes_.size()) return;

    int current_slot = ticks % wheel_sizes_[level];
    
    if (current_slot == 0 && ticks > 0) {
        cascade(level + 1, ticks / wheel_sizes_[level]);
    }
    
    auto& slot_list = wheels_[level][current_slot];
    std::list<std::unique_ptr<TimerNode>> to_cascade;
    to_cascade.swap(slot_list);

    for (auto& timer : to_cascade) {
        add_timer_internal(std::move(timer));
    }
}

#endif // TIME_WHEEL_H
