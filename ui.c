#include <ncurses.h> // ncurses库的主头文件
#include "ui.h"

// 初始化ncurses，设置终端进入UI模式
void ui_init() {
    initscr();          // 初始化ncurses，进入curses模式
    noecho();           // 关闭按键回显 (例如，按'a'，屏幕上不会出现'a')
    cbreak();           // 立即捕获按键，不需要用户按回车
    keypad(stdscr, TRUE); // 启用功能键，如F1、箭头等
    curs_set(0);        // 隐藏光标
}

// 恢复终端的正常模式
void ui_cleanup() {
    endwin();           // 退出curses模式，恢复终端
}

// 在屏幕上绘制整个播放器界面
void ui_draw(const Playlist* pl) {
    // 1. 清理屏幕
    clear();

    // 2. 获取屏幕尺寸
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // 3. 绘制边框和标题
    box(stdscr, 0, 0);
    mvprintw(0, 3, "[ C语言音乐播放器 ]");

    // 4. 绘制播放列表
    mvprintw(2, 2, "播放列表:");
    
    SongNode* current = pl->head;
    int line = 4; // 从第4行开始画
    int index = 1;
    // 遍历链表，打印每一首歌的路径
    while (current != NULL && line < max_y - 1) { // 确保不画出屏幕
        // mvprintw 会先移动光标到(y,x)再打印
        mvprintw(line, 3, "%d. %s", index, current->file_path);
        current = current->next;
        line++;
        index++;
    }

    // 5. 将所有绘制内容刷新到物理屏幕上
    refresh();
}