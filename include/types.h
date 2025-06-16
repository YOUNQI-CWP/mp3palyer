/**
 * @file types.h
 * @brief 定义应用程序中使用的核心数据结构和枚举。
 *
 * 该文件包含了播放模式、活动视图、播放方向等枚举类型，
 * 以及歌曲信息(`Song`)和全局音频状态(`AudioState`)的结构体定义。
 * 这些定义在整个应用程序中共享，以保持数据的一致性。
 */

#pragma once // 防止头文件被重复包含

#include <string>
#include <vector>
#include <SDL.h> // For Uint32 in AudioState
#include "miniaudio.h" // For ma_... types in AudioState

// 应用程序枚举
/**
 * @brief 定义歌曲的播放模式。
 */
enum class PlayMode {
    ListLoop,  ///< 列表循环播放模式。
    RepeatOne, ///< 单曲循环播放模式。
    Shuffle    ///< 随机播放模式。
};

/**
 * @brief 定义应用程序当前活动的UI视图。
 */
enum class ActiveView {
    Main,       ///< 主播放列表视图。
    LikedSongs  ///< 喜爱歌曲播放列表视图。
};

/**
 * @brief 定义歌曲的播放方向，用于历史记录。
 */
enum class PlayDirection {
    New,  ///< 播放一首新歌曲（将当前歌曲添加到历史记录）。
    Back  ///< 返回到播放历史中的上一首歌曲。
};

/**
 * @brief 存储单首歌曲信息的结构体。
 */
struct Song {
    std::string filePath;   ///< 歌曲的完整文件路径。
    std::string displayName;///< 在UI中显示的歌曲名称。
    std::string artist;     ///< 歌曲的艺术家名称。
    bool isLiked = false;   ///< 指示歌曲是否被用户喜爱。
};

/**
 * @brief 存储全局音频状态和Miniaudio设备相关信息的结构体。
 */
struct AudioState {
    ma_decoder decoder;         ///< Miniaudio解码器实例，用于解码音频文件。
    ma_device device;           ///< Miniaudio设备实例，用于音频播放。
    ma_mutex mutex;             ///< 保护音频状态访问的互斥锁。

    bool isAudioReady = false;          ///< 指示音频解码器是否已准备好播放。
    bool isDeviceInitialized = false;   ///< 指示Miniaudio设备是否已成功初始化。
    std::string currentFilePath = "";   ///< 当前播放歌曲的文件路径。
    int currentIndex = -1;              ///< 当前播放歌曲在活跃播放列表中的索引。

    PlayMode playMode = PlayMode::ListLoop; ///< 当前的播放模式（列表循环、单曲循环、随机）。
    
    std::vector<int> playHistory;       ///< 存储播放历史的歌曲索引列表。

    float totalSongDurationSec = 0.0f;  ///< 当前歌曲的总时长（秒）。
    Uint32 songStartTime = 0;           ///< 当前歌曲开始播放时的SDL_GetTicks()时间戳。
    float elapsedTimeAtPause = 0.0f;    ///< 歌曲暂停时已播放的秒数，用于恢复播放。
};