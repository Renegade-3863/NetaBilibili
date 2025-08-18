#include "WorkerThread.h"
#include <stdio.h>

int workerThreadInit(struct WorkerThread* thread, int index)
{
    // ��ʼ����ʱ�򣬹����̻߳�û�з�Ӧ��ģ�ͺ��߳� ID
    thread->evLoop = NULL;
    thread->threadID = 0;
    sprintf(thread->name, "SubThread-%d", index);
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&thread->cond, NULL);
    return 0;
}

// ���̵߳Ļص�����
static void* subThreadRunning(void* arg)
{
    struct WorkerThread* thread = (struct WorkerThread*)arg;
    pthread_mutex_lock(&thread->mutex);
    thread->evLoop = eventLoopInitEx(thread->name);
    pthread_mutex_unlock(&thread->mutex);
    pthread_cond_signal(&thread->cond);
    eventLoopRun(thread->evLoop);
    return NULL;
}

void workerThreadRun(struct WorkerThread* thread)
{
    // �������߳�
    pthread_create(&thread->threadID, NULL, subThreadRunning, thread);
    // pthread_create ����������̺߳�subThreadRunning ����һ��ִ�����
    // ���ܻ���֣�
    // evLoop �� subThreadRunning ��δ��ʼ����ɣ����߳̾ͳ�����һ�������ڵ� evLoop ��������������
    // ���������
    // ʹ�����������������߳�����һ�ᣬ��Ҫ�õ�ǰ����ֱ�ӽ���
    pthread_mutex_lock(&thread->mutex);
    while (thread->evLoop == NULL)
    {
        // ��δ��ʼ����ɣ��������߳�
        pthread_cond_wait(&thread->cond, &thread->mutex);
    }
    pthread_mutex_unlock(&thread->mutex);
}
