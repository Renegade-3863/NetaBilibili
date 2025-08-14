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

// 写数据
void taskWakeUp(struct EventLoop* evLoop)
{
    const char* msg = "我是要成为海贼王的男人!!!";
    write(evLoop->socketPair[0], msg, strlen(msg));
}

// 读数据
int readLocalMessage(void* arg)
{
    struct EventLoop* evLoop = (struct EventLoop*)arg;
    // 用于缓存数据
    char buf[256];
    read(evLoop->socketPair[1], buf, sizeof(buf));
    return 0;
}

struct EventLoop* eventLoopInitEx(const char* threadName)
{
    struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
    evLoop->isQuit = false;
    // 哪个线程执行了这个 eventLoopInitEx 函数，得到的线程 ID 就是对应线程的 ID
    evLoop->threadID = pthread_self();
    pthread_mutex_init(&evLoop->mutex, NULL);
    strcpy(evLoop->threadName, threadName == NULL ? "MainThread" : threadName);
    // 先默认使用效率最优的 EpollDispatcher
    evLoop->dispatcher = &EpollDistatcher;
    evLoop->dispatcherData = evLoop->dispatcher->init();
    // 链表
    evLoop->head = evLoop->tail = NULL;
    // map
    evLoop->channelMap = channelMapInit(128);
    // 线程间信息传输，使用 AF_UNIX 进行本机内的 UNIX 套接字通信即可
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, evLoop->socketPair);
    if (ret == -1)
    {
        perror("socketpair");
        exit(0);
    }
    // 指定规则：evLoop->socketPair[0] 发送数据，evLoop->socketPair[1] 接收数据
    // 也就是说，socketPair[1] 需要放到 select, poll, epoll 的检测集合中
    struct Channel* channel = channelInit(evLoop->socketPair[1], ReadEvent, readLocalMessage, NULL, NULL, evLoop);
    // channel 添加到任务队列中
    eventLoopAddTask(evLoop, channel, ADD);
    return evLoop;
}

int eventLoopRun(struct EventLoop* evLoop)
{
    assert(evLoop != NULL);
    // 取出事件分发和检测模型
    struct Dispatcher* dispatcher = evLoop->dispatcher;
    // 比较线程 ID 是否正常，如果是主线程，不要执行 Run 函数，它只负责建立连接，并往任务队列中添加任务
    if (evLoop->threadID != pthread_self())
    {
        return -1;
    }
    // 循环进行事件处理
    while (!evLoop->isQuit)
    {
        dispatcher->dispatch(evLoop, 2);                // 超时时长 2s
        // 只有子线程会执行到这里，它在处理事件的同时，也需要在任何有必要的时候来处理自己任务队列中的任务
        //printf("Thread %s is processing events...\n", evLoop->threadName);
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
    // 取出 channel
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
    // 加锁，保护共享资源
    pthread_mutex_lock(&evLoop->mutex);
    // 创建新节点
    struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
    node->channel = channel;
    node->type = type;
    node->next = NULL;
    // 链表为空
    if (evLoop->head == NULL)
    {
        evLoop->head = evLoop->tail = node;
    }
    else
    {
        evLoop->tail->next = node;          // 添加
        evLoop->tail = node;                // 后移
    }
    pthread_mutex_unlock(&evLoop->mutex);
    // 处理节点
    /*
     * 细节：
     *  1. 对于链表节点的添加：可能是当前线程也可能是其它线程（主线程）
     *      1). 修改 fd 的事件，当前子线程发起，当前子线程处理
     *      2). 添加新的 fd，添加任务节点的操作是由主线程发起的
     *  2. 不能让主线程处理任务队列，需要由当前的子线程来处理
    */
    if (evLoop->threadID == pthread_self())
    {
        // 当前子线程 -- 直接处理任务队列中的所有任务
        eventLoopProcessTask(evLoop);
    }
    else
    {
        // 主线程  -- 通知子线程处理任务队列中的任务
        // 1. 子线程在工作 2. 子线程被阻塞了：select, poll, epoll 中的 timeout 参数
        // 如何解决情况 2？
        // 往 select，poll，epoll 函数对应的检测集合中额外添加一个独属于服务器的 "激活文件描述符"
        // 当主线程想让子线程来处理任务队列中的结点的时候，就往这个文件描述符中写入数据即可
        // 两种解决方案：
        /*
            1. pipe 函数 （主要用于进程间通信，也可用于线程间通信
            2. socket pair （套接字通信）
        */
        // 主线程默认子线程在阻塞，调用 taskWakeUp 对子线程进行 "唤醒"
        taskWakeUp(evLoop);
    }
    return 0;
}

int eventLoopProcessTask(struct EventLoop* evLoop)
{
    // 加锁，保护任务队列
    pthread_mutex_lock(&evLoop->mutex);
    // 取出头结点
    struct ChannelElement* head = evLoop->head;
    while (head)
    {
        struct Channel* channel = head->channel;
        if (head->type == ADD)
        {
            // 添加
            //printf("Adding channel with fd: %d\n", channel->fd);
            eventLoopAdd(evLoop, channel);
        }
        else if (head->type == DELETE)
        {
            // 删除
            //printf("Removing channel with fd: %d\n", channel->fd);
            eventLoopRemove(evLoop, channel);
        }
        else if (head->type == MODIFY)
        {
            // 修改
            //printf("Modifying channel with fd: %d\n", channel->fd);
            eventLoopModify(evLoop, channel);
        }
        // 删除处理完成的结点
        struct ChannelElement* tmp = head;
        head = head->next;
        free(tmp);
        tmp = NULL;
    }
    // 清空任务队列（是否需要把这一步提前到 while 循环中？）
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
        // channelMap 中没有足够的空间来存储 fd - channel 的键值对
        // 需要扩容
        // 扩容失败的话，直接返回 -1
        if (!makeMapRoom(channelMap, fd, sizeof(struct Channel*)))
        {
            return -1;
        }
    }
    // 找到 fd 对应的数组元素位置，并存储
    if (channelMap->list[fd] == NULL)
    {
        // 如果有位置，就进行存储
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
        // 要删除的文件描述符并没存储在 channelMap 中，也就不在 dispatcher 检测的集合中
        // 我们不用做任何事
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
        // 要删除的文件描述符并没存储在 channelMap 中，也就不在 dispatcher 检测的集合中
        // 我们不用做任何事
        return -1;
    }
    int ret = evLoop->dispatcher->modify(channel, evLoop);
    return ret;
}

int destroyChannel(struct EventLoop* evLoop, struct Channel* channel)
{
    // 删除 channel 和 fd 的对应关系
    // 清空指针
    evLoop->channelMap->list[channel->fd] = NULL;
    // 关闭文件描述符
    close(channel->fd);
    // 释放 channel 内存
    free(channel);
    return 0;
}
