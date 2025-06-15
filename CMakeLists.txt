cmake_minimum_required(VERSION 3.16)
project(MP3Player VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 外部依赖 ---
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
endif()

# --- Dear ImGui ---
add_library(ImGui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_sdl2.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(ImGui PUBLIC vendor/imgui vendor/imgui/backends)
target_link_libraries(ImGui PUBLIC SDL2::SDL2 OpenGL::GL)

# --- 主可执行文件 ---
add_executable(mp3-player main.cpp)

target_include_directories(mp3-player PUBLIC vendor)

# --- 链接库 ---
target_link_libraries(mp3-player PRIVATE ImGui)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(mp3-player PRIVATE 
        Threads::Threads 
        dl
        stdc++fs # 新增：链接文件系统库
    )
endif()