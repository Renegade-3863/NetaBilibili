#include "HttpResponse.h"
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sendfile.h>

#define ResHeaderSize 16

struct HttpResponse* httpResponseInit()
{
    struct HttpResponse* response = (struct HttpResponse*)malloc(sizeof(struct HttpResponse));
    response->headerNum = 0;
    // Ĭ�ϳ�ʼ���� 16 ����λ
    int size = sizeof(struct ResponseHeader) * ResHeaderSize;
    response->headers = (struct ResponseHeader*)malloc(size);
    response->statusCode = Unknown;
    // ��ʼ�� headers ����
    bzero(response->headers, size);
    bzero(response->statusMsg, sizeof(response->statusMsg));
    // ��ʼ������ָ��Ϊ NULL
    response->sendDataFunc = NULL;
    response->fileFd = -1;
    response->fileOffset = 0;
    response->fileLength = 0;

    return response;
}

void httpResponseDestroy(struct HttpResponse* response)
{
    if (response)
    {
        free(response->headers);
        free(response);
    }
}

void httpResponseAddHeader(struct HttpResponse* response, const char* key, const char* value)
{
    if (response == NULL || key == NULL || value == NULL)
    {
        // �޷�����ͷ��ֱ�ӷ��ؼ���
        return;
    }
    strcpy(response->headers[response->headerNum].key, key);
    strcpy(response->headers[response->headerNum].value, value);
    response->headerNum++;
}

void httpResponsePrepareMsg(struct TcpConnection* conn, struct HttpResponse* response, struct Buffer* sendBuf, int socket)
{
    // ״̬��
    char tmp[1024] = { 0 };
    sprintf(tmp, "HTTP/1.1 %d %s\r\n", response->statusCode, response->statusMsg);
    //printf("Status line: %s\n", tmp);
    //printf("Send data to socket %d\n", socket);

    bufferAppendString(sendBuf, tmp);
    // ��Ӧͷ
    for (int i = 0; i < response->headerNum; ++i)
    {
        sprintf(tmp, "%s: %s\r\n", response->headers[i].key, response->headers[i].value);
        //printf("Header: %s\n", tmp);
        bufferAppendString(sendBuf, tmp);
    }
    // ����
    bufferAppendString(sendBuf, "\r\n");
#ifndef MSG_SEND_AUTO
    // дһ���֣��ͷ�һ���֣��Ȱ���Ӧͷ����ȥ
    bufferSendData(sendBuf, socket);
    //printf("Sending Data...!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n\n");
#endif
    // Ҫ�ظ����ͻ��˵�����
    //printf("Calling sendDataFunc at address %p\n", response->sendDataFunc);
    // ��� sendDataFunc ��Ϊ NULL��˵����������Ҫ����
    // ��� sendDataFunc Ϊ NULL��˵��û���ļ���Ҫ����
    if(response->sendDataFunc)
    {
        response->sendDataFunc(response->fileName, sendBuf, socket);
        return;
    }
    // 上面的调用的是完全发送数据的函数，另外判断一下是否需要继续部分发送
    if(response->sendRangeDataFunc)
    {
        response->sendRangeDataFunc(response, sendBuf, socket);
    }
    // 发送完毕后，检查文件描述符是否需要关闭
    if(response->fileFd < 0)
    {
        printf("File descriptor %d closed after sending\n", response->fileFd);

        // 文件发送完毕，可以关闭写事件监听
        writeEventEnable(conn->channel, false);
        eventLoopModify(conn->evLoop, conn->channel);
    }
}

void sendRangeRequestData(struct HttpResponse* response, struct Buffer* sendBuf, int socket)
{
    int fd = response->fileFd;
    off_t* offset = &response->fileOffset;
    int* remaining = &response->fileLength;

    if(fd < 0 || *remaining <= 0)
    {
        // 文件描述符无效或剩余长度小于等于 0，直接返回
        if (fd >= 0)
        {
            // 发送完成，关闭文件描述符
            close(fd);
            // 可以把 response 对应的函数指针清空，防止重复调用
            response->sendRangeDataFunc = NULL;
            response->fileFd = -1;
        }
        return;
    }
    // 还有数据要发，我们就发一下
    while(*remaining > 0)
    {
        // 一次最多发送 65536 字节
        size_t toSend = *remaining > 65536 ? 65536 : *remaining;
        printf("Sending %ld bytes from offset %ld\n", toSend, *offset);

        ssize_t sent = sendfile(socket, fd, offset, toSend);
        if(sent < 0)
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
                close(fd);
                response->fileFd = -1;
                *remaining = 0;
                response->sendRangeDataFunc = NULL; // 清空函数指针
                break;
            }
        }
        else if(sent == 0)
        {
            // 发送完成，我们 break 即可
            *remaining = 0;
            break;
        }
        *remaining -= sent;
    }

// Not necessary
#ifndef MSG_SEND_AUTO
    if(bufferReadableSize(sendBuf) > 0)
    {
        bufferSendData(sendBuf, socket);
    }
#endif

    // 如果发送完成，关闭文件描述符
    if(*remaining == 0 && fd >= 0)
    {
        close(fd);
        response->fileFd = -1;
    }

}