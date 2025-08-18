#include "TcpConnection.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "Log.h"

static int processRead(void *arg)
{
    struct TcpConnection *conn = (struct TcpConnection *)arg;

    // 1. 处理 WebSocket 连接的逻辑
    // Debug("���յ��� http �������ݣ�%s", conn->readBuf->data + conn->readBuf->readPos);
    // ��� conn->isWebSocket Ϊ true����ô˵���� WebSocket ���ӣ����Ǿ�Ҫ�� WebSocket Э�������д���
    if (conn->isWebSocket)
    {
        uint8_t opcode = 0;
        char payload[40960] = {0};
        size_t payloadLen = 0;
        int flag = 0;
        // DEBUG Msg
        int totalLen = 0;
        while (flag = parseWebSocketFrame(conn->readBuf, &opcode, payload, &payloadLen))
        {
            // printf("flag = %d\n", flag);
            // printf("opcode = %d\n", opcode);
            totalLen += flag;
            // printf("totalLen = %d\n", totalLen);
            if (flag < 0)
            {
                // ��� flag < 0�����������ݲ�������Ҳ�����ǽ����Ѿ����
                break;
            }
            // printf("Parsed WebSocket frame with opcode: %d, payload length: %zu\n", opcode, payloadLen);
            if (opcode == WS_OPCODE_TEXT)
            {
                // ���� WebSocket �ı���Ϣ
                // printf("Received WebSocket text message: %s\n", payload);
                // ����
                sendWebSocketTextFrame(conn, payload);
            }
            else if (opcode == WS_OPCODE_CLOSE)
            {
                // �ͻ��˷����˹ر����ӵ� WebSocket ֡
                // ���ӹر����ӵ�������������У��ȴ��̴߳���
                // printf("Received WebSocket close frame, closing connection.\n");
                eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
                printf("WebSocket connection closed.\n");
                break; // �˳�ѭ��
            }
            // ��ʱ�������������͵� WebSocket ֡
        }
        // if (flag < 0)
        //{
        //     // ����ʧ�ܣ����������ݲ�����
        //     return -1;
        // }
        //  ��� opcode �� WS_OPCODE_CLOSE����ô˵���ͻ��˷����˹ر����ӵ� WebSocket ֡
        //  ���ǾͲ����ټ����������������
        //  ֱ�ӷ��ؼ���
        if (opcode == WS_OPCODE_CLOSE)
        {
            return 0;
        }
        // ���򣬿ͻ��˷����� WebSocket ֡�Ѿ����������
        return 0;
    }

    // 2. 处理常规 HTTP 连接的逻辑
    // printf("Process Read on fd %d\n", conn->response->statusCode);
    // ��������
    // int count = bufferSocketRead(conn->readBuf, conn->channel->fd);

    // 循环读取，知道 EAGAIN / EWOULDBLOCK / error / peer close
    while (1)
    {
        int rc = bufferSocketRead(conn->readBuf, conn->channel->fd);
        // 还有数据未处理完，对于 ET 模式，必须继续循环以尽可能读完内核缓冲
        if (rc > 0)
        {
            continue;
        }
        // 否则，如果 rc == 0，表示对端已经正常关闭了连接，我们也没必要再读了
        else if (rc == 0)
        {
            break;
        }
        // 否则，rc 返回 -2，说明对端非正常关闭了连接，我们需要告知任务队列关闭对这个文件描述符的检测
        else if (rc == -2)
        {
            // 添加删除检测的任务到事件循环
            eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
            return 0;
        }
        else // 否则，说明这个 -1 是一个真实的错误，我们也必须终止连接
        {
            eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
            return -1;
        }
    }

    // printf("Read %d bytes from fd %d\n", count, conn->channel->fd);
    int socket = conn->channel->fd;
    while (bufferReadableSize(conn->readBuf) > 0)
    {
#ifdef MSG_SEND_AUTO
        // �� eventLoop ����Ӧ�ļ���������д�¼�
        writeEventEnable(conn->channel, true);
        eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
#endif
        ParseResult pr = parseHttpRequest(conn, conn->request, conn->readBuf, conn->response, conn->writeBuf, socket);
        // printf("Parse Http Request %s\n", flag ? "success" : "failed");
        if (pr == PARSE_ERROR)
        {
            // 解析错误：parse 可能没有把 400 写入 sendBuf，确保写出一个 400 并关闭
            if (bufferReadableSize(conn->writeBuf) == 0)
            {
                const char *errMsg = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
                bufferAppendString(conn->writeBuf, errMsg);
            }
            // 确保写事件已启用以便把 400 发出
            if (!isWriteEventEnable(conn->channel))
            {
                writeEventEnable(conn->channel, true);
                eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
            }
            // 不在此立刻 DELETE —— 在 processWrite 写完后删除
            break;
        }
        else if (pr == PARSE_INCOMPLETE)
        {
            // 数据不完整，我们等待下一次可读，不要关闭连接
            break;
        }
        else
        {
            // pr == PARSE_OK
            // 一个请求已被解析并处理，若产生了响应数据，确保注册写事件以便发送
            if (bufferReadableSize(conn->writeBuf) > 0)
            {
                if (!isWriteEventEnable(conn->channel))
                {
                    writeEventEnable(conn->channel, true);
                    eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
                }
            }
            continue;
        }
    }
    // #ifndef MSG_SEND_AUTO
    // if (!conn->isWebSocket)
    // {
    //     // �����һ�� HTTP ���ӣ���ô���۴����ɹ���񣬶�Ҫ��ͻ��˶Ͽ�����
    //     // ��ɾ����Ӧ�ļ����������¼������񽻸�������У��ȴ��̴߳���
    //     eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
    // }
    // #endif
    return 0;
}

static int processWrite(void *arg)
{
    struct TcpConnection *conn = (struct TcpConnection *)arg;
    // int count = bufferSendData(conn->writeBuf, conn->channel->fd);
    while (bufferReadableSize(conn->writeBuf) > 0)
    {
        int n = bufferSendData(conn->writeBuf, conn->channel->fd);
        if (n > 0)
        {
            // 写出了数据，继续循环
            continue;
        }
        else if (n == 0)
        {
            // EAGAIN，需要等待下一次 EPOLLOUT
            if (!isWriteEventEnable(conn->channel))
            {
                writeEventEnable(conn->channel, true);
                eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
            }
            return 0;
        }
        // n == -1，发送错误，删除连接即可
        eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
        perror("bufferSendData");
        return -1;
    }

    // 2) writeBuf 已空，若 response 指示有文件需要通过 sendfile 发送，继续使用 sendfile
    if (conn->response && conn->response->fileFd >= 0)
    {
        // 发送剩余的文件数据（处理 range/fileLength）
        off_t *offset = &conn->response->fileOffset;
        int to_send_once = 65536; // 每次尝试的最大字节数
        // 如果 fileLength 有指定，计算 remaining
        off_t remaining = conn->response->fileLength > 0 ? (off_t)conn->response->fileLength - *offset : -1;
        while (remaining != 0)
        {
            size_t chunk = to_send_once;
            if (remaining > 0 && remaining < (off_t)chunk)
            {
                chunk = (size_t)remaining;
            }
            ssize_t sent = sendfile(conn->channel->fd, conn->response->fileFd, offset, chunk);
            printf("Subsequent sending\n");
            if (sent > 0)
            {
                if (remaining > 0)
                {
                    remaining -= sent;
                }
                printf("remaining: %ld\n", remaining);
                continue;
            }
            if (sent == 0)
            {
                // 当没有可发送的数据时，视作 EAGAIN
                if (!isWriteEventEnable(conn->channel))
                {
                    writeEventEnable(conn->channel, true);
                    eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
                }
                return 0;
            }
            // sent < 0，要么是出错了，要么是 EAGAIN
            if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                if (!isWriteEventEnable(conn->channel))
                {
                    writeEventEnable(conn->channel, true);
                    eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
                }
                return 0;
            }
            else
            {
                // 出现错误，关闭连接
                perror("sendfile");
                eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
                return -1;
            }
        }
        // 发送完毕：关闭文件描述符并标记为已完成
        printf("closed after sending\n", conn->response->fileFd);
        close(conn->response->fileFd);
        conn->response->fileFd = -1;
        conn->response->fileOffset = 0;
        conn->response->fileLength = 0;
    }
    
    // 如果先前有 sendFile 函數未发完的数据，在这里继续发
    // if(conn->response && conn->response->sendDataFunc)
    // {
    //     // 再发送一次请求数据
    //     conn->response->sendDataFunc(conn->response->fileName, conn->writeBuf, conn->channel->fd);
    //     // 不进行后续处理，关闭写事件检测的检查再 sendDataFunc 中做
    //     return 0;
    // }

    // 3) 所有要发送的内容都已完成：取消写事件并根据类型决定是否删除连接
    if(isWriteEventEnable(conn->channel))
    {
        writeEventEnable(conn->channel, false);
        eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
    }

    // 4) 如果不是 WebSocket 连接，就直接断连即可
    if(!conn->isWebSocket)
    {
        eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
    }
    return 0;
}

struct TcpConnection *tcpConnectionInit(int fd, struct EventLoop *evLoop)
{
    struct TcpConnection *conn = (struct TcpConnection *)malloc(sizeof(struct TcpConnection));
    conn->evLoop = evLoop;
    conn->readBuf = bufferInit(10240);
    conn->writeBuf = bufferInit(10240);
    conn->request = httpRequestInit();
    conn->response = httpResponseInit();
    sprintf(conn->name, "Connection-%d", fd);
    conn->channel = channelInit(fd, ReadEvent, processRead, processWrite, tcpConnectionDestroy, conn);
    // Ĭ�ϲ��� WebSocket ����
    conn->isWebSocket = false;
    // �ѳ�ʼ�������� Channel ��Ӧ���ļ����������ӵ����������
    eventLoopAddTask(evLoop, conn->channel, ADD);
    Debug("�Ϳͻ��˽������ӣ�threadName: %s, threadID:%s, connName: %s", evLoop->threadName, evLoop->threadID, conn->name);

    return conn;
}

int tcpConnectionDestroy(void *arg)
{
    struct TcpConnection *conn = (struct TcpConnection *)arg;
    if (conn)
    {
        if (conn->readBuf && bufferReadableSize(conn->readBuf) == 0 && conn->writeBuf && bufferReadableSize(conn->writeBuf) == 0)
        {
            // printf("Connection is idle, closing: %s\n", conn->name);
            destroyChannel(conn->evLoop, conn->channel);
            bufferDestroy(conn->readBuf);
            bufferDestroy(conn->writeBuf);
            httpRequestDestroy(conn->request);
            httpResponseDestroy(conn->response);
            free(conn);
        }
    }
    // Debug("���ӶϿ����ͷ���Դ��gameover��connName: %s", conn->name);
    return 0;
}
