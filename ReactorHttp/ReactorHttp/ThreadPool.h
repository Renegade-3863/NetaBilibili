#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <stdbool.h>

// �����̳߳�
struct ThreadPool
{
    // ���̵߳ķ�Ӧ��ģ�ͣ����ã����̳߳������̵߳�ʱ�򣬲�������� mainLoop��ת��Ϊ����Ӧ��ģ�ͣ�
    struct EventLoop* mainLoop;
    // ���λ������̳߳��Ƿ�������
    bool isStart;
    // ��¼�̳߳�ӵ�е����߳�����
    int threadNum;
    // ���߳�ָ������
    struct WorkerThread* workerThreads;
    // index ���� Round Robin �ش��̳߳���ȡ���߳�
    int index;
};

// ��ʼ���̳߳�
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count);
// �����̳߳�
void threadPoolRun(struct ThreadPool* pool);
// ȡ���̳߳��е�ĳ�����̵߳ķ�Ӧ��ʵ�����������Ҳֻ�������߳���ִ�У�
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool);