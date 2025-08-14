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
    // ִ�� threadPoolRun �ĺ�������������¶������߳�
    if (pool->mainLoop->threadID != pthread_self())
    {
        exit(0);
    }
    pool->isStart = true;
    // �෴Ӧ��ģ��
    if (pool->threadNum)
    {
        // ���̳߳��еĹ����߳̽��г�ʼ��
        for (int i = 0; i < pool->threadNum; ++i)
        {
            // ��ʼ�������߳�
            workerThreadInit(&pool->workerThreads[i], pool->index);
            // ���й����߳�
            workerThreadRun(&pool->workerThreads[i]);
        }
    }
    // ���򣬵���Ӧ��ģ��
}

struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool)
{
    assert(pool->isStart);
    // ִ�� takeWorkerEventLoop �ĺ�������������¶������߳�
    if (pool->mainLoop->threadID != pthread_self())
    {
        exit(0);
    }
    // ���̳߳�����һ�����̣߳�Ȼ��ȡ�����еķ�Ӧ��ʵ��
    // ����ǵ���Ӧ��ģ�ͣ���ô�������̵߳ķ�Ӧ��ģ��
    struct EventLoop* evLoop = pool->mainLoop;
    if (pool->threadNum > 0)
    {
        evLoop = pool->workerThreads[pool->index].evLoop;
        pool->index = ++pool->index % pool->threadNum;
    }
    return evLoop;
}
