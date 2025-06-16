#include "files.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

void ScanDirectoryForMusic(const std::string& path, std::vector<Song>& playlist, const std::set<std::string>& likedPaths) {
    const std::set<std::string> supported_extensions = {".mp3", ".wav", ".flac"};
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c){ return std::tolower(c); });

                if (supported_extensions.count(extension)) {
                    std::string full_path = entry.path().string();
                    auto it = std::find_if(playlist.begin(), playlist.end(), [&](const Song& s){ return s.filePath == full_path; });

                    if (it == playlist.end()) {
                        Song newSong;
                        newSong.filePath = full_path;
                        newSong.isLiked = likedPaths.count(full_path);

                        std::string filename = entry.path().stem().string();
                        std::stringstream ss(filename);
                        std::string segment;
                        std::vector<std::string> segments;
                        while(std::getline(ss, segment, '-')) {
                            size_t first = segment.find_first_not_of(" \t\n\r");
                            if (std::string::npos == first) continue;
                            size_t last = segment.find_last_not_of(" \t\n\r");
                            segments.push_back(segment.substr(first, (last - first + 1)));
                        }

                        if (segments.size() >= 2) {
                            newSong.artist = segments[0];
                            newSong.displayName = segments[1];
                        } else {
                            newSong.displayName = filename;
                            newSong.artist = "未知艺术家";
                        }
                        
                        newSong.album = "未知专辑";
                        playlist.push_back(newSong);
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        printf("Filesystem error: %s\n", e.what());
    }
}

void SaveLikedSongs(const std::vector<Song>& playlist) {
    std::ofstream likedFile("liked_songs.txt");
    if (likedFile.is_open()) {
        for (const auto& song : playlist) {
            if (song.isLiked) {
                likedFile << song.filePath << std::endl;
            }
        }
    }
}

void LoadLikedSongs(std::set<std::string>& likedPaths) {
    std::ifstream likedFile("liked_songs.txt");
    if (likedFile.is_open()) {
        std::string line;
        while (std::getline(likedFile, line)) {
            if (!line.empty()) {
                likedPaths.insert(line);
            }
        }
    }
}

void SaveConfig(const std::vector<std::string>& musicDirs) {
    std::ofstream configFile("config.txt");
    if (configFile.is_open()) {
        for (const auto& dir : musicDirs) {
            configFile << dir << std::endl;
        }
    }
}

void LoadConfig(std::vector<std::string>& musicDirs) {
    std::ifstream configFile("config.txt");
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            if (!line.empty()) musicDirs.push_back(line);
        }
    }
}