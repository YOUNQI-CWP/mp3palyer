cmake_minimum_required(VERSION 3.10)
project(mp3-player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 定义源文件目录 ---
set(APP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

# --- 添加头文件搜索路径 ---
include_directories(
    ${APP_SRC_DIR}
    ${VENDOR_DIR}/imgui
    ${VENDOR_DIR}/imgui/backends
    ${VENDOR_DIR}/miniaudio
)

# --- 明确指定所有源文件 ---
set(APP_SOURCES
    ${APP_SRC_DIR}/main.cpp
    ${APP_SRC_DIR}/audio.cpp
    ${APP_SRC_DIR}/files.cpp
    ${APP_SRC_DIR}/ui.cpp
)

# 自动查找 ImGui 的源文件
file(GLOB IMGUI_SOURCES "${VENDOR_DIR}/imgui/*.cpp")
file(GLOB IMGUI_BACKEND_SOURCES "${VENDOR_DIR}/imgui/backends/*.cpp")

# --- 添加可执行文件及其所有源文件 ---
add_executable(mp3-player
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
    ${IMGUI_BACKEND_SOURCES}
)

# --- 查找并链接依赖库 ---
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(mp3-player PRIVATE SDL2::SDL2 OpenGL::GL)

# (可选，但推荐) 为VS Code生成更准确的配置
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)