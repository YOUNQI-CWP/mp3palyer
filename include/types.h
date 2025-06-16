#pragma once // 防止头文件被重复包含

#include <string>
#include <vector>
#include <SDL.h> // For Uint32 in AudioState
#include "miniaudio.h" // For ma_... types in AudioState

// App enums
enum class PlayMode { ListLoop, RepeatOne, Shuffle };
enum class ActiveView { Main, LikedSongs };
enum class PlayDirection { New, Back };

// Song data structure
struct Song {
    std::string filePath;
    std::string displayName;
    std::string artist;
    std::string album;
    bool isLiked = false;
};

// Global audio state structure
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