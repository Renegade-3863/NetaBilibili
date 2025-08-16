#pragma once
#include <stdbool.h>
#include <sys/types.h>
#include "Buffer.h"
#include "HttpResponse.h"
#include "TcpConnection.h"

// ����ͷ��ֵ��
struct RequestHeader
{
    char* key;
    char* value;
};

// ��ǰ�Ľ���״̬
// enum HttpRequestState
// {
//     // �ڽ���������
//     ParseReqLine,
//     // �ڽ�������ͷ
//     ParseReqHeaders,
//     // ����� Get ���󣬲����н����������״̬
//     ParseReqBody,
//     ParseReqDone
// };

typedef enum { PARSE_OK = 0, PARSE_INCOMPLETE = 1,  PARSE_ERROR = 2 } ParseResult;
typedef enum { PS_REQLINE, PS_HEADERS, PS_BODY, PS_CHUNK_SIZE, PS_CHUNK_BODY, PS_COMPLETE } ParseState;

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

// ���� Http ����ṹ��
struct HttpRequest
{
    // ���󷽷���GET/POST��
    char* method;
    // �����ͳһ��Դ��λ��
    char* url;
    // HTTP �汾��
    char* version;
    struct RequestHeader* reqHeaders;
    int reqHeadersNum;
    ParseState curState;
};

// ��ʼ��һ�� HttpRequest
struct HttpRequest* httpRequestInit();
// ���� HttpRequest �ṹ�������
void httpRequestReset(struct HttpRequest* req);
// �����ڴ�
void httpRequestResetEx(struct HttpRequest* req);
// �ڴ��ͷ�
void httpRequestDestroy(struct HttpRequest* req);
// ��ȡ Http ����Ĵ���״̬
ParseState HttpRequestState(struct HttpRequest* request);
// ��������ͷ
// key �� value ��Ҫ�ڵ��ú���ǰ�ŵ����ڴ���
void httpRequestAddHeader(struct HttpRequest* request, const char* key, const char* value);
// ���� key �õ�����ͷ�� value
char* httpRequestGetHeader(struct HttpRequest* request, const char* key);
// ����������
bool parseHttpRequestLine(struct HttpRequest* request, struct Buffer* readBuf);
// ��������ͷ
// �ú���һ�ε��ã�ֻ��������ͷ�е�һ��
bool parseHttpRequestHeader(struct HttpRequest* request, struct Buffer* readBuf);
// ���� Http �� WebSocket ����Э��
ParseResult parseHttpRequest(struct TcpConnection* conn, struct HttpRequest* request, struct Buffer* readBuf, struct HttpResponse* response, struct Buffer* sendBuf, int socket);
// ���� Http �� WebSocket ����Э��
bool processHttpRequest(struct TcpConnection* conn, struct HttpRequest* request, struct HttpResponse* response);
// �����ַ���
void decodeMsg(char* to, char* from);
const char* getFileType(const char* name);