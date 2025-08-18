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
    // calloc �ڷ����ڴ�ռ��ͬʱ�������䵽���ڴ�ռ��ʼ��Ϊȫ 0
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
    // ͨ�� channel �ͷŶ�Ӧ�� TcpConnection ����
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
    // epoll_wait ���յĳ�ʱʱ����������λΪ���룬����Ҫ����Ϊ��λ�����ݳ��� 1000
    int count = epoll_wait(data->epfd, data->events, Max, timeout * 1000);
    //printf("epoll_wait returned %d events\n", count);
    for (int i = 0; i < count; ++i)
    {
        int events = data->events[i].events;
        int fd = data->events[i].data.fd;
        // EPOLLERR���Զ��Ѿ��Ͽ�����
        // EPOLLHUP���Զ��Ѿ��Ͽ����ӣ������˻�����Է���������
        if (events & EPOLLERR || events & EPOLLHUP)
        {
            // �Է��Ѿ��Ͽ����ӣ�ɾ�� fd ����
            // epollRemove(Channel, evLoop);
            continue;
        }
        // ����Ϊ�������¼������¼�/д�¼�
        // ��Ҫ���ö�Ӧ�Ļص�����
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

