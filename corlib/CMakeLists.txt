cmake_minimum_required(VERSION 3.10)
project(CorlibProject)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR})

# 搜索当前目录下的所有 .cpp 文件
file(GLOB SRCS "*.cpp")

# 添加可执行文件
add_executable(main ${SRCS})

# 设置编译器标志
target_compile_options(main PRIVATE -Wall)
