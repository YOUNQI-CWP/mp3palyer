# 音乐播放器

这是一个基于 C++、SDL2、OpenGL 和 Dear ImGui 构建的简单音乐播放器。

<<<<<<< HEAD
## 视频演示

![演示视频](https://imgcdn.youngqi.cn/video/music-player.mp4)

=======
>>>>>>> d417881683f34d0eb3eab80788f280c4dcd223a0
## 目录

- [项目环境依赖配置](#项目环境依赖配置)
- [编译](#编译)
- [使用](#使用)
- [技术栈](#技术栈)
- [核心逻辑](#核心逻辑)
  - [线程处理](#线程处理)
  - [文件读写](#文件读写)
  - [UI 实现](#ui-实现)
  - [资源处理](#资源处理)

## 项目环境依赖配置

本项目使用 CMake 进行构建管理，并依赖以下库：

- **SDL2**: 用于窗口管理、事件处理和图形上下文。
- **OpenGL**: 用于渲染图形。
- **Dear ImGui**: 用于构建用户界面。
- **miniaudio**: 用于音频解码和播放。

### 前提条件

在编译项目之前，请确保您的系统已安装以下软件：

- **CMake**: 版本 3.10 或更高。
  - [下载 CMake](https://cmake.org/download/)
- **C++ 编译器**: 支持 C++17 标准（例如 GCC, Clang, MSVC）。
- **SDL2 开发库**: 您需要下载并安装 SDL2 的开发库。根据您的操作系统，安装方式可能有所不同：
  - **Windows**: 从 [SDL 官网](https://www.libsdl.org/download-2.0.php) 下载 `Development Libraries (VC)` 版本，并将其包含在您的项目中或系统路径中。
  - **macOS**: `brew install sdl2`
  - **Linux**: `sudo apt-get install libsdl2-dev` (Debian/Ubuntu) 或 `sudo yum install SDL2-devel` (Fedora/RHEL)
- **OpenGL 开发库**: 通常随您的显卡驱动或系统开发工具一同安装。在某些Linux发行版上可能需要安装 `libglu1-mesa-dev` 和 `freeglut3-dev`。

### 配置 `c_cpp_properties.json` (VSCode)

如果您在 VSCode 中遇到头文件找不到的 Linter 错误，请在项目根目录的 `.vscode` 文件夹中创建或更新 `c_cpp_properties.json` 文件，添加正确的 `includePath`。以下是一个示例配置：

```json
{
    "configurations": [
        {
            "name": "Win32",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/include",
                "${workspaceFolder}/src",
                "${workspaceFolder}/vendor/miniaudio",
                "${workspaceFolder}/vendor/imgui",
                "${workspaceFolder}/vendor/imgui/backends"
            ],
            "defines": [
                "_DEBUG",
                "UNICODE",
                "_UNICODE"
            ],
            "windowsSdkVersion": "10.0.19041.0",
            "compilerPath": "C:/Program Files/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64/cl.exe",
            "cStandard": "c11",
            "cppStandard": "c++17",
            "intelliSenseMode": "msvc-x64"
        }
    ],
    "version": 4
}
```

请根据您的实际 SDL2 和编译器安装路径调整 `includePath` 和 `compilerPath`。

## 编译

本项目使用 CMake 进行编译。请按照以下步骤操作：

1.  **创建构建目录** (在项目根目录执行):
    ```bash
    mkdir build
    cd build
    ```

2.  **运行 CMake 配置** (在 `build` 目录执行):
    ```bash
    cmake ..
    ```
    这会根据您的系统生成相应的构建文件（例如 Makefile 或 Visual Studio 项目文件）。

3.  **编译项目** (在 `build` 目录执行):
    ```bash
    cmake --build .
    ```
    这会编译源代码并生成可执行文件。

## 使用

编译成功后，您可以在 `build` 目录下找到生成的可执行文件（例如 `mp3-player.exe` 或 `mp3-player`）。

运行可执行文件，即可启动 MP3 播放器。首次运行时，您可能需要：

1.  在左侧边栏选择"主列表"视图。
2.  在播放列表区域上方的"音乐目录"部分，输入包含您MP3文件的目录路径，然后点击"添加"按钮。
3.  播放器将扫描指定目录下的 `.mp3`, `.wav`, `.flac` 文件并将其添加到播放列表。
4.  双击列表中的歌曲即可播放。

## 技术栈

-   **语言**: C++17
-   **构建系统**: CMake
-   **图形/窗口**: SDL2 (Simple DirectMedia Layer)
-   **渲染 API**: OpenGL
-   **用户界面**: Dear ImGui
-   **音频处理**: miniaudio
-   **文件系统操作**: C++17 `std::filesystem`

## 核心逻辑

### 线程处理

本播放器主要通过 `miniaudio` 库进行音频播放，`miniaudio` 库在内部管理其音频播放线程。应用程序通过 `ma_device` 实例与音频设备交互。

-   **`AudioState` 结构体**: 包含了 `ma_decoder`、`ma_device` 和 `ma_mutex`。`ma_mutex` 是一个互斥锁，用于在多个线程访问 `AudioState` 中的共享数据（如解码器状态、播放进度）时保证线程安全。
-   **`data_callback` 函数**: 这是 `miniaudio` 的音频数据回调函数，在一个独立的音频线程中运行。它负责从 `ma_decoder` 读取音频数据并将其填充到输出缓冲区。在访问 `audioState` 成员时，它会使用 `ma_mutex_lock` 和 `ma_mutex_unlock` 来确保数据一致性，防止竞态条件。
-   **主线程与音频线程的通信**: 主UI线程会更新 `AudioState` 中的播放模式、当前歌曲索引等信息，并通过互斥锁确保这些更新对音频线程是可见且安全的。例如，当用户拖动进度条时，主线程会调用 `ma_decoder_seek_to_pcm_frame`，这需要在互斥锁保护下进行。

### 文件读写

应用程序通过 `files.cpp` 中定义的函数进行文件读写，主要用于管理音乐目录配置和用户喜爱歌曲列表。

-   **`ScanDirectoryForMusic`**: 遍历指定目录（包括子目录）以查找支持的音乐文件（.mp3, .wav, .flac）。它使用 `std::filesystem` 进行目录遍历，并从文件名中解析艺术家和歌曲标题。
-   **`SaveConfig` / `LoadConfig`**: 负责将用户添加的音乐目录路径保存到 `config.txt` 文件中，或从该文件加载已配置的目录。每个目录路径占据文件中的一行。
-   **`SaveLikedSongs` / `LoadLikedSongs`**: 负责将用户标记为"喜爱"的歌曲路径保存到 `liked_songs.txt` 文件中，或从该文件加载喜爱歌曲的路径。同样，每个歌曲路径占据文件中的一行。

### UI 实现

用户界面通过 [Dear ImGui](https://github.com/ocornut/imgui) 库实现，并使用 SDL2 和 OpenGL 作为后端进行渲染。

-   **`SetModernDarkStyle`**: 设置 ImGui 的视觉风格，使其具有现代深色主题和圆角设计，提升用户体验。
-   **`ShowLeftSidebar`**: 渲染应用程序左侧的导航边栏，允许用户切换主播放列表和喜爱歌曲列表视图。
-   **`ShowPlayerWindow`**: 渲染底部播放器控制区域。这包括：
    -   当前播放歌曲的标题和艺术家信息。
    -   播放控制按钮 (上一曲、播放/暂停、下一曲)。
    -   歌曲播放进度条，支持拖动调整进度。
    -   音量控制滑块。
    -   播放模式切换按钮 (顺序循环、单曲循环、随机播放)。
    -   喜爱/取消喜爱当前歌曲的按钮，并实时更新和保存喜爱状态。
-   **`ShowPlaylistWindow`**: 渲染主内容区域的播放列表。在主列表视图下，它还提供了音乐目录管理界面，允许用户添加、移除和重新扫描音乐目录。
    -   歌曲列表以表格形式显示，支持滚动和双击播放歌曲。
    -   对于喜爱歌曲视图，它会动态地从主播放列表中筛选出被标记为喜爱的歌曲。

### 资源处理

项目中的资源包括音乐文件和字体文件。

-   **音乐文件**: (`.mp3`, `.wav`, `.flac`) 由 `miniaudio` 库负责解码和播放。应用程序通过 `files.cpp` 中的 `ScanDirectoryForMusic` 函数发现这些文件，并存储它们的路径和元数据。
-   **字体文件**: `font.ttf` 在应用程序启动时由 ImGui 加载 (`ImGui::GetIO().Fonts->AddFontFromFileTTF`)。它用于渲染UI中的所有文本，包括中文。
-   **配置和喜爱列表**: `config.txt` 和 `liked_songs.txt` 文件用于持久化应用程序的配置（音乐目录）和用户数据（喜爱歌曲列表）。这些文件在应用程序启动时加载，并在用户进行相关操作时保存，确保数据在不同会话间保持一致。 