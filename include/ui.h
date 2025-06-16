/**
 * @file ui.h
 * @brief 声明用户界面渲染和风格设置相关函数。
 *
 * 该文件定义了用于设置ImGui风格、显示左侧边栏、
 * 播放列表窗口和播放器窗口的函数接口。
 * 此外，它还包含了应用程序中使用的全局字体声明。
 */

#pragma once

#include "types.h"
#include <imgui.h>

// UI 相关的全局字体声明
/**
 * @brief 全局默认字体，用于UI渲染。
 */
struct ImFont;
extern ImFont* G_Font_Default;
/**
 * @brief 全局大字体，用于UI渲染。
 */
extern ImFont* G_Font_Large;

// UI 渲染函数
/**
 * @brief 设置应用程序的ImGui风格为现代暗色风格。
 */
void SetModernDarkStyle();

/**
 * @brief 显示应用程序的左侧边栏。
 *
 * @param pos 边栏窗口的屏幕位置。
 * @param size 边栏窗口的大小。
 * @param currentView 当前活动的视图枚举引用。
 */
void ShowLeftSidebar(ImVec2 pos, ImVec2 size, ActiveView& currentView);

/**
 * @brief 显示播放列表窗口。
 *
 * @param audioState 对 `AudioState` 结构的引用。
 * @param mainPlaylist 主播放列表的引用。
 * @param likedPlaylist 喜爱歌曲播放列表的引用。
 * @param musicDirs 音乐目录路径的向量引用。
 * @param currentView 当前活动的视图枚举引用。
 * @param pos 播放列表窗口的屏幕位置。
 * @param size 播放列表窗口的大小。
 */
void ShowPlaylistWindow(AudioState& audioState, std::vector<Song>& mainPlaylist, std::vector<Song>& likedPlaylist, std::vector<std::string>& musicDirs, ActiveView currentView, ImVec2 pos, ImVec2 size);

/**
 * @brief 显示应用程序的播放器窗口。
 *
 * @param audioState 对 `AudioState` 结构的引用。
 * @param mainPlaylist 主播放列表的引用。
 * @param activePlaylist 当前活动的播放列表引用。
 * @param volume 音量的引用。
 * @param progress 播放进度的引用。
 * @param pos 播放器窗口的屏幕位置。
 * @param size 播放器窗口的大小。
 */
void ShowPlayerWindow(AudioState& audioState, std::vector<Song>& mainPlaylist, std::vector<Song>& activePlaylist, float& volume, float progress, ImVec2 pos, ImVec2 size);