#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h> // <--- 添加了这一行，以包含OpenGL函数
#include <iostream>

int main(int argc, char* argv[]) {
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // 在创建窗口前设置OpenGL版本
    // 这里我们请求一个兼容的配置文件，因为我们暂时只需要基础功能
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    
    // 创建窗口
    SDL_Window* window = SDL_CreateWindow(
        "MP3 Player",                  // 窗口标题
        SDL_WINDOWPOS_UNDEFINED,       // 初始x位置
        SDL_WINDOWPOS_UNDEFINED,       // 初始y位置
        800,                           // 宽度
        600,                           // 高度
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN // 使用OpenGL | 显示窗口
    );

    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // 创建OpenGL上下文并设为当前
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        std::cerr << "OpenGL context could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    
    // 主循环标志
    bool quit = false;

    // 事件处理器
    SDL_Event e;

    // 主循环
    while (!quit) {
        // 处理事件队列
        while (SDL_PollEvent(&e) != 0) {
            // 用户请求退出
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }

        // 用一种颜色清空屏幕 (这里是深灰色)
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 更新屏幕
        SDL_GL_SwapWindow(window);
    }

    // 销毁OpenGL上下文、窗口并退出SDL
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}