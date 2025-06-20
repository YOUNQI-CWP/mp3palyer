#include <iostream>
#include <string>
#include <atomic>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "sparkchain.h"
#include "sc_tts.h"

using namespace SparkChain;
using namespace std;

// --- 全局区域 ---
// 异步状态标志，用于在异步转同步时等待结果
static atomic_bool finish(false);

// SDK 初始化参数 (请替换为您自己的)
const char *APPID     = "894244c6";
const char *APIKEY    = "97fc61dbeea347213ec7db4d37e15e46";
const char *APISECRET = "YjgwZWMzM2M2NGI2OWVmZDY0MmNiMjFk";
const char *WORKDIR   = "./";
int logLevel          = 100; // 100 表示关闭日志

// --- 核心组件 ---

/**
 * @brief 回调处理类，用于接收SDK返回的音频数据和错误信息
 * 通过 usrTag 机制，每次回调都能获取对应的文件路径，实现了无状态可重入
 */
class onlineTTSCallbacks : public TTSCallbacks {
    void onResult(TTSResult * result, void * usrTag) override {
        const char* currentAudioPath = static_cast<const char*>(usrTag);
        const char * data = result->data();
        int len           = result->len();
        int status        = result->status();

        FILE* file = fopen(currentAudioPath, "ab"); // 以二进制追加模式写入
        if (file) {
            fwrite(data, 1, len, file);
            fclose(file);
        }

        if (status == 2) { // status=2 表示这是最后的数据块，合成完成
            finish = true; // 通知主线程任务已完成
        }
    }

    void onError(TTSError * error, void * usrTag) override {
        printf("请求出错, 错误码: %d, 错误信息:%s\n", error->code(), error->errMsg().c_str());
        finish = true; // 即使出错也要通知主线程，避免无限等待
    }
};


/**
 * @brief (您想要的函数) 将文本转换为语音并保存到指定文件
 * 这是一个完全封装的函数，调用即可完成单次转换。
 *
 * @param text 要合成的文本 (UTF-8编码)
 * @param outputPath 生成的音频文件保存路径 (例如 "./hello.mp3")
 * @param vcn 发音人，默认为 "讯飞小燕"
 * @return bool true 表示合成流程成功结束, false 表示请求失败或超时
 */
bool Text_to_TTS(const char* text, const char* outputPath, const char* vcn = "xiaoyan") {
    // --- 1. 函数内部准备 ---
    OnlineTTS tts(vcn);           // 创建TTS引擎，可指定发音人
    tts.aue("lame");              // 设置音频编码为mp3
    onlineTTSCallbacks cbs{};     // 创建回调处理器
    tts.registerCallbacks(&cbs);  // 注册回调

    // --- 2. 核心逻辑 ---
    remove(outputPath); // 删除旧文件，确保从零开始
    finish = false;     // 重置等待标志

    printf("开始合成: \"%s\" -> %s\n", text, outputPath);

    // 发起异步请求，并将 outputPath 作为用户自定义数据(usrTag)传入
    int ret = tts.arun(text, (void*)outputPath);
    if (ret != 0) {
        printf("合成请求发送失败, 错误码: %d\n", ret);
        return false;
    }

    // --- 3. 等待结果 ---
    int times = 0;
    while (!finish) {
        sleep(1);
        if (times++ > 20) { // 超时设置为20秒
            printf("合成超时！\n");
            return false;
        }
    }
    printf("合成流程结束。\n");
    return true;
}


// --- SDK 初始化与反初始化 (保持不变) ---
int initSDK() {
    SparkChainConfig *config = SparkChainConfig::builder();
    config->appID(APPID)->apiKey(APIKEY)->apiSecret(APISECRET)->workDir(WORKDIR)->logLevel(logLevel);
    return SparkChain::init(config);
}

void uninitSDK() {
    SparkChain::unInit();
}


// --- 主函数：演示如何调用 ---
int main(int argc, char const *argv[]) {
    if (initSDK() != 0) {
        printf("SDK 初始化失败!\n");
        return -1;
    }

    // 直接调用封装好的函数，实现单次转换
    Text_to_TTS("你好，世界。", "./hello_world.mp3");

    printf("\n"); // 分隔一下

    // 再次调用，使用不同的文本、路径和发音人
    Text_to_TTS("我是讯飞星火认知大模型，很高兴为您服务。", "./spark_intro.mp3", "aisjiuxu");

    uninitSDK(); // 程序退出前，释放SDK资源
    return 0;
}