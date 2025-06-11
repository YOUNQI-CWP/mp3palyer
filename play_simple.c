#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

// 定义音乐文件的路径，请确保这个文件存在于你的目录下
const char* MUSIC_PATH = "test.mp3"; 

int main(int argc, char* argv[]) {

    // ----------- 1. 初始化 SDL 和 SDL_mixer -----------
    
    // 初始化SDL的音频子系统
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "无法初始化SDL: %s\n", SDL_GetError());
        return 1;
    }

    // 初始化SDL_mixer
    // 参数: 频率, 格式,声道数, 块大小(缓冲区大小)
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "无法初始化SDL_mixer: %s\n", Mix_GetError());
        SDL_Quit();
        return 1;
    }
    
    printf("SDL 和 SDL_mixer 初始化成功！\n");

    // ----------- 2. 加载音乐文件 -----------

    // Mix_Music 是用于处理大型音乐文件(如MP3, OGG)的结构体
    Mix_Music *music = Mix_LoadMUS(MUSIC_PATH);
    if (!music) {
        fprintf(stderr, "无法加载音乐文件 '%s': %s\n", MUSIC_PATH, Mix_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    printf("音乐 '%s' 加载成功！\n", MUSIC_PATH);

    // ----------- 3. 播放音乐 -----------

    // Mix_PlayMusic 的第二个参数是播放次数
    // -1 表示无限循环, 1 表示播放一次
    if (Mix_PlayMusic(music, 1) == -1) {
        fprintf(stderr, "播放音乐失败: %s\n", Mix_GetError());
        Mix_FreeMusic(music);
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    printf("开始播放音乐...\n");

    // ----------- 4. 等待播放结束 -----------

    // 只要音乐还在播放，就让主程序等待
    // Mix_PlayingMusic() 会在音乐播放时返回1
    while (Mix_PlayingMusic()) {
        // 等待100毫秒，防止CPU空转占用率100%
        SDL_Delay(100); 
    }

    printf("音乐播放结束。\n");

    // ----------- 5. 清理资源 -----------

    Mix_FreeMusic(music); // 释放音乐资源
    Mix_CloseAudio();     // 关闭音频设备
    Mix_Quit();           // 退出SDL_mixer
    SDL_Quit();           // 退出SDL

    printf("资源清理完毕，程序退出。\n");

    return 0;
}