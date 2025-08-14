#pragma once
#include "Channel.h"
#include "EventLoop.h"

struct EventLoop;
struct Dispatcher
{
    // init -- 初始化 epoll, poll 或者 select 需要的数据块
    // 初始化出来的数据块不推荐用全局变量进行保存，实现方法为保存到 EventLoop 结构体的成员中
    void* (*init) ();
    // 添加
    int (*add) (struct Channel* channel, struct EventLoop* evLoop);
    // 删除
    int (*remove) (struct Channel* channel, struct EventLoop* evLoop);
    // 修改
    int (*modify) (struct Channel* channel, struct EventLoop* evLoop);
    // 事件检测
    int (*dispatch) (struct EventLoop* evLoop, int timeout);    // 超时时间单位：s
    // 清除数据 (关闭 fd 或者释放内存)
    int (*clear) (struct EventLoop* evLoop);
};