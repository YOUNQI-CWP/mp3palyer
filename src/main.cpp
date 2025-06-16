/**
 * @file main.cpp
 * @brief MP3播放器应用程序的主文件。
 *
 * 该文件包含了MP3播放器应用程序的入口点 `main` 函数，
 * 负责初始化SDL、OpenGL、ImGui，加载字体，设置UI风格，
 * 初始化音频状态，管理主循环中的事件处理、UI渲染和状态更新，
 * 并在程序退出时进行资源清理。
 */

#include <cstdio>
#include <vector>
#include <set>
#include <filesystem>

#include <SDL.h>
#include <SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

// IMPORTANT: Define the miniaudio implementation in ONLY ONE .cpp file
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "types.h"
#include "audio.h"
#include "files.h"
#include "ui.h"

/**
 * @brief 全局默认字体。
 *
 * 用于ImGui界面的默认字体。
 */
ImFont* G_Font_Default = nullptr;
/**
 * @brief 全局大字体。
 *
 * 用于ImGui界面的大号字体，例如标题等。
 */
ImFont* G_Font_Large = nullptr;

/**
 * @brief 应用程序的入口点。
 *
 * 初始化所有必要的库，设置应用程序窗口和渲染上下文，
 * 管理主事件循环，处理音频播放和UI渲染，并在程序退出时清理资源。
 *
 * @param[in] 无，标准main函数参数。
 * @param[in] 无，标准main函数参数。
 * @return 0 表示成功，-1 表示初始化失败。
 */
int main(int, char**) {
    // 1. 初始化 SDL
    /**
     * @brief 初始化SDL子系统。
     *
     * 初始化视频、定时器和游戏控制器子系统。如果初始化失败，则打印错误并退出。
     */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // 2. 设置 OpenGL
    /**
     * @brief 配置OpenGL上下文。
     *
     * 设置GLSL版本、OpenGL上下文标志、配置文件掩码、主次版本，
     * 并启用双缓冲、深度缓冲区和模板缓冲区。
     * 创建SDL窗口和OpenGL上下文，并使其成为当前上下文。
     */
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("MP3 Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // 启用垂直同步

    // 3. 设置 Dear ImGui
    /**
     * @brief 初始化Dear ImGui。
     *
     * 创建ImGui上下文并配置ImGuiIO，例如启用键盘导航。
     */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 启用键盘导航

    // 4. 加载字体
    /**
     * @brief 加载应用程序使用的字体。
     *
     * 检查 `font.ttf` 文件是否存在。如果存在，则加载默认字体、带有图标的合并字体以及简体中文范围的字体。
     * 如果文件不存在，则打印警告信息。
     */
    const char* font_path = "font.ttf";
    if (std::filesystem::exists(font_path)) {
        ImFontConfig config;
        G_Font_Default = io.Fonts->AddFontFromFileTTF(font_path, 18.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
        
        config.MergeMode = true;
        static const ImWchar icon_ranges[] = { 0x2661, 0x2665, 0 }; // ♡ ♥ 范围
        io.Fonts->AddFontFromFileTTF(font_path, 18.0f, &config, icon_ranges);
        io.Fonts->AddFontFromFileTTF(font_path, 18.0f, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon()); // 加载简体中文范围

        G_Font_Large = io.Fonts->AddFontFromFileTTF(font_path, 28.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
        io.Fonts->AddFontFromFileTTF(font_path, 28.0f, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon()); // 加载简体中文范围
    } else {
        printf("WARNING: Font file '%s' not found. Please add it to the executable directory.\n", font_path);
    }
    
    // 5. 设置风格和后端
    /**
     * @brief 设置UI风格并初始化ImGui后端。
     *
     * 调用 `SetModernDarkStyle()` 函数设置现代暗色风格。
     * 为SDL2和OpenGL3初始化ImGui后端。
     */
    SetModernDarkStyle();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 6. 初始化应用程序状态
    /**
     * @brief 初始化应用程序的音频状态、播放列表和配置。
     *
     * 初始化 `AudioState` 结构体和其互斥锁。
     * 设置默认音量。
     * 初始化主播放列表、喜爱歌曲播放列表、音乐目录和喜爱歌曲路径集合。
     * 加载配置和喜爱歌曲。
     * 扫描音乐目录以填充主播放列表。
     */
    AudioState audioState;
    if (ma_mutex_init(&audioState.mutex) != MA_SUCCESS) return -1;

    float volume = 0.5f; // 默认音量
    std::vector<Song> mainPlaylist; // 主播放列表
    std::vector<Song> likedSongsPlaylist; // 喜爱歌曲播放列表
    std::vector<std::string> musicDirs; // 音乐目录
    std::set<std::string> likedPaths; // 喜爱歌曲的路径集合
    ActiveView currentView = ActiveView::Main; // 当前活动视图
    float display_progress = 0.0f; // 播放进度

    LoadConfig(musicDirs); // 加载配置
    LoadLikedSongs(likedPaths); // 加载喜爱歌曲
    for(const auto& dir : musicDirs) {
        ScanDirectoryForMusic(dir, mainPlaylist, likedPaths); // 扫描目录以填充播放列表
    }
    
    // 7. 主循环
    /**
     * @brief 应用程序的主事件循环。
     *
     * 循环处理SDL事件、更新音频播放状态和UI显示进度。
     * 在每帧开始时更新ImGui框架，设置主音量。
     * 根据视口大小定义UI布局。
     * 渲染左侧边栏、播放列表窗口和播放器窗口。
     * 最后渲染ImGui绘制数据并交换窗口缓冲区。
     */
    bool done = false; // 循环控制标志
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true; // 处理退出事件
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) done = true;
        }

        // 更新状态
        bool isPlaying = audioState.isDeviceInitialized && ma_device_is_started(&audioState.device);
        if (audioState.isAudioReady) {
            float totalElapsedTime = isPlaying ? audioState.elapsedTimeAtPause + (float)(SDL_GetTicks() - audioState.songStartTime) / 1000.0f : audioState.elapsedTimeAtPause;
            display_progress = (audioState.totalSongDurationSec > 0) ? (totalElapsedTime / audioState.totalSongDurationSec) : 0.0f;

            // 歌曲播放完毕，切换到下一首
            if (display_progress >= 1.0f && isPlaying) {
                std::vector<Song>& activePlaylist = (currentView == ActiveView::Main) ? mainPlaylist : likedSongsPlaylist;
                int nextIndex = GetNextSongIndex(audioState, activePlaylist.size());
                PlaySongAtIndex(audioState, activePlaylist, nextIndex, PlayDirection::New);
            }
        } else {
            display_progress = 0.0f;
        }

        // 开始新帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (audioState.isDeviceInitialized) ma_device_set_master_volume(&audioState.device, volume); // 设置主音量

        // 定义布局
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float playerHeight = 110.0f;
        const float leftSidebarWidth = 220.0f;
        ImVec2 playerPos = ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - playerHeight);
        ImVec2 playerSize = ImVec2(viewport->Size.x, playerHeight);
        ImVec2 topAreaPos = viewport->Pos;
        ImVec2 topAreaSize = ImVec2(viewport->Size.x, viewport->Size.y - playerHeight);
        ImVec2 leftSidebarPos = topAreaPos;
        ImVec2 leftSidebarSize = ImVec2(leftSidebarWidth, topAreaSize.y);
        ImVec2 mainContentPos = ImVec2(topAreaPos.x + leftSidebarWidth, topAreaPos.y);
        ImVec2 mainContentSize = ImVec2(topAreaSize.x - leftSidebarWidth, topAreaSize.y);
        
        std::vector<Song>& activePlaylist = (currentView == ActiveView::Main) ? mainPlaylist : likedSongsPlaylist;

        // 从模块渲染UI
        ShowLeftSidebar(leftSidebarPos, leftSidebarSize, currentView); // 显示左侧边栏
        ShowPlaylistWindow(audioState, mainPlaylist, likedSongsPlaylist, musicDirs, currentView, mainContentPos, mainContentSize); // 显示播放列表窗口
        ShowPlayerWindow(audioState, mainPlaylist, activePlaylist, volume, display_progress, playerPos, playerSize); // 显示播放器窗口

        // 渲染帧
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 清除颜色
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window); // 交换窗口缓冲区
    }

    // 8. 清理
    /**
     * @brief 清理所有分配的资源。
     *
     * 取消初始化音频设备、解码器和互斥锁。
     * 关闭ImGui后端，销毁ImGui上下文，销毁OpenGL上下文和SDL窗口。
     * 最后退出SDL。
     */
    if (audioState.isDeviceInitialized) ma_device_uninit(&audioState.device); // 取消初始化音频设备
    ma_mutex_lock(&audioState.mutex);
    if (audioState.isAudioReady) ma_decoder_uninit(&audioState.decoder); // 取消初始化解码器
    ma_mutex_unlock(&audioState.mutex);
    ma_mutex_uninit(&audioState.mutex); // 取消初始化互斥锁

    ImGui_ImplOpenGL3_Shutdown(); // 关闭OpenGL3 ImGui后端
    ImGui_ImplSDL2_Shutdown(); // 关闭SDL2 ImGui后端
    ImGui::DestroyContext(); // 销毁ImGui上下文
    SDL_GL_DeleteContext(gl_context); // 删除OpenGL上下文
    SDL_DestroyWindow(window); // 销毁SDL窗口
    SDL_Quit(); // 退出SDL
    return 0;
}