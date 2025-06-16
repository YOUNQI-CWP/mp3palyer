/**
 * @file files.h
 * @brief 声明文件系统操作和配置管理相关函数。
 *
 * 该文件定义了用于扫描音乐目录、保存和加载用户喜爱歌曲列表，
 * 以及保存和加载应用程序配置的函数接口。
 */

#pragma once

#include "types.h"
#include <set>

/**
 * @brief 扫描指定目录及其子目录以查找支持的音乐文件。
 *
 * @param path 要扫描的目录路径。
 * @param playlist 歌曲列表的引用，扫描到的歌曲将添加到此处。
 * @param likedPaths 包含喜爱歌曲文件路径的集合，用于标记歌曲是否被喜爱。
 */
void ScanDirectoryForMusic(const std::string& path, std::vector<Song>& playlist, const std::set<std::string>& likedPaths);

/**
 * @brief 将播放列表中所有被标记为"喜爱"的歌曲路径保存到文件中。
 *
 * @param playlist 包含歌曲信息的向量。
 */
void SaveLikedSongs(const std::vector<Song>& playlist);

/**
 * @brief 从文件中加载喜爱歌曲的路径。
 *
 * @param likedPaths 喜爱歌曲路径集合的引用，加载的路径将添加到此处。
 */
void LoadLikedSongs(std::set<std::string>& likedPaths);

/**
 * @brief 将音乐目录配置保存到文件中。
 *
 * @param musicDirs 包含音乐目录路径的向量。
 */
void SaveConfig(const std::vector<std::string>& musicDirs);

/**
 * @brief 从文件中加载音乐目录配置。
 *
 * @param musicDirs 音乐目录路径向量的引用，加载的路径将添加到此处。
 */
void LoadConfig(std::vector<std::string>& musicDirs);