#pragma once

#include "types.h"
#include <set>

void ScanDirectoryForMusic(const std::string& path, std::vector<Song>& playlist, const std::set<std::string>& likedPaths);
void SaveLikedSongs(const std::vector<Song>& playlist);
void LoadLikedSongs(std::set<std::string>& likedPaths);
void SaveConfig(const std::vector<std::string>& musicDirs);
void LoadConfig(std::vector<std::string>& musicDirs);