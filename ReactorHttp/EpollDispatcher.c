#include "Dispatcher.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define Max 520

struct EpollData
{
    int epfd;
    struct epoll_event* events;
};

static void* epollInit();
static int epollAdd(struct Channel* channel, struct EventLoop* evLoop);
static int epollRemove(struct Channel* channel, struct EventLoop* evLoop);
static int epollModify(struct Channel* channel, struct EventLoop* evLoop);
static int epollDispatch(struct EventLoop* evLoop, int timeout);
static int epollClear(struct EventLoop* evLoop);

struct Dispatcher EpollDistatcher = {
    epollInit,
    epollAdd,
    epollRemove,
    epollModify,
    epollDispatch,
    epollClear
};

static int epollCtl(struct Channel* channel, struct EventLoop* evLoop, int op)
{
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    struct epoll_event ev;
    ev.data.fd = channel->fd;
    int events = 0;
    if (channel->events & ReadEvent)
    {
        events |= EPOLLIN;
    }
    if (channel->events & WriteEvent)
    {
        events |= EPOLLOUT;
    }
    ev.events = events;
    int ret = epoll_ctl(data->epfd, op, channel->fd, &ev);
    return ret;
}

static void* epollInit()
{
    struct EpollData* data = (struct EpollData*)malloc(sizeof(struct EpollData));
    data->epfd = epoll_create(10);
    if (data->epfd == -1)
    {
        perror("epoll_create");
        exit(0);
    }
    // calloc 在分配内存空间的同时，将分配到的内存空间初始化为全 0
    data->events = (struct epoll_event*)calloc(Max, sizeof(struct epoll_event));

    return data;
}

static int epollAdd(struct Channel* channel, struct EventLoop* evLoop)
{
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_ADD);
    if (ret == -1)
    {
        perror("epoll_ctl_add");
        exit(0);
    }
    return ret;
}

static int epollRemove(struct Channel* channel, struct EventLoop* evLoop)
{
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_DEL);
    if (ret == -1)
    {
        perror("epoll_ctl_del");
        exit(0);
    }
    // 通过 channel 释放对应的 TcpConnection 连接
    channel->destroyCallback(channel->arg);
    return ret;
}

static int epollModify(struct Channel* channel, struct EventLoop* evLoop)
{
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_MOD);
    if (ret == -1)
    {
        perror("epoll_ctl_mod");
        exit(0);
    }
    return ret;
}

static int epollDispatch(struct EventLoop* evLoop, int timeout)
{
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    // epoll_wait 接收的超时时长参数，单位为毫秒，所以要用秒为单位的数据乘以 1000
    int count = epoll_wait(data->epfd, data->events, Max, timeout * 1000);
    //printf("epoll_wait returned %d events\n", count);
    for (int i = 0; i < count; ++i)
    {
        int events = data->events[i].events;
        int fd = data->events[i].data.fd;
        // EPOLLERR：对端已经断开连接
        // EPOLLHUP：对端已经断开连接，但本端还在向对方发送数据
        if (events & EPOLLERR || events & EPOLLHUP)
        {
            // 对方已经断开连接，删除 fd 即可
            // epollRemove(Channel, evLoop);
            continue;
        }
        // 其余为正常的事件：读事件/写事件
        // 需要调用对应的回调函数
        if (events & EPOLLIN)
        {
            eventActivate(evLoop, fd, ReadEvent);
        }
        if (events & EPOLLOUT)
        {
            eventActivate(evLoop, fd, WriteEvent);
        }
    }
    return 0;
}

static int epollClear(struct EventLoop* evLoop)
{
    struct EpollData* data = (struct EpollData*)evLoop->dispatcherData;
    free(data->events);
    close(data->epfd);
    free(data);
    return 0;
}

