#pragma once
#include <stdbool.h>
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
enum HttpRequestState
{
    // �ڽ���������
    ParseReqLine,
    // �ڽ�������ͷ
    ParseReqHeaders,
    // ����� Get ���󣬲����н����������״̬
    ParseReqBody,
    ParseReqDone
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
    enum HttpRequestState curState;
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
enum HttpRequestState HttpRequestState(struct HttpRequest* request);
// �������ͷ
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
bool parseHttpRequest(struct TcpConnection* conn, struct HttpRequest* request, struct Buffer* readBuf, struct HttpResponse* response, struct Buffer* sendBuf, int socket);
// ���� Http �� WebSocket ����Э��
bool processHttpRequest(struct TcpConnection* conn, struct HttpRequest* request, struct HttpResponse* response);
// �����ַ���
void decodeMsg(char* to, char* from);
const char* getFileType(const char* name);