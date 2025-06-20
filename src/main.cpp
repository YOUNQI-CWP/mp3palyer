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
#include <thread> // 为了线程安全播放欢迎音

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

#include <iostream>
#include <string>
#include <atomic>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "sparkchain.h"
#include "sc_tts.h"

using namespace SparkChain;
using namespace std;

// --- 全局区域 ---
// 异步状态标志，用于在异步转同步时等待结果
static atomic_bool finish(false);

// SDK 初始化参数 (请替换为您自己的)
const char *APPID     = "894244c6";
const char *APIKEY    = "97fc61dbeea347213ec7db4d37e15e46";
const char *APISECRET = "YjgwZWMzM2M2NGI2OWVmZDY0MmNiMjFk";
const char *WORKDIR   = "./";
int logLevel          = 100; // 100 表示关闭日志

// --- 核心组件 ---

/**
 * @brief 回调处理类，用于接收SDK返回的音频数据和错误信息
 * 通过 usrTag 机制，每次回调都能获取对应的文件路径，实现了无状态可重入
 */
class onlineTTSCallbacks : public TTSCallbacks {
    void onResult(TTSResult * result, void * usrTag) override {
        const char* currentAudioPath = static_cast<const char*>(usrTag);
        const char * data = result->data();
        int len           = result->len();
        int status        = result->status();

        FILE* file = fopen(currentAudioPath, "ab"); // 以二进制追加模式写入
        if (file) {
            fwrite(data, 1, len, file);
            fclose(file);
        }

        if (status == 2) { // status=2 表示这是最后的数据块，合成完成
            finish = true; // 通知主线程任务已完成
        }
    }

    void onError(TTSError * error, void * usrTag) override {
        printf("请求出错, 错误码: %d, 错误信息:%s\n", error->code(), error->errMsg().c_str());
        finish = true; // 即使出错也要通知主线程，避免无限等待
    }
};


/**
 * @brief (您想要的函数) 将文本转换为语音并保存到指定文件
 * 这是一个完全封装的函数，调用即可完成单次转换。
 *
 * @param text 要合成的文本 (UTF-8编码)
 * @param outputPath 生成的音频文件保存路径 (例如 "./hello.mp3")
 * @param vcn 发音人，默认为 "讯飞小燕"
 * @return bool true 表示合成流程成功结束, false 表示请求失败或超时
 */
bool Text_to_TTS(const char* text, const char* outputPath, const char* vcn = "xiaoyan") {
    // --- 1. 函数内部准备 ---
    OnlineTTS tts(vcn);           // 创建TTS引擎，可指定发音人
    tts.aue("lame");              // 设置音频编码为mp3
    onlineTTSCallbacks cbs{};     // 创建回调处理器
    tts.registerCallbacks(&cbs);  // 注册回调

    // --- 2. 核心逻辑 ---
    remove(outputPath); // 删除旧文件，确保从零开始
    finish = false;     // 重置等待标志

    printf("开始合成: \"%s\" -> %s\n", text, outputPath);

    // 发起异步请求，并将 outputPath 作为用户自定义数据(usrTag)传入
    int ret = tts.arun(text, (void*)outputPath);
    if (ret != 0) {
        printf("合成请求发送失败, 错误码: %d\n", ret);
        return false;
    }

    // --- 3. 等待结果 ---
    int times = 0;
    while (!finish) {
        sleep(1);
        if (times++ > 20) { // 超时设置为20秒
            printf("合成超时！\n");
            return false;
        }
    }
    printf("合成流程结束。\n");
    return true;
}


// --- SDK 初始化与反初始化 (保持不变) ---
int initSDK() {
    SparkChainConfig *config = SparkChainConfig::builder();
    config->appID(APPID)->apiKey(APIKEY)->apiSecret(APISECRET)->workDir(WORKDIR)->logLevel(logLevel);
    return SparkChain::init(config);
}

void uninitSDK() {
    SparkChain::unInit();
}

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
 * @brief 在单独的线程中播放欢迎音的函数。
 * 此函数会初始化一个独立的miniaudio引擎和sound对象，
 * 播放指定音频，并等待其播放完成后再清理资源。
 * @param path 音频文件路径
 */
void playWelcomeSound(const char* path) {
    if (!std::filesystem::exists(path)) {
        printf("Welcome sound file not found: %s\n", path);
        return;
    }

    ma_engine engine;
    ma_result result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize welcome audio engine.\n");
        return;
    }

    ma_sound sound;
    // 使用 ma_sound 从文件初始化，这允许我们之后检查它的状态
    result = ma_sound_init_from_file(&engine, path, 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        printf("Failed to init welcome sound from file: %s\n", path);
        ma_engine_uninit(&engine); // 初始化sound失败也要释放引擎
        return;
    }

    ma_sound_start(&sound);

    // 循环等待，直到声音播放结束
    // ma_sound_at_end() 是检查声音是否播放完毕的正确方法
    while (ma_sound_at_end(&sound) == MA_FALSE) {
        ma_sleep(100); // 休眠100毫秒，避免CPU空转
    }

    // 播放完毕，清理资源
    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
}


// +++ 已修正的功能函数 +++
/**
 * @brief 生成并播放歌曲开始前的播报提示音。
 * 该函数会根据歌曲信息生成一段TTS语音，如“现在为您播放由[歌手]演唱的[歌曲]”。
 * 语音文件会保存在程序运行目录下的 "tts_cache" 文件夹中。
 * 如果提示音已存在，则直接播放。播放过程是阻塞的，直到提示音播放完毕。
 *
 * @param song 要播放的歌曲对象，需要包含 artist, displayName 成员。
 * @return bool true 表示提示音播放成功, false 表示失败。
 */
bool playSongAnnouncement(const Song& song) {
    // 1. 构造播报文本和目标文件名
    if (song.artist.empty() || song.displayName.empty()) {
        printf("Error: Song object is missing artist or displayName.\n");
        return false;
    }

    std::string announcementText = "现在为您播放由" + song.artist + "演唱的" + song.displayName;
    
    // +++ 路径修改逻辑 +++
    // 1. 定义一个专门存放提示音的目录
    const std::string cacheDir = "tts_cache";
    
    // 2. 确保这个目录存在，如果不存在就创建它
    if (!std::filesystem::exists(cacheDir)) {
        std::filesystem::create_directory(cacheDir);
    }

    // 3. 基于歌曲信息创建一个不会重复且合法的文件名
    std::string tipFileName = song.artist + "-" + song.displayName + "-tip.mp3";

    // 4. 将目录和文件名组合成最终路径
    std::string tipFilePath = (std::filesystem::path(cacheDir) / tipFileName).string();
    // --- 路径修改逻辑结束 ---

    // 2. 检查提示音文件是否存在，不存在则调用TTS生成
    if (!std::filesystem::exists(tipFilePath)) {
        printf("Tip sound not found, generating: %s\n", tipFilePath.c_str());
        // Text_to_TTS 是阻塞的，会等待文件生成完毕
        bool success = Text_to_TTS(announcementText.c_str(), tipFilePath.c_str());
        if (!success) {
            printf("Failed to generate tip sound.\n");
            return false; // 生成失败，直接返回
        }
    }

    // 3. 阻塞式播放提示音
    printf("Playing tip sound: %s\n", tipFilePath.c_str());
    ma_engine engine;
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
        printf("Failed to initialize tip audio engine.\n");
        return false;
    }

    ma_sound sound;
    if (ma_sound_init_from_file(&engine, tipFilePath.c_str(), 0, NULL, NULL, &sound) != MA_SUCCESS) {
        printf("Failed to init tip sound from file: %s\n", tipFilePath.c_str());
        ma_engine_uninit(&engine);
        return false;
    }
    
    ma_sound_start(&sound);
    while (ma_sound_at_end(&sound) == MA_FALSE) {
        ma_sleep(100); // 循环等待直到声音播放结束
    }

    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
    
    printf("Tip sound finished.\n");
    return true;
}
// --- 功能函数结束 ---


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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    if (initSDK() != 0) {
        printf("SDK 初始化失败!\n");
        return -1;
    }

    // 在一个独立的线程中播放欢迎音，不阻塞主程序启动
    std::thread welcomeThread(playWelcomeSound, "res/welcome.mp3");
    welcomeThread.detach(); // 分离线程，让它在后台自由运行

    // 2. 设置 OpenGL
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
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 启用键盘导航

    // 4. 加载字体
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
    SetModernDarkStyle();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 6. 初始化应用程序状态
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
    uninitSDK(); // 程序退出前，释放SDK资源
    SDL_Quit(); // 退出SDL
    return 0;
}