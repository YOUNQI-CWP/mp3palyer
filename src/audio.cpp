/**
 * @file audio.cpp
 * @brief 处理音频播放和相关逻辑的实现文件。
 *
 * 该文件包含了音频数据回调函数，用于Miniaudio库处理音频流。
 * 此外，它还提供了获取下一首歌曲索引（包括随机播放和循环播放逻辑）
 * 以及根据索引播放歌曲的功能。
 */

#include "audio.h"
#include <random>
#include <cstdio>

/**
 * @brief Miniaudio设备的数据回调函数。
 *
 * 该函数由Miniaudio库调用，用于从解码器读取PCM帧并将其写入输出缓冲区。
 * 它在访问 `audioState` 时使用互斥锁来确保线程安全。
 *
 * @param pDevice 指向 `ma_device` 结构的指针。
 * @param pOutput 指向输出缓冲区的指针，音频数据将写入此处。
 * @param pInput 指向输入缓冲区的指针（在此应用程序中未使用）。
 * @param frameCount 要处理的PCM帧数。
 */
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

/**
 * @brief 获取一个新的随机索引，确保不与当前索引相同。
 *
 * 在洗牌模式下，用于从播放列表中选择下一首歌曲。
 *
 * @param current 当前歌曲的索引。
 * @param listSize 播放列表的总大小。
 * @return 新的随机索引。如果 `listSize` 小于或等于1，则返回0。
 */
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

/**
 * @brief 根据当前播放模式获取下一首歌曲的索引。
 *
 * 支持单曲重复、随机播放和列表循环模式。
 *
 * @param audioState 对 `AudioState` 结构的引用，包含当前播放模式和索引。
 * @param listSize 播放列表的总大小。
 * @return 下一首歌曲的索引。如果播放列表为空，则返回-1。
 */
int GetNextSongIndex(AudioState& audioState, int listSize) {
    if (listSize == 0) return -1;

    switch (audioState.playMode) {
        case PlayMode::RepeatOne: // 重复单曲模式
            return audioState.currentIndex;
        case PlayMode::Shuffle: // 随机播放模式
            return GetNewRandomIndex(audioState.currentIndex, listSize);
        case PlayMode::ListLoop: // 列表循环模式
        default: // 默认行为：循环播放
            return (audioState.currentIndex + 1) % listSize;
    }
}

/**
 * @brief 播放播放列表中指定索引的歌曲。
 *
 * 该函数负责初始化和启动Miniaudio设备，加载歌曲文件，
 * 并更新音频状态（如总时长、当前文件路径、当前索引）。
 *
 * @param audioState 对 `AudioState` 结构的引用，用于管理音频设备和状态。
 * @param playlist 对歌曲向量的引用，表示当前的播放列表。
 * @param index 要播放歌曲的索引。
 * @param direction 播放方向，指示是新播放还是历史播放。
 * @return 如果成功播放歌曲，则返回 `true`；否则返回 `false`。
 */
bool PlaySongAtIndex(AudioState& audioState, std::vector<Song>& playlist, int index, PlayDirection direction) {
    if (playlist.empty() || index < 0 || index >= playlist.size()) {
        printf("Error: Playlist is empty or index is out of bounds.\n");
        return false;
    }

    // 如果是新播放并且当前有歌曲正在播放，则将当前歌曲添加到播放历史中
    if (direction == PlayDirection::New && audioState.currentIndex != -1) {
        audioState.playHistory.push_back(audioState.currentIndex);
    }

    const char* filePath = playlist[index].filePath.c_str();
    // 如果设备已初始化，则先取消初始化
    if (audioState.isDeviceInitialized) ma_device_uninit(&audioState.device);

    ma_mutex_lock(&audioState.mutex);
    
    // 如果音频已准备好，则先取消初始化解码器
    if (audioState.isAudioReady) ma_decoder_uninit(&audioState.decoder);

    // 初始化解码器加载歌曲文件
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

    // 初始化Miniaudio设备
    if (ma_device_init(NULL, &deviceConfig, &audioState.device) != MA_SUCCESS) {
        printf("Error: Failed to initialize audio device.\n");
        return false;
    }

    audioState.isDeviceInitialized = true;
    // 启动音频设备
    if (ma_device_start(&audioState.device) != MA_SUCCESS) {
        printf("Error: Failed to start audio device.\n");
        return false;
    }

    audioState.elapsedTimeAtPause = 0.0f;
    audioState.songStartTime = SDL_GetTicks();

    printf("Playing: %s\n", playlist[index].displayName.c_str());
    return true;
}