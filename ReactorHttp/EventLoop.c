#include "EventLoop.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "WebSocket.h"


struct EventLoop* eventLoopInit()
{

    return eventLoopInitEx(NULL);
}

void taskWakeUp(struct EventLoop* evLoop)
{
    const char* msg = "Task wakeup message!";
    write(evLoop->socketPair[0], msg, strlen(msg));
}

int readLocalMessage(void* arg)
{
    struct EventLoop* evLoop = (struct EventLoop*)arg;
    char buf[256];
    read(evLoop->socketPair[1], buf, sizeof(buf));
    return 0;
}

struct EventLoop* eventLoopInitEx(const char* threadName)
{
    struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
    evLoop->isQuit = false;
    evLoop->threadID = pthread_self();
    pthread_mutex_init(&evLoop->mutex, NULL);
    strcpy(evLoop->threadName, threadName == NULL ? "MainThread" : threadName);
    evLoop->dispatcher = &EpollDistatcher;
    evLoop->dispatcherData = evLoop->dispatcher->init();
    evLoop->head = evLoop->tail = NULL;
    // map
    evLoop->channelMap = channelMapInit(128);
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, evLoop->socketPair);
    if (ret == -1)
    {
        perror("socketpair");
        exit(0);
    }
    struct Channel* channel = channelInit(evLoop->socketPair[1], ReadEvent, readLocalMessage, NULL, NULL, evLoop);
    eventLoopAddTask(evLoop, channel, ADD);
    return evLoop;
}

int eventLoopRun(struct EventLoop* evLoop)
{
    assert(evLoop != NULL);
    struct Dispatcher* dispatcher = evLoop->dispatcher;
    if (evLoop->threadID != pthread_self())
    {
        return -1;
    }
    while (!evLoop->isQuit)
    {
        dispatcher->dispatch(evLoop, 2);                
        eventLoopProcessTask(evLoop);
    }
    return 0;
}

int eventActivate(struct EventLoop* evLoop, int fd, int event)
{
    if (fd < 0 || evLoop == NULL)
    {
        return -1;
    }
    // 获取 channel
    struct Channel* channel = evLoop->channelMap->list[fd];
    assert(channel->fd == fd);
    if (event & ReadEvent && channel->readCallback)
    {
        channel->readCallback(channel->arg);
    }
    if (event & WriteEvent && channel->writeCallback)
    {
        channel->writeCallback(channel->arg);
    }
    return 0;
}

int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type)
{
    // 先获取锁
    pthread_mutex_lock(&evLoop->mutex);
    // 创建新节点
    struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
    node->channel = channel;
    node->type = type;
    node->next = NULL;
    // 添加到任务队列
    if (evLoop->head == NULL)
    {
        evLoop->head = evLoop->tail = node;
    }
    else
    {
        evLoop->tail->next = node;          // 尾插
        evLoop->tail = node;                // 尾指针指向新节点
    }
    pthread_mutex_unlock(&evLoop->mutex);
    // 唤醒线程
    /*
     * 细节：
     *  1. 如果当前线程是主线程，则直接在当前线程中执行任务
     *      1). 修改 fd 事件后，当前线程也需要重新调度
     *      2). 新增的 fd 需要在当前线程的调度中生效
     *  2. 如果当前线程不是主线程，则需要唤醒主线程进行调度
    */
    if (evLoop->threadID == pthread_self())
    {
        // 当前线程 -- 直接在当前线程中执行任务
        eventLoopProcessTask(evLoop);
    }
    else
    {
        // 其他线程  -- 通知主线程进行调度
        // 1. 其他线程在睡眠 2. 其他线程被唤醒时，select, poll, epoll 等的 timeout 机制
        // 唤醒机制 2
        // 在 select、poll、epoll 等待的文件描述符中，有一个被写入的情况
        // 其他线程在其他线程中被唤醒时，会将文件描述符的状态写入到对应的内存中
        // 这样就可以唤醒其他线程
        // 这里面有很多细节
        /*
            1. pipe 机制 需要创建两个匿名管道，也就是需要两个线程之间的通信
            2. socket pair 机制 也需要两个线程之间的通信
        */
        // 其他线程在其他线程中被唤醒时，会将文件描述符的状态写入到对应的内存中
        // 这样就可以唤醒其他线程
        taskWakeUp(evLoop);
    }
    return 0;
}

int eventLoopProcessTask(struct EventLoop* evLoop)
{
    pthread_mutex_lock(&evLoop->mutex);
    struct ChannelElement* head = evLoop->head;
    while (head)
    {
        struct Channel* channel = head->channel;
        if (head->type == ADD)
        {
            // 添加
            eventLoopAdd(evLoop, channel);
        }
        else if (head->type == DELETE)
        {
            // 删除
            eventLoopRemove(evLoop, channel);
        }
        else if (head->type == MODIFY)
        {
            // 修改
            eventLoopModify(evLoop, channel);
        }
        // 释放已处理的节点
        struct ChannelElement* tmp = head;
        head = head->next;
        free(tmp);
        tmp = NULL;
    }
    // 处理完毕后，清空任务队列
    evLoop->head = evLoop->tail = NULL;
    pthread_mutex_unlock(&evLoop->mutex);
    return 0;
}

int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size)
    {
        // channelMap 需要扩展以容纳 fd - channel 的映射
        // 需要重新分配
        // 分配失败的情况下直接返回 -1
        if (!makeMapRoom(channelMap, fd, sizeof(struct Channel*)))
        {
            return -1;
        }
    }
    // 找到 fd 对应的通道
    if (channelMap->list[fd] == NULL)
    {
        // 通道不存在，将通道添加到 dispatcher 中
        channelMap->list[fd] = channel;
        evLoop->dispatcher->add(channel, evLoop);
    }
    return 0;
}

int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size)
    {
        // 要删除的文件描述符在 channelMap 中并不存在
        // 也就是说并没有对应的 dispatcher 处理这个文件描述符
        // 这就意味着肯定是出错了
        return -1;
    }
    int ret = evLoop->dispatcher->remove(channel, evLoop);
    return ret;
}

int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size || channelMap->list[fd] == NULL)
    {
        return -1;
    }
    int ret = evLoop->dispatcher->modify(channel, evLoop);
    return ret;
}

int destroyChannel(struct EventLoop* evLoop, struct Channel* channel)
{
    evLoop->channelMap->list[channel->fd] = NULL;
    close(channel->fd);
    free(channel);
    return 0;
}
