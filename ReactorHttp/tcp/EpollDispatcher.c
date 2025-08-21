#include "Dispatcher.h"
#include "EventLoop.h"
#include "Log.h"
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

    // Mark: 2025/08/17
    // 注册为边缘触发
    ev.events = events | EPOLLET;

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
    int fd = channel ? channel->fd : -1;
    Debug("epollRemove called for fd=%d", fd);
    int ret = epollCtl(channel, evLoop, EPOLL_CTL_DEL);
    if (ret == -1)
    {
        perror("epoll_ctl_del");
        exit(0);
    }
    channel->destroyCallback(channel->arg);
    Debug("epollRemove done for fd=%d", fd);
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
    // epoll_wait 返回的时间单位是毫秒，因此需要将超时时间转换为毫秒
    int count = epoll_wait(data->epfd, data->events, Max, timeout * 1000);
    for (int i = 0; i < count; ++i)
    {
        int events = data->events[i].events;
        int fd = data->events[i].data.fd;
        // EPOLLHUP表示对端已经关闭连接
        if (events & EPOLLERR || events & EPOLLHUP)
        {
            // 连接已经关闭，移除 fd
            // 先做安全检查，避免通过 NULL 指针访问
            if (fd >= 0 && fd < evLoop->channelMap->size && evLoop->channelMap->list[fd])
            {
                epollRemove(evLoop->channelMap->list[fd], evLoop);
            }
            else
            {
                // 如果没有对应的 channel，直接从 epoll 集合中删除该 fd
                epoll_ctl(data->epfd, EPOLL_CTL_DEL, fd, &data->events[i]);
            }
            continue;
        }
        // 处理为读事件/写事件
        // 需要触发相应的回调
        // 安全检查：确保 fd 在 channelMap 范围内并且有 channel
        if (fd < 0 || fd >= evLoop->channelMap->size)
        {
            // 超出范围，尝试从 epoll 中移除
            epoll_ctl(data->epfd, EPOLL_CTL_DEL, fd, &data->events[i]);
            continue;
        }
        if (evLoop->channelMap->list[fd] == NULL)
        {
            // 没有 channel，移除 epoll 中的 fd，避免重复唤醒
            epoll_ctl(data->epfd, EPOLL_CTL_DEL, fd, &data->events[i]);
            continue;
        }
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

