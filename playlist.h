#ifndef PLAYLIST_H
#define PLAYLIST_H

// 歌曲节点结构体
typedef struct SongNode {
    char* file_path;          // 歌曲文件的完整路径
    struct SongNode* next;    // 指向下一首歌的指针
    struct SongNode* prev;    // 指向上-一首歌的指针
} SongNode;

// 播放列表结构体
typedef struct {
    SongNode* head;           // 链表头指针
    SongNode* tail;           // 链表尾指针
    int count;                // 歌曲总数
} Playlist;

// --- 函数声明 ---

// 创建并初始化一个新的空播放列表
Playlist* playlist_create();

// 向播放列表末尾添加一首歌
void playlist_add_song(Playlist* pl, const char* file_path);

// 销毁播放列表，释放所有内存
void playlist_destroy(Playlist* pl);

// 打印播放列表中的所有歌曲 (用于调试)
void playlist_print(const Playlist* pl);

// 从指定目录加载支持的音乐文件 (.mp3, .ogg, .wav, .flac)
void playlist_load_from_directory(Playlist* pl, const char* dir_path);

#endif // PLAYLIST_H