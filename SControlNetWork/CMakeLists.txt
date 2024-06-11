cmake_minimum_required(VERSION 3.0)
project(SControlNetWork)

# 设置语言标准
set(CMAKE_CXX_STANDARD 11)

# 设置源文件目录和可执行文件目录
set(HOME /root/projects/SControlNetWork)
set(EXECUTABLE_OUTPUT_PATH ${HOME}/bin/)

# 头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 源文件目录
file(GLOB SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# 添加并指定最终执行生成的可执行程序名
add_executable(app ${SRC_LIST})

# 指定可执行程序要链接的动态库名字
target_link_libraries(app pthread)