# CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(mp3-player VERSION 1.0)

# 设置C++标准为 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置CMake策略以处理OpenGL库选择的警告
cmake_policy(SET CMP0072 NEW)

# 寻找SDL2库
find_package(SDL2 REQUIRED)

# 寻找OpenGL库
find_package(OpenGL REQUIRED)

# 定义可执行文件及其源文件
add_executable(mp3-player 
    src/main.cpp
    src/ui.cpp
    src/audio.cpp
    src/files.cpp
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_sdl2.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

# 添加include和src目录到头文件搜索路径
target_include_directories(mp3-player PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
)

# 链接所需的库
target_link_libraries(mp3-player PRIVATE 
    SDL2::SDL2
    OpenGL::GL
)

# 为Windows平台添加特定设置
if(WIN32)
    # 链接这些库以获得完整的Windows API功能
    target_link_libraries(mp3-player PRIVATE shell32 gdi32)
endif()

# 为macOS平台添加特定设置
if(APPLE)
    # 在macOS上链接特定的框架
    target_link_libraries(mp3-player PRIVATE "-framework CoreFoundation")
endif()