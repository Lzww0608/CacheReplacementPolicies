# CMakeLists.txt for CRP Bloom Filter
# High-Performance Bloom Filter for W-TinyLFU Cache

cmake_minimum_required(VERSION 3.10)
project(CRP_BloomFilter VERSION 1.0.0 LANGUAGES CXX)

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

# Create bloom filter library
add_library(bloom_filter STATIC ${BLOOM_FILTER_SOURCES})
target_include_directories(bloom_filter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bloom_filter Threads::Threads)

# Demo executable
add_executable(bloom_filter_demo 
    examples/bloom_filter_demo.cpp
)
target_link_libraries(bloom_filter_demo bloom_filter)

# Find GoogleTest
find_package(GTest QUIET)
if(GTest_FOUND)
    # Test executable
    add_executable(bloom_filter_test
        test/bloom_filter_test.cpp
    )
    target_link_libraries(bloom_filter_test 
        bloom_filter
        GTest::gtest 
        GTest::gtest_main
    )
    
    # Enable testing
    enable_testing()
    add_test(NAME BloomFilterTests COMMAND bloom_filter_test)
    
    message(STATUS "Google Test found - tests will be built")
else()
    message(STATUS "Google Test not found - tests will not be built")
    message(STATUS "To install Google Test on Ubuntu: sudo apt-get install libgtest-dev")
endif()

# Installation
install(TARGETS bloom_filter
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES include/utils/bloom_filter.h
    DESTINATION include/utils
)

install(TARGETS bloom_filter_demo
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