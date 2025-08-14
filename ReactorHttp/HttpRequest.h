#pragma once
#include <stdbool.h>
#include "Buffer.h"
#include "HttpResponse.h"
#include "TcpConnection.h"

// 请求头键值对
struct RequestHeader
{
    char* key;
    char* value;
};

// 当前的解析状态
enum HttpRequestState
{
    // 在解析请求行
    ParseReqLine,
    // 在解析请求头
    ParseReqHeaders,
    // 如果是 Get 请求，不会有解析请求体的状态
    ParseReqBody,
    ParseReqDone
};

// 定义 Http 请求结构体
struct HttpRequest
{
    // 请求方法（GET/POST）
    char* method;
    // 请求的统一资源定位符
    char* url;
    // HTTP 版本号
    char* version;
    struct RequestHeader* reqHeaders;
    int reqHeadersNum;
    enum HttpRequestState curState;
};

// 初始化一个 HttpRequest
struct HttpRequest* httpRequestInit();
// 用于 HttpRequest 结构体的重置
void httpRequestReset(struct HttpRequest* req);
// 销毁内存
void httpRequestResetEx(struct HttpRequest* req);
// 内存释放
void httpRequestDestroy(struct HttpRequest* req);
// 获取 Http 请求的处理状态
enum HttpRequestState HttpRequestState(struct HttpRequest* request);
// 添加请求头
// key 和 value 需要在调用函数前放到堆内存上
void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value);
// 根据 key 得到请求头的 value
char* httpRequestGetHeader(struct HttpRequest* request, const char* key);
// 解析请求行
bool parseHttpRequestLine(struct HttpRequest* request, struct Buffer* readBuf);
// 解析请求头
// 该函数一次调用，只处理请求头中的一行
bool parseHttpRequestHeader(struct HttpRequest* request, struct Buffer* readBuf);
// 解析 Http 或 WebSocket 请求协议
bool parseHttpRequest(struct TcpConnection* conn, struct HttpRequest* request, struct Buffer* readBuf, struct HttpResponse* response, struct Buffer* sendBuf, int socket);
// 处理 Http 或 WebSocket 请求协议
bool processHttpRequest(struct TcpConnection* conn, struct HttpRequest* request, struct HttpResponse* response);
// 解码字符串
void decodeMsg(char* to, char* from);
const char* getFileType(const char* name);