#pragma once

#include "types.h"
#include <imgui.h>

// Forward declare global fonts to be used by the UI
struct ImFont;
extern ImFont* G_Font_Default;
extern ImFont* G_Font_Large;

// UI rendering functions
void SetModernDarkStyle();
void ShowLeftSidebar(ImVec2 pos, ImVec2 size, ActiveView& currentView);
void ShowPlaylistWindow(AudioState& audioState, std::vector<Song>& mainPlaylist, std::vector<Song>& likedPlaylist, std::vector<std::string>& musicDirs, ActiveView currentView, ImVec2 pos, ImVec2 size);
void ShowPlayerWindow(AudioState& audioState, std::vector<Song>& mainPlaylist, std::vector<Song>& activePlaylist, float& volume, float progress, ImVec2 pos, ImVec2 size);