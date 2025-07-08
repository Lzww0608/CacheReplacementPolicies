cmake_minimum_required(VERSION 3.14)
project(CRP_Cache_Library)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 包含目录
include_directories(include)

# 查找或下载Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50f33f9f6c5c0004e6b8088b9c02d0.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 启用测试
enable_testing()

# LFU缓存测试
add_executable(
  lfu_cache_test
  test/lfu_cache_test.cpp
)

target_link_libraries(
  lfu_cache_test
  gtest_main
)

include(GoogleTest)
gtest_discover_tests(lfu_cache_test)

# 传统测试（不使用GTest）
add_executable(
  lru_ttl_test
  test/lru_ttl_test.cpp
)

add_executable(
  time_wheel_test
  test/time_wheel_test.cpp
)

add_executable(
  lru_benchmark_test
  test/lru_benchmark_test.cpp
)

# 自定义目标
add_custom_target(run_lfu_tests
    COMMAND lfu_cache_test
    DEPENDS lfu_cache_test
    COMMENT "Running LFU Cache Tests"
)

add_custom_target(run_all_tests
    COMMAND lru_ttl_test && lfu_cache_test && time_wheel_test
    DEPENDS lru_ttl_test lfu_cache_test time_wheel_test
    COMMENT "Running All Cache Tests"
) 