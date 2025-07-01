CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
INCLUDES = -Iinclude
SRCDIR = src
TESTDIR = test
BUILDDIR = build

# 创建构建目录
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# 编译测试程序
test_lru_ttl: $(BUILDDIR) $(TESTDIR)/lru_ttl_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TESTDIR)/lru_ttl_test.cpp -o $(BUILDDIR)/test_lru_ttl

# 编译性能测试
test_lru_benchmark: $(BUILDDIR) $(TESTDIR)/lru_benchmark_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TESTDIR)/lru_benchmark_test.cpp -o $(BUILDDIR)/test_lru_benchmark

# 编译时间轮测试
test_time_wheel: $(BUILDDIR) $(TESTDIR)/time_wheel_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TESTDIR)/time_wheel_test.cpp -o $(BUILDDIR)/test_time_wheel

# 运行功能测试
test: test_lru_ttl
	./$(BUILDDIR)/test_lru_ttl

# 运行性能测试
benchmark: test_lru_benchmark
	./$(BUILDDIR)/test_lru_benchmark

# 清理
clean:
	rm -rf $(BUILDDIR)

# 调试版本
debug: CXXFLAGS += -g -DDEBUG
debug: test_lru_ttl

# 性能测试版本
benchmark: CXXFLAGS += -O3 -DNDEBUG
benchmark: test_lru_ttl

.PHONY: test clean debug benchmark 