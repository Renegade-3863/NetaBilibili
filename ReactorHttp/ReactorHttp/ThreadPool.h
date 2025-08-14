#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <stdbool.h>

// 定义线程池
struct ThreadPool
{
    // 主线程的反应堆模型：备用，当线程池无子线程的时候，才启用这个 mainLoop（转换为单反应堆模型）
    struct EventLoop* mainLoop;
    // 标记位，标记线程池是否启动了
    bool isStart;
    // 记录线程池拥有的子线程总数
    int threadNum;
    // 子线程指针数组
    struct WorkerThread* workerThreads;
    // index 用于 Round Robin 地从线程池中取用线程
    int index;
};

// 初始化线程池
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count);
// 启动线程池
void threadPoolRun(struct ThreadPool* pool);
// 取出线程池中的某个子线程的反应堆实例（这个函数也只会由主线程来执行）
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool);