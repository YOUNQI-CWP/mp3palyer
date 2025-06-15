#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <iostream>
#include <atomic>

// Dear ImGui Headers
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_opengl3.h"

// miniaudio - 在一个CPP文件中定义实现
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

// 全局音频变量
ma_decoder decoder;
ma_device device;
std::atomic<bool> g_audio_inited = false;

// miniaudio数据回调函数
// 当音频设备需要更多数据时，会从一个独立的线程调用这个函数
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return;
    }

    // 从解码器读取PCM帧到输出缓冲区
    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, NULL);

    (void)pInput; // 未使用的参数
}


int main(int argc, char* argv[]) {
    // --- 音频初始化 ---
    ma_result result;
    // 1. 初始化解码器
    result = ma_decoder_init_file("test.mp3", NULL, &decoder);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to open MP3 file: test.mp3" << std::endl;
        // 即使音频失败，我们仍然可以运行UI
    } else {
        // 2. 配置音频设备
        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format   = decoder.outputFormat;
        deviceConfig.playback.channels = decoder.outputChannels;
        deviceConfig.sampleRate        = decoder.outputSampleRate;
        deviceConfig.dataCallback      = data_callback;
        deviceConfig.pUserData         = &decoder; // 将解码器指针传递给回调函数

        // 3. 初始化音频设备
        if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
            std::cerr << "Failed to initialize playback device." << std::endl;
            ma_decoder_uninit(&decoder);
        } else {
            // 4. 开始播放
            if (ma_device_start(&device) != MA_SUCCESS) {
                std::cerr << "Failed to start playback device." << std::endl;
                ma_device_uninit(&device);
                ma_decoder_uninit(&decoder);
            } else {
                g_audio_inited = true; // 标记音频已成功初始化并开始播放
                std::cout << "Audio playback started." << std::endl;
            }
        }
    }

    // --- SDL和ImGui初始化 (与上一步相同) ---
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    const char* glsl_version = "#version 330";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_Window* window = SDL_CreateWindow("MP3 Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // --- 主循环 ---
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        // --- UI 渲染 ---
        ImGui::Begin("Player");
        if (g_audio_inited) {
            ImGui::Text("File: %s", "test.mp3");
            ImGui::Text("Sample Rate: %d Hz", device.sampleRate);
            ImGui::Text("Channels: %d", device.playback.channels);
        } else {
            ImGui::Text("Failed to load audio.");
        }
        ImGui::End();

        // --- 渲染 ---
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.12f, 0.14f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // --- 清理 ---
    if (g_audio_inited) {
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}