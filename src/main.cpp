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

// Define global fonts
ImFont* G_Font_Default = nullptr;
ImFont* G_Font_Large = nullptr;

// Main Application
int main(int, char**) {
    // 1. Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // 2. Setup OpenGL
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
    SDL_GL_SetSwapInterval(1);

    // 3. Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 4. Load Fonts
    const char* font_path = "font.ttf";
    if (std::filesystem::exists(font_path)) {
        ImFontConfig config;
        G_Font_Default = io.Fonts->AddFontFromFileTTF(font_path, 18.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
        
        config.MergeMode = true;
        static const ImWchar icon_ranges[] = { 0x2661, 0x2665, 0 }; // ♡ ♥
        io.Fonts->AddFontFromFileTTF(font_path, 18.0f, &config, icon_ranges);
        io.Fonts->AddFontFromFileTTF(font_path, 18.0f, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

        G_Font_Large = io.Fonts->AddFontFromFileTTF(font_path, 28.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
        io.Fonts->AddFontFromFileTTF(font_path, 28.0f, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    } else {
        printf("WARNING: Font file '%s' not found. Please add it to the executable directory.\n", font_path);
    }
    
    // 5. Setup Style and Backends
    SetModernDarkStyle();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 6. Initialize Application State
    AudioState audioState;
    if (ma_mutex_init(&audioState.mutex) != MA_SUCCESS) return -1;

    float volume = 0.5f;
    std::vector<Song> mainPlaylist;
    std::vector<Song> likedSongsPlaylist;
    std::vector<std::string> musicDirs;
    std::set<std::string> likedPaths;
    ActiveView currentView = ActiveView::Main;
    float display_progress = 0.0f;

    LoadConfig(musicDirs);
    LoadLikedSongs(likedPaths);
    for(const auto& dir : musicDirs) {
        ScanDirectoryForMusic(dir, mainPlaylist, likedPaths);
    }
    
    // 7. Main Loop
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) done = true;
        }

        // Update state
        bool isPlaying = audioState.isDeviceInitialized && ma_device_is_started(&audioState.device);
        if (audioState.isAudioReady) {
            float totalElapsedTime = isPlaying ? audioState.elapsedTimeAtPause + (float)(SDL_GetTicks() - audioState.songStartTime) / 1000.0f : audioState.elapsedTimeAtPause;
            display_progress = (audioState.totalSongDurationSec > 0) ? (totalElapsedTime / audioState.totalSongDurationSec) : 0.0f;

            if (display_progress >= 1.0f && isPlaying) {
                std::vector<Song>& activePlaylist = (currentView == ActiveView::Main) ? mainPlaylist : likedSongsPlaylist;
                int nextIndex = GetNextSongIndex(audioState, activePlaylist.size());
                PlaySongAtIndex(audioState, activePlaylist, nextIndex, PlayDirection::New);
            }
        } else {
            display_progress = 0.0f;
        }

        // Start new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (audioState.isDeviceInitialized) ma_device_set_master_volume(&audioState.device, volume);

        // Define layout
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

        // Render UI from modules
        ShowLeftSidebar(leftSidebarPos, leftSidebarSize, currentView);
        ShowPlaylistWindow(audioState, mainPlaylist, likedSongsPlaylist, musicDirs, currentView, mainContentPos, mainContentSize);
        ShowPlayerWindow(audioState, mainPlaylist, activePlaylist, volume, display_progress, playerPos, playerSize);

        // Render frame
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // 8. Cleanup
    if (audioState.isDeviceInitialized) ma_device_uninit(&audioState.device);
    ma_mutex_lock(&audioState.mutex);
    if (audioState.isAudioReady) ma_decoder_uninit(&audioState.decoder);
    ma_mutex_unlock(&audioState.mutex);
    ma_mutex_uninit(&audioState.mutex);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}