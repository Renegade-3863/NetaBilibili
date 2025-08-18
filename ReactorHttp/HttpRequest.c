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
#define MAX_FILE_SIZE (5 * 1024 * 1024) // 5MB
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
    req->curState = PS_REQLINE;
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

ParseState HttpRequestState(struct HttpRequest *request)
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

// bool parseHttpRequestLine(struct HttpRequest *request, struct Buffer *readBuf)
// {
//     // ���������У������ַ���������ַ
//     char *end = bufferFindCRLF(readBuf); // \r\n
//     // �����ַ�����ʼ��ַ
//     char *start = readBuf->data + readBuf->readPos;
//     // �������ܳ���
//     int lineSize = end - start;

//     if (lineSize)
//     {
//         start = splitRequestLine(start, end, " ", &request->method);
//         printf("Method: %s\n", request->method);
//         start = splitRequestLine(start, end, " ", &request->url);
//         printf("URL: %s\n", request->url);
//         splitRequestLine(start, end, NULL, &request->version);
//         printf("Version: %s\n", request->version);
// #if 0
//         // get /xxx/xx.txt http/1.1
//         // ���󷽷�
//         char* space = memmem(start, lineSize, " ", 1);
//         assert(space);
//         int methodSize = space - start;
//         request->method = (char*)malloc(methodSize+1);
//         strncpy(request->method, start, methodSize);
//         request->method[methodSize] = '\0';

//         // ����� URL
//         start = space + 1;
//         space = memmem(start, end - start, " ", 1);
//         assert(space);
//         int urlSize = space - start;
//         request->url = (char*)malloc(urlSize + 1);
//         strncpy(request->url, start, urlSize);
//         request->method[urlSize] = '\0';

//         // ����� Http Э��汾
//         start = space + 1;
//         //space = memmem(start, end - start, " ", 1);
//         //assert(space);
//         //int urlSize = space - start;
//         request->version = (char*)malloc(end - start + 1);
//         strncpy(request->url, start, end-start);
//         request->method[end-start] = '\0';
// #endif

//         // Ϊ��������ͷ��׼��
//         readBuf->readPos += lineSize;
//         readBuf->readPos += 2;
//         // �޸Ľ���״̬
//         request->curState = PS_HEADERS;
//         // printf("Request Line: Method: %s, URL: %s, Version: %s\n", request->method, request->url, request->version);
//         return true;
//     }
//     return false;
// }

// bool parseHttpRequestHeader(struct HttpRequest *request, struct Buffer *readBuf)
// {
//     char *end = bufferFindCRLF(readBuf);
//     if (end)
//     {
//         char *start = readBuf->data + readBuf->readPos;
//         int lineSize = end - start;
//         // ������Ǳ�׼�� Http ���󣬲�Ҫ��ð�ź���Ŀո�
//         char *middle = memmem(start, lineSize, ": ", 2);
//         if (middle)
//         {
//             char *key = malloc(middle - start + 1);
//             strncpy(key, start, middle - start);
//             key[middle - start] = '\0';

//             char *value = malloc(end - middle - 1);
//             strncpy(value, middle + 2, end - middle - 2);
//             value[end - middle - 2] = '\0';

//             httpRequestAddHeader(request, key, value);
//             // �ƶ������ݵ�λ��
//             readBuf->readPos += lineSize;
//             readBuf->readPos += 2;
//         }
//         else
//         {
//             // ����ͷ���������ˣ���������
//             readBuf->readPos += 2;
//             // �޸Ľ���״̬
//             // ��ʱ���� Post ������ֻ���� Get ����
//             request->curState = ParseReqDone;
//         }
//         return true;
//     }
//     return false;
// }

/**
 * @return PARSE_INCOMPLETE: 当前 缓冲中的数据不足以完成下一步，调用者保留缓冲区，不关闭连接，等待下次可读
 * @return PARSE_OK: 解析成功
 * @return PARSE_ERROR: 解析出错，调用者准备发送 400 并关闭连接
 */
ParseResult parseHttpRequest(struct TcpConnection *conn, struct HttpRequest *request, struct Buffer *readBuf, struct HttpResponse *response, struct Buffer *sendBuf, int socket)
{
    /* Free any previously allocated strings/headers to avoid leaks when reusing HttpRequest */
    if (request->method)
    {
        free(request->method);
        request->method = NULL;
    }
    if (request->url)
    {
        free(request->url);
        request->url = NULL;
    }
    if (request->version)
    {
        free(request->version);
        request->version = NULL;
    }
    if (request->reqHeaders)
    {
        for (int i = 0; i < request->reqHeadersNum; ++i)
        {
            free(request->reqHeaders[i].key);
            free(request->reqHeaders[i].value);
        }
        request->reqHeadersNum = 0;
    }

    // 快速别名？
    char *base = readBuf->data;
    int readable = bufferReadableSize(readBuf);
    // 解析不了，就返回 PARSE_INCOMPLETE
    if (readable <= 0)
    {
        return PARSE_INCOMPLETE;
    }

    // 1) 找到 headers 结束标记 "\r\n\r\n"
    char *hdr_end = memmem(base + readBuf->readPos, readable, "\r\n\r\n", 4);
    if (!hdr_end)
    {
        // header 未完整到达
        return PARSE_INCOMPLETE;
    }

    // header block 范围 [start, hdr_end + 4]，也就是函数头的部分
    char *start = base + readBuf->readPos;
    size_t header_block_len = (hdr_end + 4) - start;
    hdr_end += 4;
    // 2) 解析 request line （第一行）：method SP url SP version CRLF
    char *line_end = memmem(start, header_block_len, "\r\n", 2);
    if (!line_end)
    {
        // 不太可能，因为上面的检查已经确保了 header_block_len > 0，如果出现这种情况，说明发过来的不是合法的 HTTP 请求
        printf("parseHttpRequest: Invalid request line, no CRLF found\n");
        return PARSE_ERROR;
    }
    // 到这里，说明请求行是完整的，我们可以对它进行切分，调用 helper function
    // 解析 method
    char *after = splitRequestLine(start, line_end, " ", &request->method);
    if (!after)
    {
        // 切分失败，肯定是出问题了，返回 error
        printf("parseHttpRequest: Failed to parse method\n");
        return PARSE_ERROR;
    }
    // 解析 URL
    after = splitRequestLine(after, line_end, " ", &request->url);
    // 如果后面携带了 query 部分，要剪掉
    char *query_start = strchr(request->url, '?');
    if (query_start)
    {
        *query_start = '\0';
    }
    if (!after)
    {
        printf("parseHttpRequest: Failed to parse URL\n");
        return PARSE_ERROR;
    }
    // 解析 version
    char *tmpver = NULL;
    splitRequestLine(after, line_end, NULL, &tmpver);
    request->version = tmpver;

    // 3) 解析 headers 部分（从 line_end + 2 到 hdr_end）
    // 先清理旧的 headers
    request->reqHeadersNum = 0;
    char *hp = line_end + 2;
    while (hp < hdr_end)
    {
        // printf("parseHttpRequest: Parsing header line: %.*s\n", (int)(hdr_end - hp), hp);
        // 找到下一行 header
        char *next_eol = memmem(hp, hdr_end - hp, "\r\n", 2);
        if (!next_eol)
        {
            // 格式非法
            printf("parseHttpRequest: Invalid header format, no CRLF found\n");
            return PARSE_ERROR;
        }
        size_t line_len = next_eol - hp;
        if (line_len == 0)
        {
            // 空行，我们认为 headers 结束
            break;
        }
        // 查找 ": "
        char *colon = memmem(hp, line_len, ": ", 2);
        if (!colon)
        {
            // 非法的 header 格式：视为 parse error
            printf("parseHttpRequest: Invalid header format, no colon found\n");
            return PARSE_ERROR;
        }
        size_t key_len = colon - hp;
        size_t val_len = line_len - key_len - 2; // -2 是因为 ": " 的长度
        char *key = (char *)malloc(key_len + 1);
        char *val = (char *)malloc(val_len + 1);
        if (!key || !val)
        {
            // 内存分配失败
            free(key);
            free(val);
            printf("parseHttpRequest: Failed to allocate memory for headers\n");
            return PARSE_ERROR;
        }
        memcpy(key, hp, key_len);
        key[key_len] = '\0';
        memcpy(val, colon + 2, val_len); // +2 跳过 ": "
        val[val_len] = '\0';
        // 保存 header
        httpRequestAddHeader(request, key, val);
        // 移动到下一行
        hp = next_eol + 2;
    }

    // 4) 决定是否有 body (检查 Content-Length 或 Transfer-Encoding: chunked)
    char *cl = httpRequestGetHeader(request, "Content-Length");
    char *te = httpRequestGetHeader(request, "Transfer-Encoding");
    long content_length = -1;
    // 用于判断是否是分块传输
    bool is_chunked = false;

    if (cl)
    {
        content_length = atol(cl);
        if (content_length < 0)
        {
            content_length = -1;
        }
    }
    if (te)
    {
        // 忽略大小写检查
        if (strcasestr(te, "chunked") != NULL)
        {
            is_chunked = true;
        }
    }

    // 定位 body 的起始位置（如果 header_block_len > 0，则 body_start = readPos + header_block_len)
    int body_start_pos = readBuf->readPos + header_block_len;
    // 判断 body 总共有多少可读的数据
    int total_readable_after_headers = bufferReadableSize(readBuf) - (int)header_block_len;

    if (total_readable_after_headers < 0)
    {
        total_readable_after_headers = 0;
    }

    // 5) 处理不同的 body 模式
    // 分块发送的 request
    /*
        <十六进制块大小>\r\n
        <块数据>\r\n
    */
    if (is_chunked)
    {
        // printf("parseHttpRequest: Chunked transfer encoding detected\n");
        // 如果是分块的，那么按块对数据进行遍历
        int cur_pos = body_start_pos;
        while (1)
        {
            // 找 chunk-size 行
            if (cur_pos >= readBuf->writePos)
            {
                // 说明没有数据
                return PARSE_INCOMPLETE;
            }
            // 定位 chunk-size 行尾
            char *p = memmem(base + cur_pos, readBuf->writePos - cur_pos, "\r\n", 2);
            if (!p)
            {
                // 没有找到 chunk-size 行尾，说明数据不完整
                return PARSE_INCOMPLETE;
            }
            // 找到了，计算 chunk_size
            size_t hex_len = p - (base + cur_pos);
            if (hex_len == 0)
            {
                // 没有块长度数据，返回 error
                return PARSE_ERROR;
            }
            // 复制 hex string
            char *hexstr = (char *)malloc(hex_len + 1);
            if (!hexstr)
            {
                // 内存分配失败
                return PARSE_ERROR;
            }
            memcpy(hexstr, base + cur_pos, hex_len);
            hexstr[hex_len] = '\0';
            // 去掉行首的空格（trim）
            char *trim = hexstr;
            while (*trim && isspace((unsigned char)*trim))
            {
                ++trim;
            }
            // 把长度值转换为十进制
            long chunk_size = strtol(trim, NULL, 16);
            free(hexstr); // 释放 hexstr
            // 把指针移动到下一个 chunk 的起始位置
            cur_pos = (int)((p - base) + 2);
            if (chunk_size < 0)
            {
                // 块长度不可能为负
                return PARSE_ERROR;
            }
            if (chunk_size == 0)
            {
                // 代表这是最后一个 chunk，会有一个尾随的 CRLF (在 '0\r\n' 后面)，并且会以 \r\n\r\n 结尾
                char *trail_end = memmem(base + cur_pos, readBuf->writePos - cur_pos, "\r\n\r\n", 4);
                if (!trail_end)
                {
                    // 没有找到尾随的 CRLF，说明数据不完整
                    return PARSE_INCOMPLETE;
                }
                // 消费直到 trail_end + 4 位置
                int consume_len = (int)((trail_end + 4) - (base + readBuf->readPos));
                // 移动 readPos 指针
                readBuf->readPos += consume_len;
                // 完成了一个请求
                // 调用业务逻辑并准备响应
                processHttpRequest(conn, request, response);
                httpResponsePrepareMsg(conn, response, sendBuf, socket);
                // 重置状态以便下次解析另一个请求
                request->curState = PS_REQLINE;
                // 返回 OK
                return PARSE_OK;
            }
            else
            {
                // 非最终块：检查是否有 chunk_size + 2 字节长的内容可解析
                if (readBuf->writePos - cur_pos < (int)(chunk_size + 2))
                {
                    // 数据不完整，返回 PARSE_INCOMPLETE
                    return PARSE_INCOMPLETE;
                }
                // 当前 chunk 的数据是完整的，我们移动 cur_pos 指针
                cur_pos += (int)chunk_size;
                // 验证 CRLF 存在
                if (cur_pos + 2 > readBuf->writePos)
                {
                    // CRLF 不完整
                    return PARSE_INCOMPLETE;
                }
                if (!(base[cur_pos] == '\r' && base[cur_pos + 1] == '\n'))
                {
                    // 如果两个当前 chunk 不是以 CRLF 结尾，返回 PARSE_ERROR，格式出错了
                    return PARSE_ERROR;
                }
                // 移动到下一个 chunk 的起始位置
                cur_pos += 2;
                // 继续循环直到 final_chunk
                continue;
            }
        } // 结束 chunk 循环
    }
    // 否则，非 chunked 传输方式
    // 我们知道传了 content_length 字节的内容
    else if (content_length >= 0)
    {
        // 我们需要 content_length 字节的数据存在
        if (total_readable_after_headers < content_length)
        {
            // 数据不完整，返回 PARSE_INCOMPLETE
            return PARSE_INCOMPLETE;
        }
        // 完整的 body 都在 buffer 中：消费 header+body
        int consume_len = (int)header_block_len + (int)content_length;
        // 移动读头
        readBuf->readPos += consume_len;
        // 调用业务处理并准备响应
        processHttpRequest(conn, request, response);
        httpResponsePrepareMsg(conn, response, sendBuf, socket);
        request->curState = PS_REQLINE; // 重置状态以便下次解析另一个请求
        // 返回 OK
        return PARSE_OK;
    }
    else
    {
        // 没有 body，也就是 (GET/HEAD 等方法)：只消费 header_block
        readBuf->readPos += (int)header_block_len;
        // 调用业务处理并准备响应
        processHttpRequest(conn, request, response);
        httpResponsePrepareMsg(conn, response, sendBuf, socket);
        request->curState = PS_REQLINE; // 重置状态以便下次
        return PARSE_OK;
    }

    // make the compiler happy
    printf("parseHttpRequest: Unexpected state\n");
    return PARSE_ERROR;
}
// Old version of parseHttpRequest, no longer used
// ParseResult parseHttpRequest(struct TcpConnection *conn, struct HttpRequest *request, struct Buffer *readBuf, struct HttpResponse *response, struct Buffer *sendBuf, int socket)
// {
//     bool flag = true;
//     while (request->curState != ParseReqDone)
//     {
//         switch (request->curState)
//         {
//         case PS_REQLINE:
//             flag = parseHttpRequestLine(request, readBuf);
//             break;
//         case PS_HEADERS:
//             flag = parseHttpRequestHeader(request, readBuf);
//             break;
//         case PS_BODY:
//             break;
//         default:
//             break;
//         }
//         if (!flag)
//         {
//             return flag;
//         }
//         // �ж��Ƿ��������ˣ��������ˣ���Ҫ׼���ظ�������
//         if (request->curState == ParseReqDone)
//         {
//             // 1. ���ݽ�������ԭʼ���ݣ��Կͻ��˵�������������
//             processHttpRequest(conn, request, response);
//             // 2. ��֯��Ӧ���ݲ����͸��ͻ���
//             httpResponsePrepareMsg(conn, response, sendBuf, socket);
//         }
//     }
//     request->curState = PS_REQLINE; // ״̬��ԭ����֤�ܼ��������ڶ������Ժ������
//     return flag;
// }

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
static void sendFilePartial(const char *filename, struct Buffer *sendBuf,
                            int cfd, int offset, int length)
{
    int fd = open(filename, O_RDONLY);
    assert(fd > 0);
    printf("fd: %d\n", fd);

    off_t off = offset;
    int remaining = length;

    while (remaining > 0)
    {
        // 一次最多发送 65536 字节
        size_t toSend = remaining > 65536 ? 65536 : remaining;
        ssize_t sent = sendfile(cfd, fd, &off, toSend);
        if (sent <= 0)
        {
            if (errno == EINTR)
            {
                // 被信号中断，重试
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
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
    close(fd);
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
    if (strcmp(dot, ".svg") == 0)
        return "image/svg+xml";
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
    if (strcmp(dot, ".js") == 0)
        return "text/javascript; charset=utf-8";
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
    if (fd <= 0)
    {
        perror("open");
        return;
    }
#if 1
    while (1)
    {
        char buf[102400];
        int len = read(fd, buf, sizeof(buf));
        if (len > 0)
        {
            int rc_append = bufferAppendData(sendBuf, buf, len);
            if (rc_append != 0)
            {
#ifndef MSG_SEND_AUTO
                int rc_send = bufferSendData(sendBuf, cfd);
                if (rc_send == -1)
                {
                    // 发生了致命错误，例如 broken pipe (EPIPE)，此时连接不再可用，我们直接关闭文件描述符即可
                    // 不再重复尝试进行发送
                    close(fd);
                    return;
                }
#endif
            }
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
    // 获取文件大小
    int size = lseek(fd, 0, SEEK_END);
    // 设置一个读写指针，指向 fd 文件的开头
    lseek(fd, 0, SEEK_SET);
    // 通过 sendfile 进行数据传输
    // 设定一个发送限速
    while (offset < size)
    {
        // 注意 sendfile 需要发送的数据块大小必须是一个完整的 offset 值和 size 之间的区间
        // 给 sendfile 设定一个发送限速
        int ret = sendfile(cfd, fd, &offset, size - offset);
        // 因为是 ET 模式，需要注意处理 EAGAIN 和 EWOULDBLOCK
        if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            printf("暂时无法发送数据...\n");
            // 直接返回即可
            return 0;
        }
        if (ret == -1 && errno != EAGAIN)
        {
            // 发生了致命错误，关闭文件描述符并返回 -1
            close(fd);
            return -1;
        }
        offset += ret;
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
    (void)base64Len;

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
    // 清除已有的 headers，防止重复头问题
    response->headerNum = 0;
    bzero(response->statusMsg, sizeof(response->statusMsg));
    response->sendDataFunc = NULL;
    response->sendRangeDataFunc = NULL;
    response->fileFd = -1;
    response->fileOffset = 0;
    response->fileLength = 0;
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
                // printf("Parsed Range: %d-%d\n", start, end);

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

                // 设置 Content-Type 头（Range 响应也需要 Content-Type，浏览器用于识别媒体类型）
                // httpResponseAddHeader(response, "Content-Type", getFileType(file));

                // 打开写事件监听
                writeEventEnable(conn->channel, true);
                eventLoopModify(conn->evLoop, conn->channel);
                response->fileOffset = start;
                // 对于这种较大的文件，似乎可以尝试先默认只发送 5 MB?
                response->fileLength = contentLength;
                response->fileFd = open(file, O_RDONLY);

                response->sendRangeDataFunc = sendRangeRequestData;

                return true;
            }
        }
        // 添加必要的头
        char tmp[12] = {0};
        sprintf(tmp, "%ld", st.st_size);

        httpResponseAddHeader(response, "Content-type", getFileType(file));
        // printf("Got contenttype: %s\n", getFileType(file));
        httpResponseAddHeader(response, "Content-length", tmp);
        // 添加一条分支：如果要发送的文件本身很大，同时是视频文件，不适合一次性读入内存并发送，就换用 sendfile 进行发送
        // if (st.st_size > 1024 * 1024 && strcmp(getFileType(file), "video/mp4") == 0)
        // {
        //     // 设定 sendRangeDataFunc，告诉服务器使用范围发送，也就是 sendfile
        //     response->sendRangeDataFunc = sendRangeRequestData;

        //     // 因为请求的是完整的文件，所以起始位置为第一个字节
        //     response->fileOffset = 0;
        //     response->fileLength = MAX_FILE_SIZE;
        //     // response->fileLength = st.st_size;
        //     response->fileFd = open(file, O_RDONLY);

        //     return true;
        // }
        // 小于 1MB 的文件，就使用 send 简单发送
        // printf("\n\n\n\n\n\nThis is a File!!\n\n\n\n\n\n");
        //  ˵����һ���ļ�
        //  ��ô���ǰ�����ļ������ݷ������ͻ���
        //  �����Ӧͷ
        // 打开写事件监听
        writeEventEnable(conn->channel, true);
        eventLoopModify(conn->evLoop, conn->channel);
        response->sendDataFunc = sendFile;
    }

    return true;
}
