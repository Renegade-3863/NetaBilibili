#pragma once
#include <stdbool.h>
#include <pthread.h>
#include "Dispatcher.h"
#include "ChannelMap.h"

extern struct Dispatcher EpollDistatcher;
extern struct Dispatcher PollDistatcher;
extern struct Dispatcher SelectDistatcher;

// 处理该节点中的 channel 的方式
enum ElemType { ADD, DELETE, MODIFY };

// 定义任务队列的节点
struct ChannelElement
{
    int type;                           // 如何处理该节点中的 channel
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
    // mutex 用于保护上面的任务队列：子线程的任务队列可能会被主线程和工作线程并发修改
    pthread_mutex_t mutex;
    // 存储本地通信的文件描述符，通过 socketpair 初始化
    int socketPair[2];
};

// 初始化
struct EventLoop* eventLoopInit();
struct EventLoop* eventLoopInitEx(const char* threadName);
// 启动反应堆模型
int eventLoopRun(struct EventLoop* evLoop);
// 处理被激活的文件描述符
int eventActivate(struct EventLoop* evLoop, int fd, int event);
// 添加任务到任务队列
int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type);
// 处理任务队列中的任务
int eventLoopProcessTask(struct EventLoop* evLoop);
// 处理 dispatcher 中的节点
int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel);
int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel);
// 释放 channel 对象
// evLoop 参数用于定位 ChannelMap 对象
int destroyChannel(struct EventLoop* evLoop, struct Channel* channel);