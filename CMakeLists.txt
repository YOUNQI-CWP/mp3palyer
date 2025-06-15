cmake_minimum_required(VERSION 3.16)
project(MP3Player VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SDL2 & OpenGL (将find_package提前) ---
# 先找到外部依赖，才能让后续目标链接它们
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

# --- 核心修正 ---
# 明确告诉CMake，ImGui库的构建依赖于SDL2和OpenGL
# 这样在编译imgui_impl_sdl2.cpp和imgui_impl_opengl3.cpp时，
# 编译器就能找到 SDL.h 和 gl.h 等头文件。
target_link_libraries(ImGui PUBLIC SDL2::SDL2 OpenGL::GL)


# --- 主可执行文件 ---
add_executable(mp3-player main.cpp)

# 为我们的项目添加头文件搜索路径
target_include_directories(mp3-player PUBLIC
    .
    vendor
)

# 链接所有需要的库
# mp3-player 链接了 ImGui, 而ImGui已经"携带"了SDL2和OpenGL的依赖
# 所以这里的链接关系变得更清晰
target_link_libraries(mp3-player PRIVATE
    ImGui
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(mp3-player PRIVATE Threads::Threads dl)
endif()