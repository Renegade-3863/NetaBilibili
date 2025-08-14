#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// 定义函数指针，这个函数是读/写回调函数的参数模板
typedef int(*handleFunc)(void* arg);

// 定义文件描述符的读写事件
enum FDEvent
{
    Timeout = 0x01,
    ReadEvent = 0x02,
    WriteEvent = 0x04
};

struct Channel
{
    // 文件描述符
    int fd;
    // 事件
    int events;
    // 回调函数
    handleFunc readCallback;
    handleFunc writeCallback;
    // 在销毁 Channel 时调用
    handleFunc destroyCallback;
    // 回调函数的参数
    void* arg;
};

// 初始化一个 Channel
// 指定这个 Channel 封装的文件描述符 fd，以及对应文件描述符上的读/写回调函数以及参数
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg);
/*
    为什么不定义修改读事件的函数？
    一般来说，服务器端并不能禁用对文件描述符的读事件，否则就没法监听来自客户端的数据请求了
*/
// 修改 fd 的写事件 (检测 or 不检测)
void writeEventEnable(struct Channel* channel, bool flag);
// 判断是否需要检测文件描述符的写事件
bool isWriteEventEnable(struct Channel* channel);