/**
 * @file audio.h
 * @brief 声明音频播放和控制相关函数。
 *
 * 该文件定义了与Miniaudio库交互的函数，包括音频数据回调、
 * 歌曲播放逻辑和获取下一首歌曲索引的功能。
 * 这些函数在应用程序的音频核心部分使用。
 */

#pragma once

#include "types.h"

// 音频回调函数
/**
 * @brief Miniaudio设备的数据回调函数。
 *
 * 该函数由Miniaudio库调用，用于处理音频流数据。通常在单独的线程中执行。
 *
 * @param pDevice 指向 `ma_device` 结构的指针。
 * @param pOutput 指向输出缓冲区的指针。
 * @param pInput 指向输入缓冲区的指针（通常为NULL，表示不处理输入）。
 * @param frameCount 要处理的PCM帧数。
 */
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

// 音频控制函数
/**
 * @brief 播放播放列表中指定索引的歌曲。
 *
 * 该函数负责初始化或重新配置音频设备以播放选定的歌曲。
 *
 * @param audioState 对 `AudioState` 结构的引用，包含当前的音频状态和设备信息。
 * @param playlist 包含歌曲信息的向量，表示当前的播放列表。
 * @param index 要播放歌曲在 `playlist` 中的索引。
 * @param direction 播放方向，默认为 `PlayDirection::New`，表示新播放。
 * @return 如果歌曲成功开始播放，则返回 `true`；否则返回 `false`。
 */
bool PlaySongAtIndex(AudioState& audioState, std::vector<Song>& playlist, int index, PlayDirection direction = PlayDirection::New);

/**
 * @brief 根据当前播放模式获取下一首歌曲的索引。
 *
 * 根据 `audioState` 中的 `playMode`（如列表循环、单曲循环、随机播放）
 * 计算并返回下一首歌曲在播放列表中的索引。
 *
 * @param audioState 对 `AudioState` 结构的引用，包含当前的播放模式和当前歌曲索引。
 * @param listSize 播放列表的总大小。
 * @return 下一首歌曲的索引。如果播放列表为空，则返回-1。
 */
int GetNextSongIndex(AudioState& audioState, int listSize);