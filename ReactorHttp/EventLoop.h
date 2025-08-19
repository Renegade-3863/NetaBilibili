#pragma once
#include <stdbool.h>
#include <pthread.h>
#include "Dispatcher.h"
#include "ChannelMap.h"

extern struct Dispatcher EpollDistatcher;
extern struct Dispatcher PollDistatcher;
extern struct Dispatcher SelectDistatcher;

// 事件类型
enum ElemType { ADD, DELETE, MODIFY };

// 任务队列中的节点
struct ChannelElement
{
    int type;                           // 任务类型
    struct Channel* channel;
    struct ChannelElement* next;
};

struct Dispatcher;

struct EventLoop
{
    bool isQuit;
    struct Dispatcher* dispatcher;
    void* dispatcherData;
    // 任务队列
    struct ChannelElement* head;
    struct ChannelElement* tail;
    // map
    struct ChannelMap* channelMap;
    // 线程 ID, name, mutex
    pthread_t threadID;
    char threadName[32];
    // mutex 互斥锁，用于保护任务队列的线程安全
    pthread_mutex_t mutex;
    // 用于唤醒线程的文件描述符
    int socketPair[2];
};

// 初始化
struct EventLoop* eventLoopInit();
struct EventLoop* eventLoopInitEx(const char* threadName);
// 运行事件循环
int eventLoopRun(struct EventLoop* evLoop);
// 激活事件处理
int eventActivate(struct EventLoop* evLoop, int fd, int event);
// 添加任务到事件循环
int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type);
// 处理事件循环中的任务
int eventLoopProcessTask(struct EventLoop* evLoop);
// 添加 dispatcher 相关的节点
int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel);
// 清空 dispatcher 相关的节点
int eventLoopClear(struct EventLoop* evLoop);
// 销毁 channel 节点
int destroyChannel(struct EventLoop* evLoop, struct Channel* channel);