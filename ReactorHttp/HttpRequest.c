#pragma once
#define _GNU_SOURCE
#include "HttpRequest.h"
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#define HeaderSize 20
#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

struct HttpRequest *httpRequestInit()
{
    struct HttpRequest *request = (struct HttpRequest *)malloc(sizeof(struct HttpRequest));
    httpRequestReset(request);
    request->reqHeaders = (struct RequestHeader *)malloc(sizeof(struct RequestHeader) * HeaderSize);
    return request;
}

void httpRequestReset(struct HttpRequest *req)
{
    req->curState = ParseReqLine;
    req->method = NULL;
    req->url = NULL;
    req->version = NULL;
    req->reqHeadersNum = 0;
}

void httpRequestResetEx(struct HttpRequest *req)
{
    free(req->url);
    free(req->method);
    free(req->version);
    if (req->reqHeaders)
    {
        for (int i = 0; i < req->reqHeadersNum; ++i)
        {
            free(req->reqHeaders[i].key);
            free(req->reqHeaders[i].value);
        }
        free(req->reqHeaders);
    }
    httpRequestReset(req);
}

void httpRequestDestroy(struct HttpRequest *req)
{
    if (req)
    {
        httpRequestResetEx(req);
        free(req);
    }
}

enum HttpRequestState HttpRequestState(struct HttpRequest *request)
{
    return request->curState;
}

void httpRequestAddHeader(struct HttpRequest *request, const char *key, const char *value)
{
    request->reqHeaders[request->reqHeadersNum].key = (char *)key;
    request->reqHeaders[request->reqHeadersNum].value = (char *)value;
    request->reqHeadersNum++;
}

char *httpRequestGetHeader(struct HttpRequest *request, const char *key)
{
    if (request)
    {
        for (int i = 0; i < request->reqHeadersNum; ++i)
        {
            if (strncasecmp(request->reqHeaders[i].key, key, strlen(key)) == 0)
            {
                return request->reqHeaders[i].value;
            }
        }
    }
    return NULL;
}

static char *splitRequestLine(char *start, char *end, const char *sub, char **ptr)
{
    char *space = end;
    if (sub != NULL)
    {
        space = memmem(start, end - start, sub, strlen(sub));
        assert(space);
    }
    int length = space - start;
    char *tmp = (char *)malloc(length + 1);
    strncpy(tmp, start, length);
    tmp[length] = '\0';
    *ptr = tmp;
    return space + 1;
}

bool parseHttpRequestLine(struct HttpRequest *request, struct Buffer *readBuf)
{
    // ���������У������ַ���������ַ
    char *end = bufferFindCRLF(readBuf); // \r\n
    // �����ַ�����ʼ��ַ
    char *start = readBuf->data + readBuf->readPos;
    // �������ܳ���
    int lineSize = end - start;

    if (lineSize)
    {
        start = splitRequestLine(start, end, " ", &request->method);
        printf("Method: %s\n", request->method);
        start = splitRequestLine(start, end, " ", &request->url);
        printf("URL: %s\n", request->url);
        splitRequestLine(start, end, NULL, &request->version);
        printf("Version: %s\n", request->version);
#if 0
        // get /xxx/xx.txt http/1.1
        // ���󷽷�
        char* space = memmem(start, lineSize, " ", 1);
        assert(space);
        int methodSize = space - start;
        request->method = (char*)malloc(methodSize+1);
        strncpy(request->method, start, methodSize);
        request->method[methodSize] = '\0';

        // ����� URL
        start = space + 1;
        space = memmem(start, end - start, " ", 1);
        assert(space);
        int urlSize = space - start;
        request->url = (char*)malloc(urlSize + 1);
        strncpy(request->url, start, urlSize);
        request->method[urlSize] = '\0';

        // ����� Http Э��汾
        start = space + 1;
        //space = memmem(start, end - start, " ", 1);
        //assert(space);
        //int urlSize = space - start;
        request->version = (char*)malloc(end - start + 1);
        strncpy(request->url, start, end-start);
        request->method[end-start] = '\0';
#endif

        // Ϊ��������ͷ��׼��
        readBuf->readPos += lineSize;
        readBuf->readPos += 2;
        // �޸Ľ���״̬
        request->curState = ParseReqHeaders;
        // printf("Request Line: Method: %s, URL: %s, Version: %s\n", request->method, request->url, request->version);
        return true;
    }
    return false;
}

bool parseHttpRequestHeader(struct HttpRequest *request, struct Buffer *readBuf)
{
    char *end = bufferFindCRLF(readBuf);
    if (end)
    {
        char *start = readBuf->data + readBuf->readPos;
        int lineSize = end - start;
        // ������Ǳ�׼�� Http ���󣬲�Ҫ��ð�ź���Ŀո�
        char *middle = memmem(start, lineSize, ": ", 2);
        if (middle)
        {
            char *key = malloc(middle - start + 1);
            strncpy(key, start, middle - start);
            key[middle - start] = '\0';

            char *value = malloc(end - middle - 1);
            strncpy(value, middle + 2, end - middle - 2);
            value[end - middle - 2] = '\0';

            httpRequestAddHeader(request, key, value);
            // �ƶ������ݵ�λ��
            readBuf->readPos += lineSize;
            readBuf->readPos += 2;
        }
        else
        {
            // ����ͷ���������ˣ���������
            readBuf->readPos += 2;
            // �޸Ľ���״̬
            // ��ʱ���� Post ������ֻ���� Get ����
            request->curState = ParseReqDone;
        }
        return true;
    }
    return false;
}

bool parseHttpRequest(struct TcpConnection *conn, struct HttpRequest *request, struct Buffer *readBuf, struct HttpResponse *response, struct Buffer *sendBuf, int socket)
{
    bool flag = true;
    while (request->curState != ParseReqDone)
    {
        switch (request->curState)
        {
        case ParseReqLine:
            flag = parseHttpRequestLine(request, readBuf);
            break;
        case ParseReqHeaders:
            flag = parseHttpRequestHeader(request, readBuf);
            break;
        case ParseReqBody:
            break;
        default:
            break;
        }
        if (!flag)
        {
            return flag;
        }
        // �ж��Ƿ��������ˣ��������ˣ���Ҫ׼���ظ�������
        if (request->curState == ParseReqDone)
        {
            // 1. ���ݽ�������ԭʼ���ݣ��Կͻ��˵�������������
            processHttpRequest(conn, request, response);
            // 2. ��֯��Ӧ���ݲ����͸��ͻ���
            httpResponsePrepareMsg(conn, response, sendBuf, socket);
        }
    }
    request->curState = ParseReqLine; // ״̬��ԭ����֤�ܼ��������ڶ������Ժ������
    return flag;
}

/*
    ����һ�� static �Ľ��뺯�������ں����� Linux ���ļ�ϵͳ�д����ʱ��ᱻǿ������ת���� UTF-8 �����ʽ
    ����������Ҫ�ڷ������˽��յ��ļ������������ֶ�ת��
*/

/**
 * @param c 16 ���Ƶĵ����ַ�
 */
// �� 16 ���Ƶ� UTF-8 �ַ�ת���� 10 ���Ƶ����ֵĺ���
static int hexToDec(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

/**
 * @param filename 要部分发送的文件名
 * @param sendBuf 发送缓冲
 * @param cfd 发送的文件描述符
 * @param offset 要发送的文件偏移量
 * @param length 要发送的文件长度
 * 
*/
static void sendFilePartial(const char* filename, struct Buffer* sendBuf, 
                    int cfd, int offset, int length)
{
    int fd = open(filename, O_RDONLY);
    assert(fd > 0);

    off_t off = offset;
    int remaining = length;

    while(remaining > 0)
    {
        // 一次最多发送 65536 字节
        size_t toSend = remaining > 65536 ? 65536 : remaining;
        ssize_t sent = sendfile(cfd, fd, &off, toSend);
        if(sent <= 0)
        {
            if(errno == EINTR)
            {
                // 被信号中断，重试
                continue;
            }
            else if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 非阻塞模式下没有数据可发送，退出循环，等待下一次写事件
                break;
            }
            else
            {
                // 其他错误，打印错误信息，关闭文件描述符，返回

                perror("sendfile");
                break;
            }
        }
        remaining -= sent;
    }
}
/**
 * @param to �������ַ���
 * @param from ����󡢽���ǰ���ַ���
 */
// ���뺯��
void decodeMsg(char *to, char *from)
{
    // ����Ҫת�����ַ�����ֱ������ from �ַ����Ľ�β
    for (; *from != '\0'; ++to, ++from)
    {
        // isxdigit ���������жϴ�����ַ��ǲ��� 16 ���Ƹ�ʽ������ȡֵΪ 0~9 a~f A~F
        // �� UTF-8 ������ļ���������
        // Linux%E5%86%85%E6%A0%B8.jpg
        // ���� % ����������ַ��� 16 ���Ƶı���
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // ������������������˵�� from ��ǰ�����ַ�������һ�� UTF-8 ������ַ�
            // ������Ҫ�������ת��
            *to = hexToDec(from[1]) * 16 + hexToDec(from[2]);

            // ת����ɺ�to ָ���ָ���� from ԭ��ָ��� UTF-8 ����ʵ�ʶ�Ӧ���ַ�ֵ��10 ���ƣ�
            // Ϊ��ͳһ���� for ѭ���������� ++from ���߼�����������ֻ�� from ���ƶ� 2 ���ֽ�
            from += 2;
        }
        else
        {
            // ����˵�� from ��ǰ�����ַ����������� ASCII �ַ�
            // ����ֱ�ӿ�������
            *to = *from;
        }
    }
    // �ض��ַ�������ֹ��ȡ�����������
    *to = '\0';
}

const char *getFileType(const char *name)
{
    // a.jpg a.mp4 a.html
    // ����������� '.' �ַ����粻���ڷ��� NULL
    // �������󣬷�ֹ�ļ��������Ͱ������
    const char *dot = strrchr(name, '.');
    printf("File Extension: %s\n", dot);
    if (dot == NULL)
        return "text/plain; charset=utf-8"; // ���ı�
    if (strcmp(dot, ".pdf") == 0)
        return "application/pdf";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";
    if (strcmp(dot, ".mp4") == 0)
        return "video/mp4";
    if (strcmp(dot, ".json") == 0)
        return "application/json";

    return "text/plain; charset=utf-8";
}

void sendDir(const char *dirName, struct Buffer *sendBuf, int cfd)
{
    // printf("Sending directory: %s\n", dirName);
    char buf[4096] = {0};
    // ƴ�� html �ļ��� head �ֶ�
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);
    struct dirent **namelist;
    // ����Ҫ����ɸѡ���򣬵���������ָ��Ϊ NULL
    // ɨ��Ŀ¼��˳����ѭ Linux �Զ���� alphasort ��ʽ�����ļ�����ĸ˳���ģʽ
    int num = scandir(dirName, &namelist, NULL, alphasort);
    for (int i = 0; i < num; ++i)
    {
        // ȡ��һ���ļ���
        // ���� namelist ָ�����һ��ָ������ struct dirent *tmp
        char *name = namelist[i]->d_name;
        // �ж�ȡ������ name ��������һ����С����Ŀ¼������һ���������ļ�
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath, "%s/%s", dirName, name);
        stat(subPath, &st);
        // printf("\nSubpath is: %s", subPath);
        if (S_ISDIR(st.st_mode))
        {
            // �������ӣ�ʹ�� <a></a> ��ǩ
            // �﷨Ϊ <a href="">name</a>
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                    name, name, st.st_size);
        }
        else
        {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    name, name, st.st_size);
        }
        // send(cfd, buf, strlen(buf), 0);
        bufferAppendString(sendBuf, buf);
#ifndef MSG_SEND_AUTO
        // дһ���֣��ͷ�һ����
        bufferSendData(sendBuf, cfd);
#endif
        // ��շ��ͻ���
        memset(buf, 0, sizeof(buf));
        // �ڴ�����
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    bufferAppendString(sendBuf, buf);
#ifndef MSG_SEND_AUTO
    // дһ���֣��ͷ�һ����
    bufferSendData(sendBuf, cfd);
#endif
    // ��������ָ��
    free(namelist);
}

void sendFile(const char *filename, struct Buffer *sendBuf, int cfd)
{
    // 1. ��Ҫ������ļ�
    // ��ֻ����ʽ�򿪣���ֹ�ļ������޸�
    // printf("filename: %s\n", filename);
    int fd = open(filename, O_RDONLY);
    // ʹ�ö����ж��ļ��Ƿ�򿪳ɹ�
    assert(fd > 0);
    // �����һ����Ƶ�ļ������� sendfile ������������
    int totalSize = 0;
#if 1
    // ʹ��һ�� while ѭ�������ϵش��ļ��ж�ȡһ�������ݲ��� TCP ���ӽ��з���
    while (1)
    {
        char buf[1024000];
        int len = read(fd, buf, sizeof(buf));
        if (len > 0)
        {
            // send(cfd, buf, len, 0);
            bufferAppendData(sendBuf, buf, len);
            // printf("Succefully sent file %s\n", filename);
#ifndef MSG_SEND_AUTO
            // дһ���֣��ͷ�һ����
            bufferSendData(sendBuf, cfd);
            // printf("Sending %d bytes data to socket %d\n", len, cfd);
            // printf("Total size sent: %d bytes\n", totalSize);

            totalSize += len;
#endif
            // ��ֹ���Ͷ˷������ݵ�Ƶ�ʹ��죬�����˽��ն˵Ļ���
            // ���������÷����߳�ÿ�η��ͱ���һЩ���
            // timespec �ṹ�����ں��뼶���ʱ�����
            // struct timespec ts = {
            //    .tv_sec = 0,
            //    .tv_nsec = 10 * 1000000L
            //};
            // nanosleep(&ts, NULL);
        }
        else if (len == 0)
        {
            // ��ȡ�Ѿ���ɣ������˳�ѭ��
            break;
        }
        else
        {
            close(fd);
            perror("read");
        }
    }
#else
    off_t offset = 0;
    int size = lseek(fd, 0, SEEK_END);
    // ����һ�д����Ѷ�Ӧ fd �ļ��Ķ�ȡָ���Ƶ��ļ�ĩβ��������Ҫ���ļ�ָ�����ƻ���
    lseek(fd, 0, SEEK_SET);
    // ���ļ�û�з������ʱ���ظ����� sendfile �����������ݷ���
    while (offset < size)
    {
        // ע�⶯̬����Ҫ���͵����ݿ�Ĵ�С�����ݺ����Լ����µ� offset ֵ�� size �����޸�
        int ret = sendfile(cfd, fd, &offset, size - offset);
        // if (ret == -1 && errno == EAGAIN)
        // {
        //     printf("û����...\n");
        // }
        if (ret == -1 && errno != EAGAIN)
        {
            // ���Ƕ�ȡ�������򻺴���д����������֮��Ĳ�������
            // �Ǿ��ǳ����ˣ����Ǵ�ʱ������������
            return -1;
        }
    }
#endif
    close(fd);
}

// �ж�һ�� Http �����Ƿ�Ϊ WebSocket ��������
static bool isWebSocketHandshake(struct HttpRequest *request)
{
    // 1. ����Ƿ�Ϊ WebSocket ��������
    const char *upgrade = httpRequestGetHeader(request, "Upgrade");
    const char *connection = httpRequestGetHeader(request, "Connection");
    const char *ws_key = httpRequestGetHeader(request, "Sec-WebSocket-Key");
    const char *ws_version = httpRequestGetHeader(request, "Sec-WebSocket-Version");

    if (upgrade && strcasecmp(upgrade, "websocket") == 0 &&
        connection && strcasecmp(connection, "Upgrade") == 0 &&
        ws_key && ws_version && strcmp(ws_version, "13") == 0)
    {
        // ˵����һ�� WebSocket ��������
        // ���� true ����
        return true;
    }
    // ����˵������ WebSocket ��������
    return false;
}

static int base64Encode(const unsigned char *input, int inputLen, char *output)
{
    BIO *bmem = NULL, *b64 = NULL;
    BUF_MEM *bptr = NULL;

    // ����һ���ڴ� BIO ����
    b64 = BIO_new(BIO_f_base64());
    // ������
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);

    BIO_write(b64, input, inputLen);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    memcpy(output, bptr->data, bptr->length);
    output[bptr->length] = '\0';

    BIO_free_all(b64);
    return bptr->length;
}

// ���� WebSocket ��������
static bool handleWebSocketHandshake(struct HttpRequest *request, struct HttpResponse *response)
{
    const char *ws_key = httpRequestGetHeader(request, "Sec-WebSocket-Key");
    if (!ws_key)
    {
        // ���û�� Sec-WebSocket-Key ͷ����˵������һ���Ϸ��� WebSocket ��������
        return false;
    }
    // �ǺϷ��� WebSocket ��������
    // ���� Sec-WebSocket-Accept ��Ӧͷ
    // ƴ�� GUID
    char acceptKey[128] = {0};
    snprintf(acceptKey, sizeof(acceptKey), "%s%s", ws_key, WEBSOCKET_GUID);

    // ��ƴ�Ӻ���ַ������� SHA-1 ��ϣ����
    unsigned char hash[SHA_DIGEST_LENGTH] = {0};
    // ʹ�� SHA-1 ��ϣ�㷨�����ϣֵ
    SHA1((unsigned char *)acceptKey, strlen(acceptKey), hash);

    // �Թ�ϣֵ���� Base64 ����
    char base64Hash[128] = {0};
    int base64Len = base64Encode(hash, SHA_DIGEST_LENGTH, base64Hash);

    // ֮��������Ҫ�� Base64 �������ַ������ӵ���Ӧͷ��
    response->statusCode = SwitchingProtocols;
    strcpy(response->statusMsg, "Switching Protocols");
    // ���� Sec-WebSocket-Accept ��Ӧͷ
    httpResponseAddHeader(response, "Sec-WebSocket-Accept", base64Hash);
    // ���� Upgrade ��Ӧͷ
    httpResponseAddHeader(response, "Upgrade", "websocket");
    // ���� Connection ��Ӧͷ
    httpResponseAddHeader(response, "Connection", "Upgrade");
    // ���� Sec-WebSocket-Version ��Ӧͷ
    httpResponseAddHeader(response, "Sec-WebSocket-Version", "13");
    // ���� Sec-WebSocket-Protocol ��Ӧͷ
    // httpResponseAddHeader(response, "Sec-WebSocket-Protocol", "chat");
    // ������Ӧͷ
    return true;
}

// ������Ŀǰ�ǻ��� get �ģ�Http ������� WebSocket ��������
bool processHttpRequest(struct TcpConnection *conn, struct HttpRequest *request, struct HttpResponse *response)
{
    if (isWebSocketHandshake(request))
    {
        // ˵����һ�� WebSocket ��������
        // �Ժ���һ������������ WebSocket ��������
        bool flag = handleWebSocketHandshake(request, response);
        if (!flag)
        {
            response->statusCode = BadRequest;
            strcpy(response->statusMsg, "Bad Request");
            return false;
        }
        // ������ɺ󣬰Ѷ�Ӧ�� connection ����Ϊ WebSocket ����
        conn->isWebSocket = true;
        return true;
    }
    if (strcasecmp(request->method, "get") != 0)
    {
        // 说明不是 GET 请求
        // ˵������ GET
        return false;
    }
    // ת�������������ļ���
    // ֮���Կ����Ե��ã�����Ϊ���뺯���У�to ָ��ָ������λ��һ��һֱ�� from ָ��ǰ��
    decodeMsg(request->url, request->url);
    // ����˵���յ���һ�� GET ����
    // ���Ǵ����ͻ���Ҫ��ľ�̬��Դ��Ŀ¼/�ļ���
    char *file = NULL;
    // �޸�·�����������ʹ��Ŀ¼·��
    if (strcmp(request->url, "/") == 0)
    {
        file = "./";
    }
    else
    {
        // ָ�����һ���ֽڣ�������ͷ�� "/" ����
        file = request->url + 1;
    }
    // ��ȡ�ļ�����
    // ����ṹ�嶨���� <sys/stat.h> ��
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1)
    {
        // ��ȡ�ļ�������Ϣ�����ڣ�Ҳ��˵���ļ������Ͳ�����
        // ��ô������Ҫ����һ�� 404 ����
        strcpy(response->fileName, "404.html");
        response->statusCode = NotFound;
        strcpy(response->statusMsg, "Not Found");
        // �����Ӧͷ
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        response->sendDataFunc = sendFile;
        return 0;
    }

    strcpy(response->fileName, file);
    response->statusCode = OK;
    strcpy(response->statusMsg, "OK");
    // �ж��ļ�����
    if (S_ISDIR(st.st_mode))
    {
        // printf("\n\n\n\n\n\nThis is a directory!!\n\n\n\n\n\n");
        //  ˵����һ��Ŀ¼
        //  ��ô���ǰ����Ŀ¼�е����ݷ������ͻ���
        //  �����Ӧͷ
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        response->sendDataFunc = sendDir;
        // printf("The address of sendDir is %p\n", sendDir);
    }
    else
    {
        // 是一个正常的 GET 请求，我们引入解析 Range 请求的逻辑代码
        const char *rangeHeader = httpRequestGetHeader(request, "Range");
        if (rangeHeader != NULL)
        {
            // 解析 Range 头，格式一般为 "bytes=start-end"
            int start = 0, end = 0;
            if (sscanf(rangeHeader, "bytes=%d-%d", &start, &end) >= 1)
            {
                printf("Parsed Range: %d-%d\n", start, end);

                // 解析成功，我们可以进一步判断
                if (end == 0 || end >= st.st_size)
                {
                    // 如果 end 没有指定，或者请求的范围超过了文件大小，那么我们强制指定其为文件大小 -1
                    end = st.st_size - 1;
                }

                // 计算请求内容的长度
                int contentLength = end - start + 1;

                // 拼响应数据
                response->statusCode = 206; // Partial Content
                strcpy(response->statusMsg, "Partial Content");

                // 设置 Content-Range 头
                char contentRange[128];
                sprintf(contentRange, "bytes %d-%d/%ld", start, end, st.st_size);
                httpResponseAddHeader(response, "Content-Range", contentRange);

                // 设置 Accept-Ranges 头
                httpResponseAddHeader(response, "Accept-Ranges", "bytes");

                // 设置 Content-Length 头
                char contentLengthStr[32];
                sprintf(contentLengthStr, "%d", contentLength);
                httpResponseAddHeader(response, "Content-Length", contentLengthStr);

                // 打开写事件监听
                writeEventEnable(conn->channel, true);
                eventLoopModify(conn->evLoop, conn->channel);

                response->sendRangeDataFunc = sendRangeRequestData;

                response->fileOffset = start;
                response->fileLength = contentLength;
                response->fileFd = open(file, O_RDONLY);


                return true;
            }
        }
        // printf("\n\n\n\n\n\nThis is a File!!\n\n\n\n\n\n");
        //  ˵����һ���ļ�
        //  ��ô���ǰ�����ļ������ݷ������ͻ���
        //  �����Ӧͷ
        char tmp[12] = {0};
        sprintf(tmp, "%ld", st.st_size);
        httpResponseAddHeader(response, "Content-type", getFileType(file));
        printf("Got contenttype: %s\n", getFileType(file));
        httpResponseAddHeader(response, "Content-length", tmp);
        response->sendDataFunc = sendFile;
    }

    return true;
}
