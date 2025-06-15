#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> // 用于目录操作
#include "playlist.h"

// 创建并初始化一个新的空播放列表
Playlist* playlist_create() {
    // 为Playlist结构体分配内存
    Playlist* pl = (Playlist*)malloc(sizeof(Playlist));
    if (!pl) {
        perror("无法为播放列表分配内存");
        return NULL;
    }
    // 初始化
    pl->head = NULL;
    pl->tail = NULL;
    pl->count = 0;
    return pl;
}

// 向播放列表末尾添加一首歌
void playlist_add_song(Playlist* pl, const char* file_path) {
    if (!pl || !file_path) return;

    // 为新的歌曲节点分配内存
    SongNode* new_node = (SongNode*)malloc(sizeof(SongNode));
    if (!new_node) {
        perror("无法为歌曲节点分配内存");
        return;
    }

    // 复制文件路径字符串
    new_node->file_path = strdup(file_path);
    if (!new_node->file_path) {
        perror("无法为歌曲路径分配内存");
        free(new_node);
        return;
    }
    new_node->next = NULL;

    // 如果列表是空的
    if (pl->head == NULL) {
        new_node->prev = NULL;
        pl->head = new_node;
        pl->tail = new_node;
    } else { // 如果列表不为空，添加到末尾
        pl->tail->next = new_node;
        new_node->prev = pl->tail;
        pl->tail = new_node;
    }
    pl->count++;
}

// 销毁播放列表，释放所有内存
void playlist_destroy(Playlist* pl) {
    if (!pl) return;

    SongNode* current = pl->head;
    SongNode* next_node;

    while (current != NULL) {
        next_node = current->next;
        free(current->file_path); // 释放strdup复制的字符串
        free(current);            // 释放节点本身
        current = next_node;
    }

    free(pl); // 最后释放播放列表结构体
}

// 打印播放列表中的所有歌曲 (用于调试)
void playlist_print(const Playlist* pl) {
    if (!pl || pl->count == 0) {
        printf("播放列表为空。\n");
        return;
    }

    printf("--- 播放列表 (%d首歌) ---\n", pl->count);
    SongNode* current = pl->head;
    int index = 1;
    while (current != NULL) {
        printf("%d: %s\n", index++, current->file_path);
        current = current->next;
    }
    printf("-------------------------\n");
}

// 内部辅助函数：检查文件名是否以支持的后缀结尾
static int is_supported_file(const char* filename) {
    const char* supported_extensions[] = {".mp3", ".ogg", ".wav", ".flac", NULL};
    const char* dot = strrchr(filename, '.'); // 找到最后一个'.'
    if (!dot || dot == filename) {
        return 0; // 没有后缀
    }
    for (int i = 0; supported_extensions[i]; i++) {
        // strcasecmp 忽略大小写比较
        if (strcasecmp(dot, supported_extensions[i]) == 0) {
            return 1; // 是支持的格式
        }
    }
    return 0; // 不支持
}


// 从指定目录加载支持的音乐文件
void playlist_load_from_directory(Playlist* pl, const char* dir_path) {
    DIR *d;
    struct dirent *dir; // dirent 结构体代表目录中的一个条目

    d = opendir(dir_path);
    if (!d) {
        perror("无法打开目录");
        fprintf(stderr, "请检查目录 '%s' 是否存在。\n", dir_path);
        return;
    }
    
    printf("正在扫描目录: %s\n", dir_path);

    // 循环读取目录中的每一个文件/文件夹
    while ((dir = readdir(d)) != NULL) {
        // 如果条目是普通文件 (DT_REG) 并且后缀是我们支持的
        if (dir->d_type == DT_REG && is_supported_file(dir->d_name)) {
            // 构建完整的路径 (例如: /home/user/music/song.mp3)
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dir->d_name);
            
            // 使用我们之前写的函数添加到播放列表
            playlist_add_song(pl, full_path);
        }
    }
    closedir(d); // 操作完成，关闭目录
}
