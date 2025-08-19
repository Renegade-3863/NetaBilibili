#pragma once
#include "Buffer.h"
#include "TcpConnection.h"


// ����״̬��ö��
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

// ������Ӧͷ�Ľṹ��
struct ResponseHeader
{
    char key[32];
    char value[128];
};

// ����һ������ָ�룬������֯Ҫ�ظ����ͻ��˵����ݿ�
typedef int (*responseBody)(const char* fileName, struct Buffer* sendBuf, int socket);
typedef void (*responseRangeBody)(struct HttpResponse* response, struct Buffer* sendBuf, int socket);


// ���� response �ṹ��
struct HttpResponse
{
    // ״̬�У�״̬�룬״̬����
    enum HttpStatusCode statusCode;
    char statusMsg[128];
    char fileName[128];
    // ��Ӧͷ - ��ֵ��
    struct ResponseHeader* headers;
    // ��Ӧͷ�ĳ���
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
// ���� HttpResponse 
void httpResponseDestroy(struct HttpResponse* response);
// ������Ӧͷ��ֵ��
void httpResponseAddHeader(struct HttpResponse* response, const char* key, const char* value);
// ��֯ Http ��Ӧ����
void httpResponsePrepareMsg(struct TcpConnection* conn, struct HttpResponse* response, struct Buffer* sendBuf, int socket);

// 用于 range request 的响应函数
void sendRangeRequestData(struct HttpResponse* response, struct Buffer* sendBuf, int socket);

