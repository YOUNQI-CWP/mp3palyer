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
#include <fstream>
#include <sstream>
#include <random> // For shuffle and random selection

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// ======== Global Fonts ========
ImFont* G_Font_Default = nullptr;
ImFont* G_Font_Large = nullptr;

// ======== Enums & Data Structures ========
enum class PlayMode { ListLoop, RepeatOne, Shuffle };
enum class ActiveView { Main, LikedSongs };
enum class PlayDirection { New, Back };

// ==================== MODIFICATION: ADD METADATA FIELDS ====================
struct Song {
    std::string filePath;
    std::string displayName;
    std::string artist; // 新增：艺术家
    std::string album;  // 新增：专辑
    bool isLiked = false;
};
// =========================================================================

struct AudioState {
    ma_decoder decoder;
    ma_device device;
    ma_mutex mutex;

    bool isAudioReady = false;
    bool isDeviceInitialized = false;
    std::string currentFilePath = "";
    int currentIndex = -1;

    PlayMode playMode = PlayMode::ListLoop;
    
    std::vector<int> playHistory;

    float totalSongDurationSec = 0.0f;
    Uint32 songStartTime = 0;
    float elapsedTimeAtPause = 0.0f;
};

// ======== Forward Declarations ========
bool PlaySongAtIndex(AudioState& audioState, std::vector<Song>& playlist, int index, PlayDirection direction = PlayDirection::New);
void ShowLeftSidebar(ImVec2 pos, ImVec2 size, ActiveView& currentView);
void ShowPlayerWindow(AudioState& audioState, std::vector<Song>& mainPlaylist, std::vector<Song>& activePlaylist, float& volume, float progress, ImVec2 pos, ImVec2 size);
void ShowPlaylistWindow(AudioState& audioState, std::vector<Song>& mainPlaylist, std::vector<Song>& likedPlaylist, std::vector<std::string>& musicDirs, ActiveView currentView, ImVec2 pos, ImVec2 size);
int GetNextSongIndex(AudioState& audioState, int listSize);


// ======== Audio, Config & Logic Functions ========

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

int GetNewRandomIndex(int current, int listSize) {
    if (listSize <= 1) return 0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, listSize - 1);
    int nextIndex;
    do {
        nextIndex = distrib(gen);
    } while (nextIndex == current);
    return nextIndex;
}


int GetNextSongIndex(AudioState& audioState, int listSize) {
    if (listSize == 0) return -1;

    switch (audioState.playMode) {
        case PlayMode::RepeatOne:
            return audioState.currentIndex;
        case PlayMode::Shuffle:
            return GetNewRandomIndex(audioState.currentIndex, listSize);
        case PlayMode::ListLoop:
        default:
            return (audioState.currentIndex + 1) % listSize;
    }
}

bool PlaySongAtIndex(AudioState& audioState, std::vector<Song>& playlist, int index, PlayDirection direction) {
    if (playlist.empty() || index < 0 || index >= playlist.size()) {
        return false;
    }

    if (direction == PlayDirection::New && audioState.currentIndex != -1) {
        audioState.playHistory.push_back(audioState.currentIndex);
    }

    const char* filePath = playlist[index].filePath.c_str();
    if (audioState.isDeviceInitialized) ma_device_uninit(&audioState.device);

    ma_mutex_lock(&audioState.mutex);
    
    if (audioState.isAudioReady) ma_decoder_uninit(&audioState.decoder);

    ma_result result = ma_decoder_init_file(filePath, NULL, &audioState.decoder);
    if (result != MA_SUCCESS) {
        printf("Could not load file: %s\n", filePath);
        audioState.isAudioReady = false;
        ma_mutex_unlock(&audioState.mutex);
        return false;
    }

    ma_uint64 totalFrames;
    ma_decoder_get_length_in_pcm_frames(&audioState.decoder, &totalFrames);
    audioState.totalSongDurationSec = (totalFrames > 0) ? (float)totalFrames / audioState.decoder.outputSampleRate : 0.0f;
    audioState.isAudioReady = true;
    audioState.currentFilePath = filePath;
    audioState.currentIndex = index;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.performanceProfile = ma_performance_profile_conservative;
    deviceConfig.playback.format = audioState.decoder.outputFormat;
    deviceConfig.playback.channels = audioState.decoder.outputChannels;
    deviceConfig.sampleRate = audioState.decoder.outputSampleRate;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = &audioState;

    ma_mutex_unlock(&audioState.mutex);

    if (ma_device_init(NULL, &deviceConfig, &audioState.device) != MA_SUCCESS) {
        return false;
    }

    audioState.isDeviceInitialized = true;
    if (ma_device_start(&audioState.device) != MA_SUCCESS) return false;

    audioState.elapsedTimeAtPause = 0.0f;
    audioState.songStartTime = SDL_GetTicks();

    printf("Playing: %s\n", playlist[index].displayName.c_str());
    return true;
}

void SaveLikedSongs(const std::vector<Song>& playlist) {
    std::ofstream likedFile("liked_songs.txt");
    if (likedFile.is_open()) {
        for (const auto& song : playlist) {
            if (song.isLiked) {
                likedFile << song.filePath << std::endl;
            }
        }
    }
}

void LoadLikedSongs(std::set<std::string>& likedPaths) {
    std::ifstream likedFile("liked_songs.txt");
    if (likedFile.is_open()) {
        std::string line;
        while (std::getline(likedFile, line)) {
            if (!line.empty()) {
                likedPaths.insert(line);
            }
        }
    }
}

void SaveConfig(const std::vector<std::string>& musicDirs) {
    std::ofstream configFile("config.txt");
    if (configFile.is_open()) {
        for (const auto& dir : musicDirs) {
            configFile << dir << std::endl;
        }
    }
}

void LoadConfig(std::vector<std::string>& musicDirs) {
    std::ifstream configFile("config.txt");
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            if (!line.empty()) musicDirs.push_back(line);
        }
    }
}

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

// ==================== MODIFICATION: PARSE METADATA FROM FILENAME ====================
void ScanDirectoryForMusic(const std::string& path, std::vector<Song>& playlist, const std::set<std::string>& likedPaths) {
    const std::set<std::string> supported_extensions = {".mp3", ".wav", ".flac"};
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c){ return std::tolower(c); });

                if (supported_extensions.count(extension)) {
                    std::string full_path = entry.path().string();
                    auto it = std::find_if(playlist.begin(), playlist.end(), [&](const Song& s){ return s.filePath == full_path; });

                    if (it == playlist.end()) {
                        Song newSong;
                        newSong.filePath = full_path;
                        newSong.isLiked = likedPaths.count(full_path);

                        // --- 从文件名解析元数据 ---
                        std::string filename = entry.path().stem().string(); // 获取不带扩展名的文件名
                        std::stringstream ss(filename);
                        std::string segment;
                        std::vector<std::string> segments;
                        while(std::getline(ss, segment, '-')) {
                            // Trim whitespace
                            size_t first = segment.find_first_not_of(' ');
                            if (std::string::npos == first) continue;
                            size_t last = segment.find_last_not_of(' ');
                            segments.push_back(segment.substr(first, (last - first + 1)));
                        }

                        if (segments.size() >= 2) { // 格式: Artist - Title
                            newSong.artist = segments[0];
                            newSong.displayName = segments[1];
                            newSong.album = "未知专辑"; // 默认
                            if (segments.size() >= 3) { // 格式: Artist - Album - Title
                                newSong.album = segments[1];
                                newSong.displayName = segments[2];
                            }
                        } else { // 无法解析
                            newSong.displayName = filename;
                            newSong.artist = "未知艺术家";
                            newSong.album = "未知专辑";
                        }
                        
                        playlist.push_back(newSong);
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        printf("Filesystem error: %s\n", e.what());
    }
}
// ===================================================================================

// ======== Main Application ========
int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) return -1;

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
        printf("WARNING: Font file '%s' not found. Please add it to the executable directory to see Chinese characters and icons.\n", font_path);
    }

    SetModernDarkStyle();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    AudioState audioState;
    if (ma_mutex_init(&audioState.mutex) != MA_SUCCESS) return -1;

    float volume = 0.5f;
    std::vector<Song> mainPlaylist;
    std::vector<Song> likedSongsPlaylist;
    std::vector<std::string> musicDirs;
    std::set<std::string> likedPaths;

    LoadConfig(musicDirs);
    LoadLikedSongs(likedPaths);
    for(const auto& dir : musicDirs) {
        ScanDirectoryForMusic(dir, mainPlaylist, likedPaths);
    }

    ActiveView currentView = ActiveView::Main;
    float display_progress = 0.0f;

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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (audioState.isDeviceInitialized) ma_device_set_master_volume(&audioState.device, volume);

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

        ShowLeftSidebar(leftSidebarPos, leftSidebarSize, currentView);
        ShowPlaylistWindow(audioState, mainPlaylist, likedSongsPlaylist, musicDirs, currentView, mainContentPos, mainContentSize);
        ShowPlayerWindow(audioState, mainPlaylist, activePlaylist, volume, display_progress, playerPos, playerSize);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

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

void ShowLeftSidebar(ImVec2 pos, ImVec2 size, ActiveView& currentView) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("Sidebar", nullptr, flags);

    if (ImGui::Selectable(u8"主列表", currentView == ActiveView::Main)) { currentView = ActiveView::Main; }
    if (ImGui::Selectable(u8"我喜欢的音乐", currentView == ActiveView::LikedSongs)) { currentView = ActiveView::LikedSongs; }
    
    ImGui::End();
    ImGui::PopStyleVar();
}

// ==================== MODIFICATION: ENTIRE ShowPlayerWindow FUNCTION REVAMPED ====================
void ShowPlayerWindow(AudioState& audioState, std::vector<Song>& mainPlaylist, std::vector<Song>& activePlaylist, float& volume, float progress, ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags player_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::Begin("Player", nullptr, player_flags);

    Song* currentSongInActive = nullptr;
    if (audioState.isAudioReady && audioState.currentIndex >= 0 && audioState.currentIndex < activePlaylist.size()) {
        currentSongInActive = &activePlaylist[audioState.currentIndex];
    }
    
    if (ImGui::BeginTable("PlayerLayout", 3, ImGuiTableFlags_SizingStretchSame)) {
        // --- 左侧栏：歌曲信息 ---
        ImGui::TableNextColumn();
        {
            float artSize = size.y * 0.8f;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + artSize, p.y + artSize), IM_COL32(40, 40, 40, 255), 4.0f);
            ImGui::Dummy(ImVec2(artSize, artSize));
            
            ImGui::SameLine(0, 10.0f);

            // 垂直居中对齐歌曲信息
            float infoHeight = (G_Font_Large ? G_Font_Large->FontSize : 18.0f) + (G_Font_Default ? G_Font_Default->FontSize : 18.0f) + ImGui::GetStyle().ItemSpacing.y;
            float infoOffsetY = (artSize - infoHeight) / 2.0f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + infoOffsetY);

            ImGui::BeginGroup();
            if (currentSongInActive) {
                if (G_Font_Large) ImGui::PushFont(G_Font_Large);
                ImGui::Text("%s", currentSongInActive->displayName.c_str());
                if (G_Font_Large) ImGui::PopFont();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("%s - %s", currentSongInActive->artist.c_str(), currentSongInActive->album.c_str());
                ImGui::PopStyleColor();
            } else {
                if (G_Font_Large) ImGui::PushFont(G_Font_Large);
                ImGui::Text("No Song Loaded");
                if (G_Font_Large) ImGui::PopFont();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("Unknown Artist - Unknown Album");
                ImGui::PopStyleColor();
            }
            ImGui::EndGroup();
        }

        // --- 中间栏：播放控制 ---
        ImGui::TableNextColumn();
        {
            float controlsHeight = ImGui::GetFrameHeight();
            float sliderHeight = ImGui::GetFrameHeight();
            float totalContentHeight = controlsHeight + sliderHeight + ImGui::GetStyle().ItemSpacing.y * 2;
            
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (size.y - totalContentHeight) / 2.0f);

            float controls_width = 150.0f; 
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - controls_width) / 2);
            ImGui::BeginGroup();
            if (ImGui::Button("<<", ImVec2(40, controlsHeight))) {
                if (!audioState.playHistory.empty()) {
                    int prevIndex = audioState.playHistory.back();
                    audioState.playHistory.pop_back();
                    PlaySongAtIndex(audioState, activePlaylist, prevIndex, PlayDirection::Back);
                } else if (audioState.playMode == PlayMode::Shuffle && !activePlaylist.empty()) {
                    PlaySongAtIndex(audioState, activePlaylist, GetNextSongIndex(audioState, activePlaylist.size()), PlayDirection::New);
                } else if (!activePlaylist.empty()) {
                    int prevIndex = (audioState.currentIndex - 1 + activePlaylist.size()) % activePlaylist.size();
                    PlaySongAtIndex(audioState, activePlaylist, prevIndex, PlayDirection::New);
                }
            }
            ImGui::SameLine();
            bool isPlaying = audioState.isDeviceInitialized && ma_device_is_started(&audioState.device);
            if (ImGui::Button(isPlaying ? "||" : ">", ImVec2(50, controlsHeight))) {
                if (isPlaying) {
                    ma_device_stop(&audioState.device);
                    audioState.elapsedTimeAtPause += (float)(SDL_GetTicks() - audioState.songStartTime) / 1000.0f;
                } else if(audioState.isDeviceInitialized) {
                    ma_device_start(&audioState.device);
                    audioState.songStartTime = SDL_GetTicks();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(">>", ImVec2(40, controlsHeight))) {
                if (!activePlaylist.empty()) {
                    PlaySongAtIndex(audioState, activePlaylist, GetNextSongIndex(audioState, activePlaylist.size()), PlayDirection::New);
                }
            }
            ImGui::EndGroup();

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.1f);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
            
            float elapsed_seconds = progress * audioState.totalSongDurationSec;
            char time_str_elapsed[16], time_str_total[16];
            sprintf(time_str_elapsed, "%02d:%02d", (int)elapsed_seconds / 60, (int)elapsed_seconds % 60);
            sprintf(time_str_total, "%02d:%02d", (int)audioState.totalSongDurationSec / 60, (int)audioState.totalSongDurationSec % 60);

            ImGui::Text("%s", time_str_elapsed);
            ImGui::SameLine();
            if (ImGui::SliderFloat("##Progress", &progress, 0.0f, 1.0f, "")) {
                if (audioState.isAudioReady) {
                    ma_mutex_lock(&audioState.mutex);
                    ma_uint64 targetFrame = (ma_uint64)(progress * audioState.totalSongDurationSec * audioState.decoder.outputSampleRate);
                    ma_decoder_seek_to_pcm_frame(&audioState.decoder, targetFrame);
                    audioState.elapsedTimeAtPause = progress * audioState.totalSongDurationSec;
                    audioState.songStartTime = SDL_GetTicks();
                    ma_mutex_unlock(&audioState.mutex);
                }
            }
            ImGui::SameLine();
            ImGui::Text("%s", time_str_total);
            ImGui::PopItemWidth();
        }

        // --- 右侧栏：音量和模式 ---
        ImGui::TableNextColumn();
        {
            float controlsHeight = ImGui::GetFrameHeight();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (size.y - controlsHeight - ImGui::GetStyle().WindowPadding.y*2) / 2.0f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.1f);
            ImGui::BeginGroup();
            
            Song* currentSongInMain = nullptr;
            if (currentSongInActive) {
                 auto it = std::find_if(mainPlaylist.begin(), mainPlaylist.end(), [&](const Song& s){ return s.filePath == currentSongInActive->filePath; });
                 if (it != mainPlaylist.end()) currentSongInMain = &(*it);
            }
            if (currentSongInMain) {
                const char* heartIcon = currentSongInMain->isLiked ? u8"♥" : u8"♡";
                if (ImGui::Button(heartIcon)) {
                    currentSongInMain->isLiked = !currentSongInMain->isLiked;
                    SaveLikedSongs(mainPlaylist);
                }
                ImGui::SameLine();
            }

            const char* modeText = "";
            switch (audioState.playMode) {
                case PlayMode::ListLoop: modeText = u8"顺序"; break;
                case PlayMode::RepeatOne: modeText = u8"单曲"; break;
                case PlayMode::Shuffle: modeText = u8"随机"; break;
            }
            if (ImGui::Button(modeText)) {
                int currentMode = (int)audioState.playMode;
                audioState.playMode = (PlayMode)((currentMode + 1) % 3);
                audioState.playHistory.clear();
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(150);
            ImGui::SliderFloat("##Volume", &volume, 0.0f, 1.0f, u8"音量 %.2f");
            ImGui::PopItemWidth();

            ImGui::EndGroup();
        }

        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void ShowPlaylistWindow(AudioState& audioState, std::vector<Song>& mainPlaylist, std::vector<Song>& likedPlaylist, std::vector<std::string>& musicDirs, ActiveView currentView, ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags playlist_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("Playlist", nullptr, playlist_flags);

    if (currentView == ActiveView::Main) {
        ImGui::BeginChild("DirManagement", ImVec2(0, 150), true);
        ImGui::Text(u8"音乐目录");
        ImGui::Separator();
        static char add_dir_buf[1024] = "";
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 60);
        ImGui::InputText("##AddPath", add_dir_buf, sizeof(add_dir_buf));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(u8"添加")) {
            if (strlen(add_dir_buf) > 0) {
                musicDirs.push_back(add_dir_buf);
                SaveConfig(musicDirs);
                std::set<std::string> likedPaths; LoadLikedSongs(likedPaths);
                ScanDirectoryForMusic(add_dir_buf, mainPlaylist, likedPaths);
                strcpy(add_dir_buf, "");
            }
        }
        static int selected_dir_idx = -1;
        ImGui::BeginChild("##DirList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.5f), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (int i = 0; i < musicDirs.size(); ++i) {
            if (ImGui::Selectable(musicDirs[i].c_str(), selected_dir_idx == i)) selected_dir_idx = i;
        }
        ImGui::EndChild();
        if (ImGui::Button(u8"移除选中") && selected_dir_idx != -1) {
            musicDirs.erase(musicDirs.begin() + selected_dir_idx);
            SaveConfig(musicDirs);
            selected_dir_idx = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"重新扫描")) {
            mainPlaylist.clear();
            std::set<std::string> likedPaths; LoadLikedSongs(likedPaths);
            for (const auto& dir : musicDirs) ScanDirectoryForMusic(dir, mainPlaylist, likedPaths);
        }
        ImGui::EndChild();
    }

    std::vector<Song>& activePlaylist = (currentView == ActiveView::Main) ? mainPlaylist : likedPlaylist;
    if (currentView == ActiveView::LikedSongs) {
        likedPlaylist.clear();
        for (const auto& song : mainPlaylist) {
            if (song.isLiked) likedPlaylist.push_back(song);
        }
    }

    ImGui::Text(u8"播放列表");
    if (ImGui::BeginTable("playlist_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 40.0f);
        ImGui::TableSetupColumn(u8"标题");
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(activePlaylist.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", i + 1);
                ImGui::TableSetColumnIndex(1);

                Song& song = activePlaylist[i];
                bool is_selected = (audioState.isAudioReady && audioState.currentFilePath == song.filePath);

                if (ImGui::Selectable(song.displayName.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        PlaySongAtIndex(audioState, activePlaylist, i, PlayDirection::New);
                    }
                }
            }
        }
        clipper.End();
        ImGui::EndTable();
    }
    
    ImGui::End();
    ImGui::PopStyleVar();
}
