#include "TcpServer.h"
#include "TcpConnection.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "Log.h"

static int set_nonblocking(int fd)
{
    // 记录标签
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        return -1;
    }
    // 设定标签
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

struct TcpServer *tcpServerInit(unsigned short port, int threadNum)
{
    struct TcpServer *tcp = (struct TcpServer *)malloc(sizeof(struct TcpServer));
    tcp->listener = listenerInit(port);
    // 初始化的事件循环是主线程的，不用调用 eventLoopInitEx 函数
    tcp->mainLoop = eventLoopInit();
    tcp->threadNum = threadNum;
    tcp->threadPool = threadPoolInit(tcp->mainLoop, threadNum);
    // printf("TCP : %d\n", tcp->threadPool->threadNum);
    return tcp;
}

struct Listener *listenerInit(unsigned short port)
{
    struct Listener *listener = (struct Listener *)malloc(sizeof(struct Listener));
    // 1. 设置监听的文件描述符（fd）
    // 使用 Ipv4 的 TCP 套接字，TCP 是流式协议，所以用 SOCK_STREAM（传输层使用的是流式协议）
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket");
        return NULL;
    }
    // 2. 设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        perror("setsockopt");
        return NULL;
    }
    // 3. 绑定端口号
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    // 主机上数据存储顺序为小端序，需要转成网络字节序（大端）
    addr.sin_port = htons(port);
    // 设置 0 地址，表示可以绑定本机所有的 IP 地址
    addr.sin_addr.s_addr = INADDR_ANY;
    // 绑定文件描述符和套接字地址
    ret = bind(lfd, (struct sockaddr *)(&addr), sizeof(addr));
    if (ret == -1)
    {
        perror("bind");
        return NULL;
    }
    // 4. 设置监听
    // printf("Listening on port %d\n", port);
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        perror("listen");
        return NULL;
    }
    // 设定非阻塞
    if (set_nonblocking(lfd) == -1)
    {
        perror("set_nonblocking listener");
        return NULL;
    }
    listener->lfd = lfd;
    listener->port = port;
    // 5. 返回监听的文件描述符
    return listener;
}

// 和客户端建立连接，要调用 accept 函数
static int acceptConnection(void *arg)
{
    struct TcpServer *server = (struct TcpServer *)arg;
    // 第二个参数用来保存与服务器建立连接的客户端的 IP 和端口信息，HTTP 服务器不需要记录这部分信息
    // 第三个参数用于描述套接字的长度，也不需要
#ifdef __linux__
    // mark 2025/08/17
    while (1)
    {
        int cfd = accept4(server->listener->lfd, NULL, NULL, SOCK_NONBLOCK);
        if (cfd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 没有连接请求，返回 -1
                return 0;
            }
            perror("accept4");
            return -1;
        }
        // 从线程池中取出一个线程的反应堆模型实例，来处理这个 cfd
        // printf("Accepted connection on fd: %d\n", cfd);
        struct EventLoop *evLoop = takeWorkerEventLoop(server->threadPool);
        if (evLoop == NULL)
        {
            perror("takeWorkerEventLoop");
            close(cfd);
            return -1;
        }
        // 设置客户端 socket 为非阻塞
        // int cflags = fcntl(cfd, F_GETFL, 0);
        // fcntl(cfd, F_SETFL, cflags | O_NONBLOCK);
        // 将 cfd 放到 TcpConnection 中梳理
        tcpConnectionInit(cfd, evLoop);
    }
#else
    while (1)
    {
        int cfd = accept(server->listener->lfd, NULL, NULL);
        if (cfd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 没有连接请求，返回 -1
                return 0;
            }
            perror("accept");
            return -1;
        }
        set_nonblocking(cfd);
        // 从线程池中取出一个线程的反应堆模型实例，来处理这个 cfd
        // printf("Accepted connection on fd: %d\n", cfd);
        struct EventLoop *evLoop = takeWorkerEventLoop(server->threadPool);
        if (evLoop == NULL)
        {
            perror("takeWorkerEventLoop");
            close(cfd);
            return -1;
        }
        // 设置客户端 socket 为非阻塞
        // int cflags = fcntl(cfd, F_GETFL, 0);
        // fcntl(cfd, F_SETFL, cflags | O_NONBLOCK);
        // 将 cfd 放到 TcpConnection 中梳理
        tcpConnectionInit(cfd, evLoop);
    }
#endif
    // printf("Accepted connection on fd: %d\n", cfd);
    return 0;
}

void tcpServerRun(struct TcpServer *server)
{
    // 启动线程池
    threadPoolRun(server->threadPool);
    // 初始化包含监听的文件描述符的 Channel 实例
    struct Channel *channel = channelInit(server->listener->lfd, ReadEvent, acceptConnection, NULL, NULL, server);
    // printf("Channel initialization successful\n");
    //  添加 "添加监听文件描述符" 的任务到任务队列中
    eventLoopAddTask(server->mainLoop, channel, ADD);
    // printf("Adding Channel Task successful\n");
    //  启动反应堆模型
    // printf("Running EventLoop\n");
    Debug("服务器程序已启动...");
    eventLoopRun(server->mainLoop);
}