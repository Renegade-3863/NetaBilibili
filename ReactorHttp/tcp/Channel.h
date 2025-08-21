#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// 函数指针类型，用于处理读写事件的回调函数
typedef int(*handleFunc)(void* arg);

// 文件描述符事件类型
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
    // 事件类型
    int events;
    // 事件处理函数
    handleFunc readCallback;
    handleFunc writeCallback;
    // 销毁回调函数
    handleFunc destroyCallback;
    // 事件处理函数的参数
    void* arg;
};

// 初始化一个 Channel
// 指向的 Channel 包含了文件描述符以及相关的读写事件处理函数的指针
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg);
/*
    为什么要修改 Channel 的记录结构体？
    一方面是为了能够更好地管理一个 Channel 的生命周期，并且能够方便地进行事件的注册和注销
    另一方面是为了能够更好地处理与 Channel 相关的读写事件
*/
// 改变 fd 的写事件 (写 or 不写)
void writeEventEnable(struct Channel* channel, bool flag);
// 判断是否需要写入文件描述符
bool isWriteEventEnable(struct Channel* channel);