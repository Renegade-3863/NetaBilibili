#pragma once
#include <pthread.h>
#include "EventLoop.h"

// 定义子线程对应的结构体
struct WorkerThread
{
    pthread_t threadID; // ID
    char name[24];
    pthread_mutex_t mutex;  // 互斥锁，用于线程同步
    pthread_cond_t cond;    // 条件变量，用于必要时阻塞线程
    struct EventLoop* evLoop;   // 每个工作线程都有一个自己的反应堆模型
};

// 初始化
int workerThreadInit(struct WorkerThread* thread, int index);
// 启动线程
void workerThreadRun(struct WorkerThread* thread);