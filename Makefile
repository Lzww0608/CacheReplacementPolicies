CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
INCLUDES = -I.
SOURCES = src/utils/rand.cpp
TEST_SOURCE = test_rand.cpp

# 目标文件
OBJECTS = $(SOURCES:.cpp=.o)
TEST_OBJECTS = $(TEST_SOURCE:.cpp=.o)

# 默认目标
all: test_rand

# 编译测试程序
test_rand: $(OBJECTS) $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 编译源文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 运行测试
test: test_rand
	./test_rand

# 清理
clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) test_rand

.PHONY: all test clean 