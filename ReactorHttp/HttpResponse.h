#pragma once
#include "Buffer.h"
#include "TcpConnection.h"


// 状态码
enum HttpStatusCode
{
    Unknown,
    SwitchingProtocols = 101,
    OK = 200,
    MovedPermanently = 301,
    MovedTemporarily = 302,
    BadRequest = 400,
    NotFound = 404
};

// 响应头
struct ResponseHeader
{
    char key[32];
    char value[128];
};

// 响应体
typedef int (*responseBody)(const char* fileName, struct Buffer* sendBuf, int socket);
typedef void (*responseRangeBody)(struct HttpResponse* response, struct Buffer* sendBuf, int socket);


// 处理 response 结构体
struct HttpResponse
{
    // 状态码
    enum HttpStatusCode statusCode;
    char statusMsg[128];
    char fileName[128];
    // 响应头 - 键值对
    struct ResponseHeader* headers;
    // 响应头数量
    int headerNum;
    // 要发送的文件描述符
    int fileFd;
    responseBody sendDataFunc;
    // 用于 range request
    // 当前的发送偏移
    off_t fileOffset;
    // 剩余要发送的长度（用于范围请求）
    int fileLength;
    // 用于 range request 的响应函数，如果不需要范围请求，则为 NULL
    responseRangeBody sendRangeDataFunc;
    // 标记位，用于区分当前的 sendRangeDataFunc 是用于响应 Range Request 还是用于响应完整数据的请求
    bool isRangeRequest;
};

// 初始化 HttpResponse 结构体
struct HttpResponse* httpResponseInit();
// 销毁 HttpResponse 
void httpResponseDestroy(struct HttpResponse* response);
// 响应头 - 添加键值对
void httpResponseAddHeader(struct HttpResponse* response, const char* key, const char* value);
// 响应头 - 准备响应头
void httpResponsePrepareMsg(struct TcpConnection* conn, struct HttpResponse* response, struct Buffer* sendBuf, int socket);

// 用于 range request 的响应函数
void sendRangeRequestData(struct HttpResponse* response, struct Buffer* sendBuf, int socket);

