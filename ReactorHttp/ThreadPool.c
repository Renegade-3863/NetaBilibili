#include "ThreadPool.h"
#include <assert.h>
#include <stdlib.h>

struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count)
{
    struct ThreadPool* pool = (struct ThreadPool*)malloc(sizeof(struct ThreadPool));
    pool->index = 0;
    pool->isStart = false;
    //printf("Pool->isStart = %d\n", pool->isStart);
    pool->mainLoop = mainLoop;
    pool->threadNum = count;
    pool->workerThreads = (struct WorkerThread*)malloc(sizeof(struct WorkerThread) * count);
    return pool;
}

void threadPoolRun(struct ThreadPool* pool)
{
    assert(pool && !pool->isStart);
    // 执行 threadPoolRun 的函数，正常情况下都是主线程
    if (pool->mainLoop->threadID != pthread_self())
    {
        exit(0);
    }
    pool->isStart = true;
    // 多反应堆模型
    if (pool->threadNum)
    {
        // 对线程池中的工作线程进行初始化
        for (int i = 0; i < pool->threadNum; ++i)
        {
            // 初始化工作线程
            workerThreadInit(&pool->workerThreads[i], pool->index);
            // 运行工作线程
            workerThreadRun(&pool->workerThreads[i]);
        }
    }
    // 否则，单反应堆模型
}

struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool)
{
    assert(pool->isStart);
    // 执行 takeWorkerEventLoop 的函数，正常情况下都是主线程
    if (pool->mainLoop->threadID != pthread_self())
    {
        exit(0);
    }
    // 从线程池中找一个子线程，然后取出其中的反应堆实例
    // 如果是单反应堆模型，那么就用主线程的反应堆模型
    struct EventLoop* evLoop = pool->mainLoop;
    if (pool->threadNum > 0)
    {
        evLoop = pool->workerThreads[pool->index].evLoop;
        pool->index = ++pool->index % pool->threadNum;
    }
    return evLoop;
}
