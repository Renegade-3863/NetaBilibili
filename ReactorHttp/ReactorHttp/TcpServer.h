#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"

struct Listener
{
    int lfd;
    unsigned short port;
};

struct TcpServer
{
    int threadNum;
    struct EventLoop* mainLoop;
    struct ThreadPool* threadPool;
    struct Listener* listener;
};

// 初始化 TCP 服务器实例，指定服务器要绑定的端口，以及使用的线程池中的线程个数
struct TcpServer* tcpServerInit(unsigned short port, int threadNum);
// 初始化监听文件描述符
struct Listener* listenerInit(unsigned short port);
// 运行 TCP 服务器
void tcpServerRun(struct TcpServer* server);