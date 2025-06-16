#include "audio.h"
#include <random>
#include <cstdio>

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