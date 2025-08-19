#pragma once
#include <stdint.h>
#include <pthread.h>
#include "EventLoop.h"
#include "Buffer.h"
#include "Channel.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "WebSocket.h"

//#define MSG_SEND_AUTO

// 定义一个最大的挂起节点个数（防止请求过多压垮服务器）
// 单连接仅允许最多 32 个挂起的请求
#define MAX_PENDING_RESPONSES 32

struct PendingResponse
{
    uint64_t seq; // 请求的序列号
    struct Buffer* buf;     // 要发送的响应（头+体）
    struct PendingResponse* next;
};

struct TcpConnection
{
    struct EventLoop* evLoop;
    struct Channel* channel;
    struct Buffer* readBuf;
    struct Buffer* writeBuf;
    char name[32];
    struct HttpRequest* request;
    struct HttpResponse* response;
    bool isWebSocket; // 判断是否为 WebSocket 连接

    /* 处理连接池的部分 */
    uint64_t nextReqSeq; // 下一个请求的序列号（来自当前连接的下一个并发请求要分配这个序列号）
    uint64_t nextWriteSeq; // 下一个允许写出的响应的序列号

    pthread_mutex_t respMutex; // 响应缓冲区的互斥锁，用于保护当前连接的挂起响应队列
    struct PendingResponse* pendingHead; // 挂起响应队列头
    struct PendingResponse* pendingTail; // 挂起响应队列尾

    // 记录当前连接上挂起的响应数量
    int pendingCount;
    // 在外面以宏的形式定义了，用于限制每个连接上最多允许的挂起响应数量
    // int maxPending;
};

// ��ʼ�� TcpConnection
struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evLoop);
// �ͷű�Ҫ��Դ
int tcpConnectionDestroy(void* arg);