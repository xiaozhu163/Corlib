cmake_minimum_required(VERSION 3.10)
project(libco)

# 仅适用于 macOS
set(CMAKE_MACOSX_RPATH 0)

# 设置库的版本
set(LIBCO_VERSION 0.5)

# 设置编译标志
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fno-strict-aliasing -O2 -Wall -export-dynamic -Wall -pipe -D_GNU_SOURCE -D_REENTRANT -fPIC -Wno-deprecated -m64")

# 启用 C 和汇编语言
enable_language(C ASM)

# 添加源文件
set(SOURCE_FILES
    co_comm.cpp
    co_epoll.cpp
    co_hook_sys_call.cpp
    co_routine.cpp
    coctx.cpp
    coctx_swap.S
)

# 添加静态和共享库目标
add_library(colib_static STATIC ${SOURCE_FILES})
add_library(colib_shared SHARED ${SOURCE_FILES})

# 设置库输出名称
set_target_properties(colib_static PROPERTIES OUTPUT_NAME colib)
set_target_properties(colib_shared PROPERTIES OUTPUT_NAME colib)

# 确保直接输出，不通过符号链接
set_target_properties(colib_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(colib_shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# 禁用共享库的符号链接
set_target_properties(colib_shared PROPERTIES NO_SONAME 1)

# 设置共享库版本，会生成 libcolib.${LIBCO_VERSION}.so 及一个符号链接 libcolib.so
# 对于 macOS，扩展名将为 .dylib
set_target_properties(colib_shared PROPERTIES VERSION ${LIBCO_VERSION} SOVERSION ${LIBCO_VERSION})

# 宏，用于添加示例目标
macro(add_example_target EXAMPLE_TARGET)
    add_executable("example_${EXAMPLE_TARGET}" "example_${EXAMPLE_TARGET}.cpp")
    target_link_libraries("example_${EXAMPLE_TARGET}" colib_shared pthread dl)
endmacro(add_example_target)

# 添加所有示例目标
add_example_target(closure)
add_example_target(cond)
add_example_target(copystack)
add_example_target(echocli)
add_example_target(echosvr)
add_example_target(poll)
add_example_target(setenv)
add_example_target(specific)
add_example_target(thread)

# 添加主可执行目标
add_executable(main main.cpp)
target_link_libraries(main colib_shared pthread dl)
