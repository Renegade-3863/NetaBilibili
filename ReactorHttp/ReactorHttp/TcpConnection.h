#pragma once
#include "EventLoop.h"
#include "Buffer.h"
#include "Channel.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "WebSocket.h"

//#define MSG_SEND_AUTO

struct TcpConnection
{
    struct EventLoop* evLoop;
    struct Channel* channel;
    struct Buffer* readBuf;
    struct Buffer* writeBuf;
    char name[32];
    struct HttpRequest* request;
    struct HttpResponse* response;
    bool isWebSocket; // 是否为 WebSocket 连接
};

// 初始化 TcpConnection
struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evLoop);
// 释放必要资源
int tcpConnectionDestroy(void* arg);