#include "WorkerThread.h"
#include <stdio.h>

int workerThreadInit(struct WorkerThread* thread, int index)
{
    // 初始化的时候，工作线程还没有反应堆模型和线程 ID
    thread->evLoop = NULL;
    thread->threadID = 0;
    sprintf(thread->name, "SubThread-%d", index);
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&thread->cond, NULL);
    return 0;
}

// 子线程的回调函数
static void* subThreadRunning(void* arg)
{
    struct WorkerThread* thread = (struct WorkerThread*)arg;
    pthread_mutex_lock(&thread->mutex);
    thread->evLoop = eventLoopInitEx(thread->name);
    pthread_mutex_unlock(&thread->mutex);
    // 使用条件变量唤醒主线程，让主线程能继续执行
    pthread_cond_signal(&thread->cond);
    eventLoopRun(thread->evLoop);
    return NULL;
}

void workerThreadRun(struct WorkerThread* thread)
{
    // 创建子线程
    pthread_create(&thread->threadID, NULL, subThreadRunning, thread);
    // pthread_create 创建完成子线程后，subThreadRunning 并不一定执行完成
    // 可能会出现：
    // evLoop 在 subThreadRunning 还未初始化完成，主线程就尝试往一个不存在的 evLoop 对象中添加任务
    // 解决方案：
    // 使用条件变量，让主线程阻塞一会，不要让当前函数直接结束
    pthread_mutex_lock(&thread->mutex);
    while (thread->evLoop == NULL)
    {
        // 还未初始化完成，阻塞主线程
        pthread_cond_wait(&thread->cond, &thread->mutex);
    }
    pthread_mutex_unlock(&thread->mutex);
}
