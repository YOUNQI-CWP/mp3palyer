#include "ui.h"
#include "audio.h"
#include "files.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

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
        // --- Left panel: Song Info ---
        ImGui::TableNextColumn();
        {
            float artSize = size.y * 0.8f;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + artSize, p.y + artSize), IM_COL32(40, 40, 40, 255), 4.0f);
            ImGui::Dummy(ImVec2(artSize, artSize));
            
            ImGui::SameLine(0, 10.0f);

            float infoHeight = (G_Font_Large ? G_Font_Large->FontSize : 18.0f) + (G_Font_Default ? G_Font_Default->FontSize : 18.0f) + ImGui::GetStyle().ItemSpacing.y;
            float infoOffsetY = (artSize - infoHeight) / 2.0f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + infoOffsetY);

            ImGui::BeginGroup();
            if (currentSongInActive) {
                if (G_Font_Large) ImGui::PushFont(G_Font_Large);
                ImGui::Text("%s", currentSongInActive->displayName.c_str());
                if (G_Font_Large) ImGui::PopFont();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("%s", currentSongInActive->artist.c_str());
                ImGui::PopStyleColor();
            } else {
                if (G_Font_Large) ImGui::PushFont(G_Font_Large);
                ImGui::Text("No Song Loaded");
                if (G_Font_Large) ImGui::PopFont();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("Unknown Artist");
                ImGui::PopStyleColor();
            }
            ImGui::EndGroup();
        }

        // --- Middle panel: Playback Controls ---
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

        // --- Right panel: Volume and Modes ---
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