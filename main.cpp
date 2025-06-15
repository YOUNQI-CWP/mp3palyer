#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <atomic>
#include <mutex>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

namespace fs = std::filesystem;

struct AudioState {
    ma_decoder* pDecoder = nullptr;
    std::vector<fs::path> playlist;
    int current_song_index = -1;
    std::atomic<ma_uint64> cursor_frames = 0;
    ma_uint64 total_frames = 0;
};

AudioState g_audio_state;
std::mutex g_audio_mutex;
ma_device g_device;
std::atomic<bool> g_device_inited = false;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    std::lock_guard<std::mutex> lock(g_audio_mutex);
    if (g_audio_state.pDecoder == nullptr) {
        memset(pOutput, 0, frameCount * pDevice->playback.channels * ma_get_bytes_per_sample(pDevice->playback.format));
        return;
    }
    ma_uint64 framesRead;
    ma_decoder_read_pcm_frames(g_audio_state.pDecoder, pOutput, frameCount, &framesRead);
    g_audio_state.cursor_frames.fetch_add(framesRead);
}

void load_song(int song_index) {
    if (g_device_inited) ma_device_stop(&g_device);
    ma_decoder* pOldDecoder = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_audio_mutex);
        pOldDecoder = g_audio_state.pDecoder;
        g_audio_state.pDecoder = nullptr;
        g_audio_state.current_song_index = -1;
        g_audio_state.cursor_frames = 0;
        g_audio_state.total_frames = 0;
    }
    if (pOldDecoder) { ma_decoder_uninit(pOldDecoder); delete pOldDecoder; }
    if (song_index < 0 || song_index >= g_audio_state.playlist.size()) return;

    ma_decoder* pNewDecoder = new ma_decoder;
    const char* filepath = g_audio_state.playlist[song_index].c_str();
    if (ma_decoder_init_file(filepath, NULL, pNewDecoder) != MA_SUCCESS) { delete pNewDecoder; return; }
    {
        std::lock_guard<std::mutex> lock(g_audio_mutex);
        g_audio_state.pDecoder = pNewDecoder;
        g_audio_state.current_song_index = song_index;
        ma_decoder_get_length_in_pcm_frames(g_audio_state.pDecoder, &g_audio_state.total_frames);
    }
    if (g_device_inited) ma_device_start(&g_device);
}

void scan_music_directory(const std::string& path) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".mp3" || ext == ".MP3") g_audio_state.playlist.push_back(fs::absolute(entry.path()));
            }
        }
    } catch (const fs::filesystem_error& e) { std::cerr << "Filesystem error: " << e.what() << std::endl; }
}

int main(int argc, char* argv[]) {
    scan_music_directory("./music");

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_f32; deviceConfig.playback.channels = 2; deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = data_callback;
    if (ma_device_init(NULL, &deviceConfig, &g_device) == MA_SUCCESS) g_device_inited = true;
    else std::cerr << "Failed to initialize audio device." << std::endl;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    
    // --- 核心修正行 ---
    SDL_Window* window = SDL_CreateWindow("MP3 Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui_ImplSDL2_InitForOpenGL(window, gl_context); ImGui_ImplOpenGL3_Init("#version 330");

    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) { ImGui_ImplSDL2_ProcessEvent(&e); if (e.type == SDL_QUIT) quit = true; }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
        
        ImGui::Begin("MP3 Player");
        
        ImGui::BeginChild("Playlist", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 3));
        for (int i = 0; i < g_audio_state.playlist.size(); ++i) {
            if (ImGui::Selectable(g_audio_state.playlist[i].filename().c_str(), g_audio_state.current_song_index == i)) {
                load_song(i);
            }
        }
        ImGui::EndChild();

        float progress = 0.0f;
        ma_uint64 cursor = g_audio_state.cursor_frames.load();
        ma_uint64 length = g_audio_state.total_frames;
        if (length > 0) progress = (float)cursor / (float)length;
        ImGui::ProgressBar(progress);
        
        if (g_audio_state.current_song_index != -1 && cursor >= length && length > 0) {
            load_song(g_audio_state.current_song_index + 1);
        }

        bool is_playing = g_device_inited && ma_device_get_state(&g_device) == ma_device_state_started;
        bool song_loaded = g_audio_state.current_song_index != -1;
        
        auto switch_song = [&](int direction) {
            int next_index = (direction == 0) ? 0 : g_audio_state.current_song_index + direction;
            load_song(next_index);
        };

        if (ImGui::Button("<< Prev")) { switch_song(-1); } ImGui::SameLine();
        if (is_playing) {
            if (ImGui::Button("Pause")) { if (g_device_inited) ma_device_stop(&g_device); }
        } else {
            if (ImGui::Button("Play ")) {
                if (song_loaded && g_device_inited) { ma_device_start(&g_device); }
                else if (!song_loaded && !g_audio_state.playlist.empty()) { switch_song(0); }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop ")) {
            if (g_device_inited) ma_device_stop(&g_device);
            load_song(-1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Next >>")) { switch_song(1); }
        
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(0.1f, 0.12f, 0.14f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    if (g_device_inited) ma_device_uninit(&g_device);
    if (g_audio_state.pDecoder) { ma_decoder_uninit(g_audio_state.pDecoder); delete g_audio_state.pDecoder; }
    
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}