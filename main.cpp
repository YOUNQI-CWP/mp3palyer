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
#include <fstream> // Required for file I/O
#include <sstream> // Required for string stream

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// ======== Data Structures ========
struct Song {
    std::string filePath;
    std::string displayName;
};

struct AudioState {
    ma_decoder decoder;
    ma_device device;
    ma_mutex mutex;
    bool isAudioReady = false;
    bool isDeviceInitialized = false;
    std::string currentFilePath = "";
    int currentIndex = -1;
    float totalSongDurationSec = 0.0f;
    Uint32 songStartTime = 0;
    float elapsedTimeAtPause = 0.0f;
};

// ======== Forward Declarations ========
bool PlaySongAtIndex(AudioState& audioState, std::vector<Song>& playlist, int index);
void ShowLeftSidebar(ImVec2 pos, ImVec2 size);
void ShowPlayerWindow(AudioState& audioState, std::vector<Song>& playlist, float& volume, float progress, const std::string& songName, ImVec2 pos, ImVec2 size);
void ShowPlaylistWindow(AudioState& audioState, std::vector<Song>& playlist, std::vector<std::string>& musicDirs, ImVec2 pos, ImVec2 size);

// ======== Audio & Config Functions ========

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

bool PlaySongAtIndex(AudioState& audioState, std::vector<Song>& playlist, int index) {
    if (playlist.empty() || index < 0 || index >= playlist.size()) {
        return false;
    }
    const char* filePath = playlist[index].filePath.c_str();
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
        audioState.currentIndex = -1;
        ma_mutex_unlock(&audioState.mutex);
        return false;
    }
    ma_uint64 totalFrames;
    ma_decoder_get_length_in_pcm_frames(&audioState.decoder, &totalFrames);
    audioState.totalSongDurationSec = (totalFrames > 0) ? (float)totalFrames / audioState.decoder.outputSampleRate : 0.0f;
    audioState.isAudioReady = true;
    audioState.currentFilePath = filePath;
    audioState.currentIndex = index;
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
    audioState.elapsedTimeAtPause = 0.0f;
    audioState.songStartTime = SDL_GetTicks();
    printf("Playing: %s\n", playlist[index].displayName.c_str());
    return true;
}

void SaveConfig(const std::vector<std::string>& musicDirs) {
    std::ofstream configFile("config.txt");
    if (configFile.is_open()) {
        for (const auto& dir : musicDirs) {
            configFile << dir << std::endl;
        }
        configFile.close();
    }
}

void LoadConfig(std::vector<std::string>& musicDirs) {
    std::ifstream configFile("config.txt");
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            if (!line.empty()) {
                musicDirs.push_back(line);
            }
        }
        configFile.close();
    }
}

// ======== UI & Utility Functions ========

void SetModernDarkStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
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
    SDL_GL_SetSwapInterval(1);
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
    std::vector<std::string> musicDirs;

    LoadConfig(musicDirs);
    for(const auto& dir : musicDirs) {
        ScanDirectoryForMusic(dir, playlist);
    }

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

        bool isPlaying = audioState.isDeviceInitialized && ma_device_is_started(&audioState.device);
        if (audioState.isAudioReady) {
            display_song_name = playlist[audioState.currentIndex].displayName;
            float totalElapsedTime = 0.0f;
            if (isPlaying) {
                float currentSessionTime = (float)(SDL_GetTicks() - audioState.songStartTime) / 1000.0f;
                totalElapsedTime = audioState.elapsedTimeAtPause + currentSessionTime;
            } else { 
                totalElapsedTime = audioState.elapsedTimeAtPause;
            }
            display_progress = (audioState.totalSongDurationSec > 0) ? (totalElapsedTime / audioState.totalSongDurationSec) : 0.0f;
            if (display_progress >= 1.0f && isPlaying) {
                PlaySongAtIndex(audioState, playlist, (audioState.currentIndex + 1) % playlist.size());
            }
        } else {
            display_song_name = "No song loaded.";
            display_progress = 0.0f;
        }
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if (audioState.isDeviceInitialized) {
            ma_device_set_master_volume(&audioState.device, volume);
        }
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float playerHeight = 90.0f;
        const float leftSidebarWidth = 220.0f;
        ImVec2 playerPos = ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - playerHeight);
        ImVec2 playerSize = ImVec2(viewport->Size.x, playerHeight);
        ImVec2 topAreaPos = viewport->Pos;
        ImVec2 topAreaSize = ImVec2(viewport->Size.x, viewport->Size.y - playerHeight);
        ImVec2 leftSidebarPos = topAreaPos;
        ImVec2 leftSidebarSize = ImVec2(leftSidebarWidth, topAreaSize.y);
        ImVec2 mainContentPos = ImVec2(topAreaPos.x + leftSidebarWidth, topAreaPos.y);
        ImVec2 mainContentSize = ImVec2(topAreaSize.x - leftSidebarWidth, topAreaSize.y);

        ShowLeftSidebar(leftSidebarPos, leftSidebarSize);
        ShowPlaylistWindow(audioState, playlist, musicDirs, mainContentPos, mainContentSize);
        ShowPlayerWindow(audioState, playlist, volume, display_progress, display_song_name, playerPos, playerSize);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

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

void ShowLeftSidebar(ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("Sidebar", nullptr, flags);
    ImGui::PopStyleVar();
    ImGui::Text("Playlists");
    ImGui::Separator();
    if (ImGui::Selectable("Liked Songs")) {};
    if (ImGui::Selectable("My Chill Mix")) {};
    if (ImGui::Selectable("Discover Weekly")) {};
    ImGui::End();
}

void ShowPlayerWindow(AudioState& audioState, std::vector<Song>& playlist, float& volume, float progress, const std::string& songName, ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags player_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("Player", nullptr, player_flags);
    ImGui::PopStyleVar();
    ImGui::BeginGroup();
    {
        float artSize = size.y * 0.8f;
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetCursorScreenPos(), 
            ImVec2(ImGui::GetCursorScreenPos().x + artSize, ImGui::GetCursorScreenPos().y + artSize), 
            IM_COL32(50, 50, 50, 255), 4.0f);
        ImGui::Dummy(ImVec2(artSize, artSize));
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    {
        ImGui::TextWrapped("%s", songName.c_str());
        ImGui::TextDisabled("Artist Name");
        float elapsed_seconds = progress * audioState.totalSongDurationSec;
        int elapsed_mins = (int)elapsed_seconds / 60;
        int elapsed_secs = (int)elapsed_seconds % 60;
        int total_mins = (int)audioState.totalSongDurationSec / 60;
        int total_secs = (int)audioState.totalSongDurationSec % 60;
        char time_str[32];
        sprintf(time_str, "%02d:%02d / %02d:%02d", elapsed_mins, elapsed_secs, total_mins, total_secs);
        float current_progress = progress;
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        if (ImGui::SliderFloat("##Progress", &current_progress, 0.0f, 1.0f, "")) {
            if (audioState.isAudioReady) {
                ma_mutex_lock(&audioState.mutex);
                ma_uint64 targetFrame = (ma_uint64)(audioState.totalSongDurationSec * current_progress * audioState.decoder.outputSampleRate);
                ma_decoder_seek_to_pcm_frame(&audioState.decoder, targetFrame);
                audioState.elapsedTimeAtPause = current_progress * audioState.totalSongDurationSec;
                audioState.songStartTime = SDL_GetTicks();
                ma_mutex_unlock(&audioState.mutex);
            }
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Text("%s", time_str);
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    float controls_width = 150;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - controls_width - 150) / 2);
    ImGui::BeginGroup();
    {
        if (ImGui::Button(" << ")) { 
            if (!playlist.empty()) {
                int prevIndex = (audioState.currentIndex - 1 + playlist.size()) % playlist.size();
                PlaySongAtIndex(audioState, playlist, prevIndex);
            }
        }
        ImGui::SameLine();
        bool isPlaying = audioState.isDeviceInitialized && ma_device_is_started(&audioState.device);
        const char* play_pause_icon = isPlaying ? " >|| " : "  |>  ";
        if (ImGui::Button(play_pause_icon)) { 
            if (isPlaying) {
                ma_device_stop(&audioState.device);
                float sessionTime = (float)(SDL_GetTicks() - audioState.songStartTime) / 1000.0f;
                audioState.elapsedTimeAtPause += sessionTime;
            } else {
                if(audioState.isDeviceInitialized) {
                    ma_device_start(&audioState.device);
                    audioState.songStartTime = SDL_GetTicks();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(" >> ")) { 
            if (!playlist.empty()) {
                int nextIndex = (audioState.currentIndex + 1) % playlist.size();
                PlaySongAtIndex(audioState, playlist, nextIndex);
            }
        }
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 150);
    ImGui::PushItemWidth(140);
    ImGui::SliderFloat("##Volume", &volume, 0.0f, 1.0f, "Volume: %.2f");
    ImGui::PopItemWidth();
    ImGui::End();
}

void ShowPlaylistWindow(AudioState& audioState, std::vector<Song>& playlist, std::vector<std::string>& musicDirs, ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags playlist_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("Playlist", nullptr, playlist_flags);
    ImGui::PopStyleVar();

    // --- Directory Management UI ---
    ImGui::BeginChild("DirManagement", ImVec2(0, 150), true);
    ImGui::Text("Music Directories");
    ImGui::Separator();
    
    static char add_dir_buf[1024] = "";
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 60);
    ImGui::InputText("##AddPath", add_dir_buf, sizeof(add_dir_buf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        if (strlen(add_dir_buf) > 0) {
            musicDirs.push_back(add_dir_buf);
            SaveConfig(musicDirs);
            ScanDirectoryForMusic(add_dir_buf, playlist);
            strcpy(add_dir_buf, "");
        }
    }

    static int selected_dir_idx = -1;
    // FIX: Replace ListBoxHeader/Footer with a standard scrolling child window
    ImGui::BeginChild("##DirList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.5f), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < musicDirs.size(); ++i) {
        if (ImGui::Selectable(musicDirs[i].c_str(), selected_dir_idx == i)) {
            selected_dir_idx = i;
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Remove Selected") && selected_dir_idx != -1) {
        musicDirs.erase(musicDirs.begin() + selected_dir_idx);
        SaveConfig(musicDirs);
        selected_dir_idx = -1; 
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan All")) {
        playlist.clear();
        for (const auto& dir : musicDirs) {
            ScanDirectoryForMusic(dir, playlist);
        }
    }
    ImGui::EndChild();
    
    // --- Playlist Table ---
    ImGui::Text("Current Playlist");
    if (ImGui::BeginTable("playlist_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 40.0f);
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
                bool is_selected = (audioState.currentIndex == i);
                if (ImGui::Selectable(song.displayName.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        PlaySongAtIndex(audioState, playlist, i);
                    }
                }
            }
        }
        clipper.End();
        ImGui::EndTable();
    }
    ImGui::End();
}
