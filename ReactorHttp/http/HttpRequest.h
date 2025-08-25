#pragma once
#include <stdbool.h>
#include <sys/types.h>
#include "Buffer.h"
#include "HttpResponse.h"
#include "TcpConnection.h"

struct RequestHeader
{
    char* key;
    char* value;
};

typedef enum { PARSE_OK = 0, PARSE_INCOMPLETE = 1,  PARSE_ERROR = 2 } ParseResult;
typedef enum { PS_REQLINE, PS_HEADERS, PS_BODY, PS_CHUNK_SIZE, PS_CHUNK_BODY, PS_COMPLETE } ParseState;

// 异步上传上下文：用于在多次读事件中累积接收并落盘
struct UploadCtx
{
    int fd;                    // 目标文件 fd
    ssize_t expected;          // 期望接收的总长度（Content-Length），若为 -1 表示未知（例如 chunked）
    ssize_t received;          // 已接收的字节数
    bool chunked;              // 是否 chunked 传输
    int duration_sec;          // X-Video-Duration 值（可选）
    char filename[256];        // 原始文件名（解码后）
    char path[256];            // 落盘路径，例如 static/uploads/<filename>
};

// 保存 Http 解析的上下文
struct HttpParseCtx
{
    ParseState state;
    ssize_t content_length;         // 如果为 -1，代表 Http 请求的长度未知
    ssize_t body_read;              // 已读的 body 长度
    ssize_t chunk_size;             // 当前待读取的 chunk 大小
    bool chunk_final;               // 当前 chunk 是否为最后一个 chunk
    bool keep_alive;                // 是否为长连接
};

// Http 请求的结构体
struct HttpRequest
{
    // 请求方法，如 GET/POST
    char* method;
    // 请求的统一资源定位符
    char* url;
    // HTTP 版本
    char* version;
    struct RequestHeader* reqHeaders;
    int reqHeadersNum;
    ParseState curState;
};

// 初始化一个 HttpRequest
struct HttpRequest* httpRequestInit();
// 重置 HttpRequest 结构体
void httpRequestReset(struct HttpRequest* req);
// 释放内存
void httpRequestResetEx(struct HttpRequest* req);
// 释放 HttpRequest 结构体
void httpRequestDestroy(struct HttpRequest* req);
// 获取 HttpRequest 当前的解析状态
ParseState HttpRequestState(struct HttpRequest* request);
// 添加一个请求头
// key 是请求头的键，value 是请求头的值
void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value);
// 获取请求头的值
// key 是请求头的键，返回值是请求头的值
char* httpRequestGetHeader(struct HttpRequest* request, const char* key);
// 解析请求行
bool parseHttpRequestLine(struct HttpRequest* request, struct Buffer* readBuf);
// 解析请求头
// 只解析一个请求头，返回值是请求头的值
bool parseHttpRequestHeader(struct HttpRequest* request, struct Buffer* readBuf);
// 解析 Http 请求的 WebSocket 协议
ParseResult parseHttpRequest(struct TcpConnection* conn, struct HttpRequest* request, struct Buffer* readBuf, struct HttpResponse* response, struct Buffer* sendBuf, int socket);
// 处理 Http 请求
bool processHttpRequest(struct TcpConnection* conn, struct HttpRequest* request, struct HttpResponse* response);
// 对 Http 请求的 body 进行解码
void decodeMsg(char* to, char* from);
const char* getFileType(const char* name);