#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <stdbool.h>

// 线程池
struct ThreadPool
{
    // 主线程的事件循环
    struct EventLoop* mainLoop;
    bool isStart;
    // 记录工作线程的数量
    int threadNum;
    // 工作线程数组
    struct WorkerThread* workerThreads;
    // index 用于 Round Robin 轮询获取工作线程
    int index;
};

// 初始化线程池
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count);
// 启动线程池
void threadPoolRun(struct ThreadPool* pool);
// 取出线程池中的某个工作线程的事件循环，实际上也只能取出工作线程的事件循环
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool);