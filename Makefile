CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
INCLUDES = -Iinclude
SRCDIR = src
TESTDIR = test
BUILDDIR = build

# GTest配置
GTEST_LIBS = -lgtest -lgtest_main

# 创建构建目录
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# 编译测试程序
test_lru_ttl: $(BUILDDIR) $(TESTDIR)/lru_ttl_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TESTDIR)/lru_ttl_test.cpp -o $(BUILDDIR)/test_lru_ttl

# 编译LFU缓存GTest测试
test_lfu_cache: $(BUILDDIR) $(TESTDIR)/lfu_cache_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TESTDIR)/lfu_cache_test.cpp $(GTEST_LIBS) -o $(BUILDDIR)/test_lfu_cache

# 编译性能测试
test_lru_benchmark: $(BUILDDIR) $(TESTDIR)/lru_benchmark_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TESTDIR)/lru_benchmark_test.cpp -o $(BUILDDIR)/test_lru_benchmark

# 编译时间轮测试
test_time_wheel: $(BUILDDIR) $(TESTDIR)/time_wheel_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TESTDIR)/time_wheel_test.cpp -o $(BUILDDIR)/test_time_wheel

# 运行功能测试
test: test_lru_ttl
	./$(BUILDDIR)/test_lru_ttl

# 运行LFU缓存测试
test_lfu: test_lfu_cache
	./$(BUILDDIR)/test_lfu_cache

# 运行所有测试
test_all: test_lru_ttl test_lfu_cache test_time_wheel
	@echo "Running all tests..."
	./$(BUILDDIR)/test_lru_ttl
	./$(BUILDDIR)/test_lfu_cache
	./$(BUILDDIR)/test_time_wheel

# 运行性能测试
benchmark: test_lru_benchmark
	./$(BUILDDIR)/test_lru_benchmark

# 清理
clean:
	rm -rf $(BUILDDIR)

# 调试版本
debug: CXXFLAGS += -g -DDEBUG
debug: test_lru_ttl test_lfu_cache

# 性能测试版本
release: CXXFLAGS += -O3 -DNDEBUG
release: test_lfu_cache

.PHONY: test test_lfu test_all clean debug benchmark release 