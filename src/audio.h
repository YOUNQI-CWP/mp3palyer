#pragma once

#include "types.h"

// Audio callback function
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

// Audio control functions
bool PlaySongAtIndex(AudioState& audioState, std::vector<Song>& playlist, int index, PlayDirection direction = PlayDirection::New);
int GetNextSongIndex(AudioState& audioState, int listSize);