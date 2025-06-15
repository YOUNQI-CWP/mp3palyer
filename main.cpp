#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>
#include <SDL.h>
#include <SDL_opengl.h>
#include <string>
#include <vector>
#include <filesystem>
#include <set>
#include <algorithm> 
#include <cctype>    

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// ======== Data Structures ========
struct Song {
    std::string filePath;
    std::string displayName;
};

// REFACTOR: Enhance AudioState with a stopwatch mechanism for progress tracking.
struct AudioState {
    ma_decoder decoder;
    ma_device device;
    ma_mutex mutex;

    bool isAudioReady = false;
    bool isDeviceInitialized = false;
    std::string currentFilePath = "";

    // Stopwatch state for performance
    float totalSongDurationSec = 0.0f;
    Uint32 songStartTime = 0;       // SDL_GetTicks() when play started/resumed
    float elapsedTimeAtPause = 0.0f;
};

// ======== Audio Functions ========

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    AudioState* audioState = (AudioState*)pDevice->pUserData;
    if (audioState == NULL) return;

    ma_mutex_lock(&audioState->mutex);
    if (audioState->isAudioReady) {
        ma_decoder_read_pcm_frames(&audioState->decoder, pOutput, frameCount, NULL);
    }
    ma_mutex_unlock(&audioState->mutex);
    
    (void)pInput;
}

bool PlaySong(AudioState& audioState, const char* filePath) {
    if (audioState.isDeviceInitialized) {
        ma_device_uninit(&audioState.device);
        audioState.isDeviceInitialized = false;
    }

    ma_mutex_lock(&audioState.mutex);
    if (audioState.isAudioReady) {
        ma_decoder_uninit(&audioState.decoder);
    }

    ma_result result = ma_decoder_init_file(filePath, NULL, &audioState.decoder);
    if (result != MA_SUCCESS) {
        printf("Could not load file: %s\n", filePath);
        audioState.isAudioReady = false;
        audioState.currentFilePath = "";
        ma_mutex_unlock(&audioState.mutex);
        return false;
    }
    
    // Calculate total duration once at the beginning. This can be slow for VBR but it's a one-time cost.
    ma_uint64 totalFrames;
    ma_decoder_get_length_in_pcm_frames(&audioState.decoder, &totalFrames);
    audioState.totalSongDurationSec = (totalFrames > 0) ? (float)totalFrames / audioState.decoder.outputSampleRate : 0.0f;

    audioState.isAudioReady = true;
    audioState.currentFilePath = filePath;
    ma_mutex_unlock(&audioState.mutex);

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.performanceProfile = ma_performance_profile_conservative;
    deviceConfig.playback.format = audioState.decoder.outputFormat;
    deviceConfig.playback.channels = audioState.decoder.outputChannels;
    deviceConfig.sampleRate = audioState.decoder.outputSampleRate;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = &audioState;

    if (ma_device_init(NULL, &deviceConfig, &audioState.device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return false;
    }
    audioState.isDeviceInitialized = true;

    if (ma_device_start(&audioState.device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&audioState.device);
        audioState.isDeviceInitialized = false;
        return false;
    }
    
    // Reset stopwatch on new song
    audioState.elapsedTimeAtPause = 0.0f;
    audioState.songStartTime = SDL_GetTicks();

    printf("Playing: %s\n", filePath);
    return true;
}

// ======== UI & Utility Functions ========

void SetModernDarkStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    ImGui::StyleColorsDark();
}

void ScanDirectoryForMusic(const std::string& path, std::vector<Song>& playlist) {
    const std::set<std::string> supported_extensions = {".mp3", ".wav", ".flac"};
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c){ return std::tolower(c); });

                if (supported_extensions.count(extension)) {
                    std::string full_path = entry.path().string();
                    auto it = std::find_if(playlist.begin(), playlist.end(), 
                                           [&](const Song& s){ return s.filePath == full_path; });
                    if (it == playlist.end()) {
                        playlist.push_back({full_path, entry.path().filename().string()});
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        printf("Filesystem error: %s\n", e.what());
    }
}

void ShowPlayerWindow(AudioState& audioState, float& volume, float progress, const std::string& songName, ImVec2 pos, ImVec2 size);
void ShowPlaylistWindow(AudioState& audioState, std::vector<Song>& playlist, ImVec2 pos, ImVec2 size);


// ======== Main Application ========
int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

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
    SDL_GL_SetSwapInterval(1); // Enable VSync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    SetModernDarkStyle();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    AudioState audioState;
    if (ma_mutex_init(&audioState.mutex) != MA_SUCCESS) {
        printf("Failed to initialize mutex.\n");
        return -1;
    }
    float volume = 0.5f;
    std::vector<Song> playlist;

    // Cache for UI state
    float display_progress = 0.0f;
    std::string display_song_name = "No song loaded.";

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) done = true;
        }

        // --- UI State Update using Stopwatch ---
        bool isPlaying = audioState.isDeviceInitialized && ma_device_is_started(&audioState.device);
        if (audioState.isAudioReady) {
            display_song_name = std::filesystem::path(audioState.currentFilePath).filename().string();
            if (isPlaying) {
                float currentSessionTime = (float)(SDL_GetTicks() - audioState.songStartTime) / 1000.0f;
                float totalElapsedTime = audioState.elapsedTimeAtPause + currentSessionTime;
                display_progress = (audioState.totalSongDurationSec > 0) ? (totalElapsedTime / audioState.totalSongDurationSec) : 0.0f;
            } else { // Paused
                display_progress = (audioState.totalSongDurationSec > 0) ? (audioState.elapsedTimeAtPause / audioState.totalSongDurationSec) : 0.0f;
            }
             if(display_progress >= 1.0f) display_progress = 1.0f; // Cap at 100%
        } else {
            display_song_name = "No song loaded.";
            display_progress = 0.0f;
        }
        
        // --- Start ImGui Frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (audioState.isDeviceInitialized) {
            ma_device_set_master_volume(&audioState.device, volume);
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float playerHeight = 110.0f;
        ImVec2 playerPos = ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - playerHeight);
        ImVec2 playerSize = ImVec2(viewport->Size.x, playerHeight);
        ImVec2 playlistPos = viewport->Pos;
        ImVec2 playlistSize = ImVec2(viewport->Size.x, viewport->Size.y - playerHeight);

        ShowPlaylistWindow(audioState, playlist, playlistPos, playlistSize);
        ShowPlayerWindow(audioState, volume, display_progress, display_song_name, playerPos, playerSize);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    if (audioState.isDeviceInitialized) {
        ma_device_uninit(&audioState.device);
    }
    ma_mutex_lock(&audioState.mutex);
    if (audioState.isAudioReady) {
        ma_decoder_uninit(&audioState.decoder);
    }
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

void ShowPlayerWindow(AudioState& audioState, float& volume, float progress, const std::string& songName, ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags player_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("Player", nullptr, player_flags);
    
    ImGui::Text("Now Playing:");
    ImGui::TextWrapped("%s", songName.c_str());
    
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));

    if (ImGui::Button("<< Prev")) { /* TODO */ }
    ImGui::SameLine();

    bool isPlaying = audioState.isDeviceInitialized && ma_device_is_started(&audioState.device);
    if (isPlaying) {
        if (ImGui::Button("Pause")) { 
            ma_device_stop(&audioState.device);
            // Record elapsed time when pausing
            float sessionTime = (float)(SDL_GetTicks() - audioState.songStartTime) / 1000.0f;
            audioState.elapsedTimeAtPause += sessionTime;
        }
    } else {
        if (ImGui::Button("Play ")) {
            if(audioState.isDeviceInitialized) {
                ma_device_start(&audioState.device);
                // Reset stopwatch start time on resume
                audioState.songStartTime = SDL_GetTicks();
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Next >>")) { /* TODO */ }
    ImGui::SameLine();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::SliderFloat("##Volume", &volume, 0.0f, 1.0f, "Volume: %.2f");
    ImGui::PopItemWidth();

    ImGui::End();
}

void ShowPlaylistWindow(AudioState& audioState, std::vector<Song>& playlist, ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags playlist_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Playlist", nullptr, playlist_flags);

    static char folder_path_buf[1024] = "/home/user/Music";
    ImGui::InputText("Folder Path", folder_path_buf, sizeof(folder_path_buf));
    ImGui::SameLine();
    if (ImGui::Button("Scan Folder")) {
        ScanDirectoryForMusic(folder_path_buf, playlist);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        playlist.clear();
    }
    
    ImGui::Separator();

    ma_mutex_lock(&audioState.mutex);
    std::string current_song_path = audioState.currentFilePath;
    ma_mutex_unlock(&audioState.mutex);

    if (ImGui::BeginTable("playlist_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Title");
        ImGui::TableHeadersRow();
        
        ImGuiListClipper clipper;
        clipper.Begin(playlist.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", i + 1);
                ImGui::TableSetColumnIndex(1);
                
                const Song& song = playlist[i];
                bool is_selected = (current_song_path == song.filePath);

                if (ImGui::Selectable(song.displayName.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    PlaySong(audioState, song.filePath.c_str());
                }
            }
        }
        clipper.End();

        ImGui::EndTable();
    }

    ImGui::End();
}
