/*
@Author: Lzww
@Description: 时间轮单元测试
@Language: C++17
*/

#include "../include/time_wheel.h"
#include <iostream>
#include <cassert>
#include <atomic>

void test_single_timer() {
    std::cout << "=== Test: Single Timer ===" << std::endl;
    TimeWheel<> tw;
    tw.start();

    std::atomic<bool> executed = false;
    // 50ms / 10ms/tick = 5 ticks
    tw.add_timer(50, [&]() {
        executed = true;
        std::cout << "  Single timer executed." << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(executed);
    std::cout << "  Single timer test PASSED." << std::endl;
}

void test_cancel_timer() {
    std::cout << "=== Test: Cancel Timer ===" << std::endl;
    TimeWheel<> tw;
    tw.start();

    std::atomic<bool> executed = false;
    uint64_t timer_id = tw.add_timer(100, [&]() {
        executed = true;
    });

    bool cancelled = tw.cancel_timer(timer_id);
    assert(cancelled);
    std::cout << "  Timer cancellation successful." << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(!executed);
    std::cout << "  Cancelled timer did not execute." << std::endl;

    bool already_cancelled = tw.cancel_timer(timer_id);
    assert(!already_cancelled);
    std::cout << "  Cancelling a non-existent timer failed as expected." << std::endl;
    
    std::cout << "  Cancel timer test PASSED." << std::endl;
}

void test_cascade_timer() {
    std::cout << "=== Test: Cascade Timer (Long Delay) ===" << std::endl;
    TimeWheel<> tw(10, {10, 10}); // 10ms tick, 2 levels
    tw.start();
    
    std::atomic<int> executed_count = 0;
    // 延迟需要跨越至少一个完整的轮询周期: 10ms * 10 = 100ms
    long long delay = 110; // 11 ticks
    
    tw.add_timer(delay, [&]() {
        executed_count++;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(delay + 50));
    assert(executed_count == 1);
    std::cout << "  Cascade/Overflow timer test PASSED." << std::endl;
}

void test_concurrent_add() {
    std::cout << "=== Test: Concurrent Add ===" << std::endl;
    TimeWheel<> tw;
    tw.start();

    const int num_threads = 8;
    const int timers_per_thread = 100;
    std::atomic<int> executed_count = 0;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < timers_per_thread; ++j) {
                tw.add_timer(50 + (rand() % 50), [&]() {
                    executed_count++;
                });
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(executed_count == num_threads * timers_per_thread);
    std::cout << "  Executed " << executed_count << " timers concurrently." << std::endl;
    std::cout << "  Concurrent add test PASSED." << std::endl;
}

void test_concurrent_add_cancel() {
    std::cout << "=== Test: Concurrent Add & Cancel ===" << std::endl;
    TimeWheel<> tw;
    tw.start();

    const int num_threads = 4;
    const int ops_per_thread = 500;
    std::atomic<int> executed_count = 0;
    std::atomic<int> cancelled_count = 0;
    std::vector<uint64_t> timer_ids;
    std::mutex vector_mutex;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        // Add threads
        threads.emplace_back([&]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                uint64_t id = tw.add_timer(100 + (rand() % 100), [&]() {
                    executed_count++;
                });
                std::lock_guard<std::mutex> lock(vector_mutex);
                timer_ids.push_back(id);
            }
        });
        // Cancel threads
        threads.emplace_back([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            for (int j = 0; j < ops_per_thread / 2; ++j) {
                uint64_t id_to_cancel = 0;
                {
                    std::lock_guard<std::mutex> lock(vector_mutex);
                    if (!timer_ids.empty()) {
                        id_to_cancel = timer_ids.back();
                        timer_ids.pop_back();
                    }
                }
                if (id_to_cancel > 0 && tw.cancel_timer(id_to_cancel)) {
                    cancelled_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "  Total timers added: " << num_threads * ops_per_thread << std::endl;
    std::cout << "  Total timers cancelled: " << cancelled_count << std::endl;
    std::cout << "  Total timers executed: " << executed_count << std::endl;
    assert(executed_count + cancelled_count <= num_threads * ops_per_thread);
    std::cout << "  Concurrent add & cancel test PASSED." << std::endl;
}


int main() {
    srand(time(nullptr));
    std::cout << "--- Running Time Wheel Unit Tests ---" << std::endl;
    
    test_single_timer();
    test_cancel_timer();
    test_cascade_timer();
    test_concurrent_add();
    test_concurrent_add_cancel();

    std::cout << "\n✅ All Time Wheel tests passed!" << std::endl;
    return 0;
} 