cmake_minimum_required(VERSION 3.16)
project(MP3Player VERSION 1.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找SDL2库
find_package(SDL2 REQUIRED)

# 添加我们的主可执行文件
add_executable(mp3-player main.cpp)

# 链接SDL2到我们的可执行文件
target_link_libraries(mp3-player PRIVATE SDL2::SDL2)

# 在Linux上，我们可能需要链接OpenGL和pthreads
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(mp3-player PRIVATE OpenGL::GL Threads::Threads)
endif()