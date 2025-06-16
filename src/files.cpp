/**
 * @file files.cpp
 * @brief 处理文件系统操作和配置管理的实现文件。
 *
 * 该文件包含了扫描指定目录以查找音乐文件、
 * 以及保存和加载用户喜爱歌曲列表和应用程序配置的功能。
 * 支持的音乐文件扩展名包括.mp3、.wav和.flac。
 */

#include "files.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

/**
 * @brief 扫描指定目录及其子目录以查找支持的音乐文件。
 *
 * 遍历给定路径下的所有文件，如果文件是支持的音乐格式（.mp3, .wav, .flac），
 * 则将其添加到播放列表中。同时，根据 `likedPaths` 集合设置歌曲的喜爱状态。
 * 会尝试从文件名中解析艺术家和显示名称（例如 "艺术家 - 歌曲名称.mp3"）。
 *
 * @param path 要扫描的目录路径。
 * @param playlist 歌曲列表的引用，扫描到的歌曲将添加到此处。
 * @param likedPaths 包含喜爱歌曲文件路径的集合，用于标记歌曲是否被喜爱。
 */
void ScanDirectoryForMusic(const std::string& path, std::vector<Song>& playlist, const std::set<std::string>& likedPaths) {
    const std::set<std::string> supported_extensions = {".mp3", ".wav", ".flac"}; // 支持的音乐文件扩展名
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                // 将扩展名转换为小写以进行不区分大小写的比较
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c){ return std::tolower(c); });

                if (supported_extensions.count(extension)) {
                    std::string full_path = entry.path().string();
                    // 检查歌曲是否已存在于播放列表中
                    auto it = std::find_if(playlist.begin(), playlist.end(), [&](const Song& s){ return s.filePath == full_path; });

                    if (it == playlist.end()) {
                        Song newSong;
                        newSong.filePath = full_path;
                        newSong.isLiked = likedPaths.count(full_path); // 根据喜爱路径设置喜爱状态

                        std::string filename = entry.path().stem().string();
                        std::stringstream ss(filename);
                        std::string segment;
                        std::vector<std::string> segments;
                        // 尝试从文件名中解析艺术家和歌曲名（例如 "艺术家 - 歌曲名"）
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
                        playlist.push_back(newSong);
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        printf("Filesystem error: %s\n", e.what());
    }
}

/**
 * @brief 将播放列表中所有被标记为"喜爱"的歌曲路径保存到文件中。
 *
 * 歌曲路径将被保存到 `liked_songs.txt` 文件中，每行一个路径。
 *
 * @param playlist 包含歌曲信息的向量。
 */
void SaveLikedSongs(const std::vector<Song>& playlist) {
    std::ofstream likedFile("liked_songs.txt");
    if (likedFile.is_open()) {
        for (const auto& song : playlist) {
            if (song.isLiked) {
                likedFile << song.filePath << std::endl;
            }
        }
        likedFile.close(); // 关闭文件
    }
}

/**
 * @brief 从文件中加载喜爱歌曲的路径。
 *
 * 从 `liked_songs.txt` 文件中读取歌曲路径，并将其添加到 `likedPaths` 集合中。
 * 每行被视为一个歌曲路径。
 *
 * @param likedPaths 喜爱歌曲路径集合的引用，加载的路径将添加到此处。
 */
void LoadLikedSongs(std::set<std::string>& likedPaths) {
    std::ifstream likedFile("liked_songs.txt");
    if (likedFile.is_open()) {
        std::string line;
        while (std::getline(likedFile, line)) {
            if (!line.empty()) {
                likedPaths.insert(line);
            }
        }
        likedFile.close(); // 关闭文件
    }
}

/**
 * @brief 将音乐目录配置保存到文件中。
 *
 * 音乐目录路径将被保存到 `config.txt` 文件中，每行一个路径。
 *
 * @param musicDirs 包含音乐目录路径的向量。
 */
void SaveConfig(const std::vector<std::string>& musicDirs) {
    std::ofstream configFile("config.txt");
    if (configFile.is_open()) {
        for (const auto& dir : musicDirs) {
            configFile << dir << std::endl;
        }
        configFile.close(); // 关闭文件
    }
}

/**
 * @brief 从文件中加载音乐目录配置。
 *
 * 从 `config.txt` 文件中读取音乐目录路径，并将其添加到 `musicDirs` 向量中。
 * 每行被视为一个目录路径。
 *
 * @param musicDirs 音乐目录路径向量的引用，加载的路径将添加到此处。
 */
void LoadConfig(std::vector<std::string>& musicDirs) {
    std::ifstream configFile("config.txt");
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            if (!line.empty()) musicDirs.push_back(line);
        }
        configFile.close(); // 关闭文件
    }
}