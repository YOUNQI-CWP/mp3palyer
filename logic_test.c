#include <stdio.h>
#include "playlist.h"

// main 函数的 argc 和 argv 参数可以接收命令行输入
// argc: 命令行参数的数量 (程序名本身算一个)
// argv: 一个字符串数组，存放着每个参数
int main(int argc, char* argv[]) {
    printf("测试播放列表核心逻辑 (V3: 灵活目录加载)\n\n");

    const char* target_dir;

    // 检查用户是否在命令行提供了路径
    if (argc > 1) {
        // 如果提供了 (例如: ./logic_test /path/to/music)
        // argv[0] 是程序名, argv[1] 是第一个参数
        target_dir = argv[1];
        printf("收到指定目录，将扫描: %s\n", target_dir);
    } else {
        // 如果没有提供路径，使用默认的相对路径 "./music"
        target_dir = "./music";
        printf("未指定目录，将扫描默认的相对目录: %s\n", target_dir);
    }
    printf("\n");

    // 1. 创建播放列表
    Playlist* my_playlist = playlist_create();
    if (!my_playlist) {
        return 1;
    }

    // 2. 从目标目录加载歌曲
    playlist_load_from_directory(my_playlist, target_dir);
    printf("\n");

    // 3. 打印加载后的播放列表
    playlist_print(my_playlist);
    printf("\n");
    
    // 4. 销毁播放列表
    printf("销毁播放列表并释放内存...\n");
    playlist_destroy(my_playlist);
    printf("测试完成。\n");

    return 0;
}