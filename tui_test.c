#include <stdio.h>
#include "playlist.h"
#include "ui.h"
#include <ncurses.h> // 为了 getch()

int main(int argc, char* argv[]) {
    // ---- 加载播放列表 (和上次一样) ----
    const char* target_dir = (argc > 1) ? argv[1] : "./music";
    Playlist* my_playlist = playlist_create();
    playlist_load_from_directory(my_playlist, target_dir);

    // ---- UI部分 ----
    // 1. 初始化UI
    ui_init();

    // 2. 绘制界面
    ui_draw(my_playlist);

    // 3. 等待用户按任意键退出
    // getch() 会阻塞程序，直到有一个按键输入
    mvprintw(getmaxy(stdscr) - 1, 3, "[ 按任意键退出 ]");
    refresh();
    getch(); 

    // 4. 清理UI资源
    ui_cleanup();

    // ---- 清理数据 ----
    playlist_destroy(my_playlist);

    printf("TUI测试程序已正常退出。\n");

    return 0;
}