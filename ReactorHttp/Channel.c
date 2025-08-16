#include "Channel.h"
#include <stdlib.h>

struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg)
{
    struct Channel* channel = (struct Channel*)malloc(sizeof(struct Channel));
    channel->arg = arg;
    channel->fd = fd;
    channel->events = events;
    channel->readCallback = readFunc;
    channel->writeCallback = writeFunc;
    channel->destroyCallback = destroyFunc;
    return channel;
}

void writeEventEnable(struct Channel* channel, bool flag)
{
    // printf("Setting write event for fd %d to %s\n", channel->fd, flag ? "enabled" : "disabled");
    if (flag)
    {
        channel->events |= WriteEvent;
    }
    else
    {
        channel->events = channel->events & ~WriteEvent;
    }
    // printf("Write event %s for fd %d\n", flag ? "enabled" : "disabled", channel->fd);
}

bool isWriteEventEnable(struct Channel* channel)
{
    return channel->events & WriteEvent;
}
