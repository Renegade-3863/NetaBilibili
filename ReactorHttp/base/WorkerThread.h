#pragma once
#include <pthread.h>
#include "EventLoop.h"

// 工作线程结构体
struct WorkerThread
{
    pthread_t threadID; // ID
    char name[24];
    pthread_mutex_t mutex;  // 互斥锁，用于保护线程间的同步
    pthread_cond_t cond;    // 条件变量，用于线程间的通信
    struct EventLoop* evLoop;   // 每个工作线程都有一个事件循环
};

// 初始化
int workerThreadInit(struct WorkerThread* thread, int index);
// 启动工作线程
void workerThreadRun(struct WorkerThread* thread);