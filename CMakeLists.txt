# CMakeLists.txt for CRP Project
# High-Performance Cache and Bloom Filter Implementation

cmake_minimum_required(VERSION 3.10)
project(CRP VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# Find required packages
find_package(Threads REQUIRED)

# Check for SIMD support
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <immintrin.h>
    int main() {
        __m128i a = _mm_set1_epi32(1);
        return 0;
    }
" HAVE_SSE)

if(HAVE_SSE)
    add_definitions(-DHAVE_SSE)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(BLOOM_FILTER_SOURCES
    src/utils/bloom_filter.cpp
)

set(SRRIP_CACHE_SOURCES
    src/SRRIP/srrip_cache_instantiations.cpp
)

# Create bloom filter library
add_library(bloom_filter STATIC ${BLOOM_FILTER_SOURCES})
target_include_directories(bloom_filter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bloom_filter Threads::Threads)

# Create SRRIP cache library
add_library(srrip_cache STATIC ${SRRIP_CACHE_SOURCES})
target_include_directories(srrip_cache PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(srrip_cache Threads::Threads)

# S3FIFO cache is header-only, no separate library needed

# Demo executables
add_executable(bloom_filter_demo 
    examples/bloom_filter_demo.cpp
)
target_link_libraries(bloom_filter_demo bloom_filter)

add_executable(srrip_cache_demo
    examples/srrip_cache_demo.cpp
)
target_link_libraries(srrip_cache_demo srrip_cache)

add_executable(s3fifo_cache_demo
    examples/s3fifo_cache_demo.cpp
)
target_link_libraries(s3fifo_cache_demo Threads::Threads)

add_executable(address_mapping_test
    test/address_mapping_test.cpp
)
target_link_libraries(address_mapping_test srrip_cache)

# Find GoogleTest
find_package(GTest QUIET)
if(GTest_FOUND)
    # Test executables
    add_executable(bloom_filter_test
        test/bloom_filter_test.cpp
    )
    target_link_libraries(bloom_filter_test 
        bloom_filter
        GTest::gtest 
        GTest::gtest_main
    )

    add_executable(srrip_cache_test
        test/srrip_cache_test.cpp
    )
    target_link_libraries(srrip_cache_test
        srrip_cache
        GTest::gtest
        GTest::gtest_main
    )

    add_executable(s3fifo_cache_test
        test/s3fifo_cache_test.cpp
    )
    target_link_libraries(s3fifo_cache_test
        Threads::Threads
        GTest::gtest
        GTest::gtest_main
    )

    # Enable testing
    enable_testing()
    add_test(NAME BloomFilterTests COMMAND bloom_filter_test)
    add_test(NAME SRRIPCacheTests COMMAND srrip_cache_test)
    add_test(NAME S3FIFOCacheTests COMMAND s3fifo_cache_test)
    
    message(STATUS "Google Test found - tests will be built")
else()
    message(STATUS "Google Test not found - tests will not be built")
    message(STATUS "To install Google Test on Ubuntu: sudo apt-get install libgtest-dev")
endif()

# Installation
install(TARGETS bloom_filter srrip_cache
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES 
    include/utils/bloom_filter.h
    include/SRRIP/srrip_cache.h
    include/SRRIP/cache_set.h
    include/SRRIP/cache_line.h
    include/s3fifo/cache.h
    include/utils/node.h
    include/utils/intrusive_list.h
    DESTINATION include
)

install(TARGETS bloom_filter_demo srrip_cache_demo s3fifo_cache_demo address_mapping_test
    RUNTIME DESTINATION bin
)

# Print configuration
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CXX Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  CXX Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  SSE Support: ${HAVE_SSE}")
message(STATUS "  Google Test: ${GTest_FOUND}")
message(STATUS "") 