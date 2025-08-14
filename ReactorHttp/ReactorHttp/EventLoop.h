#pragma once
#include <stdbool.h>
#include <pthread.h>
#include "Dispatcher.h"
#include "ChannelMap.h"

extern struct Dispatcher EpollDistatcher;
extern struct Dispatcher PollDistatcher;
extern struct Dispatcher SelectDistatcher;

// ����ýڵ��е� channel �ķ�ʽ
enum ElemType { ADD, DELETE, MODIFY };

// ����������еĽڵ�
struct ChannelElement
{
    int type;                           // ��δ���ýڵ��е� channel
    struct Channel* channel;
    struct ChannelElement* next;
};

struct Dispatcher;

struct EventLoop
{
    bool isQuit;
    struct Dispatcher* dispatcher;
    void* dispatcherData;
    // �������
    struct ChannelElement* head;
    struct ChannelElement* tail;
    // map
    struct ChannelMap* channelMap;
    // �߳� ID, name, mutex
    pthread_t threadID;
    char threadName[32];
    // mutex ���ڱ��������������У����̵߳�������п��ܻᱻ���̺߳͹����̲߳����޸�
    pthread_mutex_t mutex;
    // �洢����ͨ�ŵ��ļ���������ͨ�� socketpair ��ʼ��
    int socketPair[2];
};

// ��ʼ��
struct EventLoop* eventLoopInit();
struct EventLoop* eventLoopInitEx(const char* threadName);
// ������Ӧ��ģ��
int eventLoopRun(struct EventLoop* evLoop);
// ����������ļ�������
int eventActivate(struct EventLoop* evLoop, int fd, int event);
// ��������������
int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type);
// ������������е�����
int eventLoopProcessTask(struct EventLoop* evLoop);
// ���� dispatcher �еĽڵ�
int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel);
// �ͷ� channel ����
// evLoop �������ڶ�λ ChannelMap ����
int destroyChannel(struct EventLoop* evLoop, struct Channel* channel);