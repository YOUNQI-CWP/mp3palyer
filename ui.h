#ifndef UI_H
#define UI_H

#include "playlist.h" // UI需要知道Playlist是什么样子的

// 初始化ncurses，设置终端进入UI模式
void ui_init();

// 恢复终端的正常模式
void ui_cleanup();

// 在屏幕上绘制整个播放器界面 (现阶段只画播放列表)
void ui_draw(const Playlist* pl);

#endif // UI_H