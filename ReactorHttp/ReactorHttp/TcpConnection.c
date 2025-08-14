#include "TcpConnection.h"
#include <stdlib.h>
#include <stdio.h>
#include "Log.h"


static int processRead(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    //printf("Process Read on fd %d\n", conn->response->statusCode);
    // 接收数据
    int count = bufferSocketRead(conn->readBuf, conn->channel->fd);

    Debug("接收到的 http 请求数据：%s", conn->readBuf->data + conn->readBuf->readPos);
    // 如果 conn->isWebSocket 为 true，那么说明是 WebSocket 连接，我们就要用 WebSocket 协议来进行处理
    if (conn->isWebSocket)
    {
        uint8_t opcode = 0;
        char payload[40960] = { 0 };
        size_t payloadLen = 0;
        int flag = 0;
        // DEBUG Msg
        int totalLen = 0;
        while (flag = parseWebSocketFrame(conn->readBuf, &opcode, payload, &payloadLen))
        {
            printf("flag = %d\n", flag);
            printf("opcode = %d\n", opcode);
            totalLen += flag;
            printf("totalLen = %d\n", totalLen);
            if(flag < 0)
            {
                // 如果 flag < 0，可能是数据不完整，也可能是解析已经完成
                break;
            }
            //printf("Parsed WebSocket frame with opcode: %d, payload length: %zu\n", opcode, payloadLen);
            if (opcode == WS_OPCODE_TEXT)
            {
                // 处理 WebSocket 文本消息
                //printf("Received WebSocket text message: %s\n", payload);
                // 回显
                sendWebSocketTextFrame(conn, payload);
            }
            else if (opcode == WS_OPCODE_CLOSE)
            {
                // 客户端发来了关闭连接的 WebSocket 帧
                // 添加关闭连接的任务到任务队列中，等待线程处理
                //printf("Received WebSocket close frame, closing connection.\n");
                eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
                printf("WebSocket connection closed.\n");
                break; // 退出循环
            }
            // 暂时不处理其他类型的 WebSocket 帧
        }
        //if (flag < 0)
        //{
        //    // 解析失败，可能是数据不完整
        //    return -1;
        //}
        // 如果 opcode 是 WS_OPCODE_CLOSE，那么说明客户端发来了关闭连接的 WebSocket 帧
        // 我们就不能再继续处理这个连接了
        // 直接返回即可
        if (opcode == WS_OPCODE_CLOSE)
        {
            return 0;
        }
        // 否则，客户端发来的 WebSocket 帧已经被处理完毕
        return 0;
    }
    //printf("Read %d bytes from fd %d\n", count, conn->channel->fd);
    if (count > 0)
    {
            // 接收到了 Http 请求，解析 Http 请求
            int socket = conn->channel->fd;
#ifdef MSG_SEND_AUTO
            // 让 eventLoop 检测对应文件描述符的写事件
            writeEventEnable(conn->channel, true);
            eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
#endif
            bool flag = parseHttpRequest(conn, conn->request, conn->readBuf, conn->response, conn->writeBuf, socket);
            //printf("Parse Http Request %s\n", flag ? "success" : "failed");
            if (!flag)
            {
                // 解析失败了，回复一个简单的 html
                char* errMsg = "Http/1.1 400 Bad Request\r\n\r\n";
                bufferAppendString(conn->writeBuf, errMsg);
                // 发送错误信息
                bufferSendData(conn->writeBuf, socket);
                // 断开连接
                // 把删除对应文件描述符的事件的任务交给任务队列，等待线程处理
                eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
                return -1;
            }
    }
#ifndef MSG_SEND_AUTO
    if (!conn->isWebSocket)
    {
        // 如果是一个 HTTP 连接，那么无论处理成功与否，都要与客户端断开连接
        // 把删除对应文件描述符的事件的任务交给任务队列，等待线程处理
        eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
    }
#endif
    return 0;
}

static int processWrite(void* arg)
{
    Debug("开始发送数据的了(基于写事件发送)..."); 
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    // 发送数据
    printf("Process Write on fd %d\n", conn->channel->fd);
    int count = bufferSendData(conn->writeBuf, conn->channel->fd);
    if (count > 0)
    {
        // 发送了一部分数据
        // 判断数据是否被全部发送出去了
        // 如果被全部发送出去了
        if (bufferReadableSize(conn->writeBuf) == 0)
        {
            // 如果不是 WebSocket 连接（那就是 HTTP 连接），那么就可以关闭连接了
            if (!conn->isWebSocket)
            {
                // 发送完成了
                // 1. 不再需要检测这个 fd 的写事件 -- 修改 channel 中保存的事件
                writeEventEnable(conn->channel, false);
                // 2. 修改 dispatcher 的检测集合 -- 添加新任务到任务队列中
                eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
                // 3. 可以考虑删除检测节点
                eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
            }
            // 反之，如果是 WebSocket 连接，那么就不需要关闭连接
            else
            {
                // 发送完成了，继续等待客户端发送数据
                // 只需要修改 Channel 中的事件，暂时关闭检测写事件
                writeEventEnable(conn->channel, false);
                // 添加到任务队列中，等待工作线程处理
                eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
            }
        }
    }
    return 0;
}

struct TcpConnection* tcpConnectionInit(int fd, struct EventLoop* evLoop)
{
    struct TcpConnection* conn = (struct TcpConnection*)malloc(sizeof(struct TcpConnection));
    conn->evLoop = evLoop;
    conn->readBuf = bufferInit(10240);
    conn->writeBuf = bufferInit(10240);
    conn->request = httpRequestInit();
    conn->response = httpResponseInit();
    sprintf(conn->name, "Connection-%d", fd);
    conn->channel = channelInit(fd, ReadEvent, processRead, processWrite, tcpConnectionDestroy, conn);
    // 默认不是 WebSocket 连接
    conn->isWebSocket = false;
    // 把初始化出来的 Channel 对应的文件描述符添加到任务队列中
    eventLoopAddTask(evLoop, conn->channel, ADD);
    Debug("和客户端建立连接，threadName: %s, threadID:%s, connName: %s", evLoop->threadName, evLoop->threadID, conn->name);

    return conn;
}

int tcpConnectionDestroy(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    if (conn)
    {
        if (conn->readBuf && bufferReadableSize(conn->readBuf) == 0 
            && conn->writeBuf && bufferReadableSize(conn->writeBuf) == 0)
        {
            destroyChannel(conn->evLoop, conn->channel);
            bufferDestroy(conn->readBuf);
            bufferDestroy(conn->writeBuf);
            httpRequestDestroy(conn->request);
            httpResponseDestroy(conn->response);
            free(conn);
        }
    }
    Debug("连接断开，释放资源，gameover，connName: %s", conn->name);
    return 0;
}
