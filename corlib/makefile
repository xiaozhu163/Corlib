# 编译器
CXX = g++

# 编译器标志
CXXFLAGS = -Wall -std=c++17 -Iinclude

# 搜索当前目录及其子目录下的所有 .cpp 文件
# SRCS = $(shell find . -name '*.cpp')

# 搜索当前目录下的所有 .cpp 文件
SRCS = $(wildcard *.cpp)

# 将 .cpp 文件名替换为 .o 文件名，并保留路径
OBJS = $(SRCS:.cpp=.o)

# 目标可执行文件
TARGET = main

# 默认目标
all: $(TARGET)

# 生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 生成目标文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理
veryclean:
	rm -f $(OBJS) $(TARGET)

clean:
	rm -f $(OBJS)

.PHONY: all clean veryclean
