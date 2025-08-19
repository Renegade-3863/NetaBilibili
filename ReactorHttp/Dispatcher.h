#pragma once
#include "Channel.h"
#include "EventLoop.h"

struct EventLoop;
struct Dispatcher
{
    // init -- 初始化 epoll、poll 或者 select 需要的数据结构
    // 初始化完毕后需要将其作为参数传入 EventLoop 结构体的成员变量
    void* (*init) ();
    // 添加
    int (*add) (struct Channel* channel, struct EventLoop* evLoop);
    // 删除
    int (*remove) (struct Channel* channel, struct EventLoop* evLoop);
    // 修改
    int (*modify) (struct Channel* channel, struct EventLoop* evLoop);
    // 事件分发
    int (*dispatch) (struct EventLoop* evLoop, int timeout);    // 超时时间，单位为秒
    // 清空
    int (*clear) (struct EventLoop* evLoop);
};