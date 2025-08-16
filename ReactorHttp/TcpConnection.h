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
    bool isWebSocket; // �Ƿ�Ϊ WebSocket ����
};

// ��ʼ�� TcpConnection
struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evLoop);
// �ͷű�Ҫ��Դ
int tcpConnectionDestroy(void* arg);