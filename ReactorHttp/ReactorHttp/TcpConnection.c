#include "TcpConnection.h"
#include <stdlib.h>
#include <stdio.h>
#include "Log.h"


static int processRead(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    //printf("Process Read on fd %d\n", conn->response->statusCode);
    // ��������
    int count = bufferSocketRead(conn->readBuf, conn->channel->fd);

    Debug("���յ��� http �������ݣ�%s", conn->readBuf->data + conn->readBuf->readPos);
    // ��� conn->isWebSocket Ϊ true����ô˵���� WebSocket ���ӣ����Ǿ�Ҫ�� WebSocket Э�������д���
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
                // ��� flag < 0�����������ݲ�������Ҳ�����ǽ����Ѿ����
                break;
            }
            //printf("Parsed WebSocket frame with opcode: %d, payload length: %zu\n", opcode, payloadLen);
            if (opcode == WS_OPCODE_TEXT)
            {
                // ���� WebSocket �ı���Ϣ
                //printf("Received WebSocket text message: %s\n", payload);
                // ����
                sendWebSocketTextFrame(conn, payload);
            }
            else if (opcode == WS_OPCODE_CLOSE)
            {
                // �ͻ��˷����˹ر����ӵ� WebSocket ֡
                // ��ӹر����ӵ�������������У��ȴ��̴߳���
                //printf("Received WebSocket close frame, closing connection.\n");
                eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
                printf("WebSocket connection closed.\n");
                break; // �˳�ѭ��
            }
            // ��ʱ�������������͵� WebSocket ֡
        }
        //if (flag < 0)
        //{
        //    // ����ʧ�ܣ����������ݲ�����
        //    return -1;
        //}
        // ��� opcode �� WS_OPCODE_CLOSE����ô˵���ͻ��˷����˹ر����ӵ� WebSocket ֡
        // ���ǾͲ����ټ����������������
        // ֱ�ӷ��ؼ���
        if (opcode == WS_OPCODE_CLOSE)
        {
            return 0;
        }
        // ���򣬿ͻ��˷����� WebSocket ֡�Ѿ����������
        return 0;
    }
    //printf("Read %d bytes from fd %d\n", count, conn->channel->fd);
    if (count > 0)
    {
            // ���յ��� Http ���󣬽��� Http ����
            int socket = conn->channel->fd;
#ifdef MSG_SEND_AUTO
            // �� eventLoop ����Ӧ�ļ���������д�¼�
            writeEventEnable(conn->channel, true);
            eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
#endif
            bool flag = parseHttpRequest(conn, conn->request, conn->readBuf, conn->response, conn->writeBuf, socket);
            //printf("Parse Http Request %s\n", flag ? "success" : "failed");
            if (!flag)
            {
                // ����ʧ���ˣ��ظ�һ���򵥵� html
                char* errMsg = "Http/1.1 400 Bad Request\r\n\r\n";
                bufferAppendString(conn->writeBuf, errMsg);
                // ���ʹ�����Ϣ
                bufferSendData(conn->writeBuf, socket);
                // �Ͽ�����
                // ��ɾ����Ӧ�ļ����������¼������񽻸�������У��ȴ��̴߳���
                eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
                return -1;
            }
    }
#ifndef MSG_SEND_AUTO
    if (!conn->isWebSocket)
    {
        // �����һ�� HTTP ���ӣ���ô���۴���ɹ���񣬶�Ҫ��ͻ��˶Ͽ�����
        // ��ɾ����Ӧ�ļ����������¼������񽻸�������У��ȴ��̴߳���
        eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
    }
#endif
    return 0;
}

static int processWrite(void* arg)
{
    Debug("��ʼ�������ݵ���(����д�¼�����)..."); 
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    // ��������
    printf("Process Write on fd %d\n", conn->channel->fd);
    int count = bufferSendData(conn->writeBuf, conn->channel->fd);
    if (count > 0)
    {
        // ������һ��������
        // �ж������Ƿ�ȫ�����ͳ�ȥ��
        // �����ȫ�����ͳ�ȥ��
        if (bufferReadableSize(conn->writeBuf) == 0)
        {
            // ������� WebSocket ���ӣ��Ǿ��� HTTP ���ӣ�����ô�Ϳ��Թر�������
            if (!conn->isWebSocket)
            {
                // ���������
                // 1. ������Ҫ������ fd ��д�¼� -- �޸� channel �б�����¼�
                writeEventEnable(conn->channel, false);
                // 2. �޸� dispatcher �ļ�⼯�� -- ������������������
                eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
                // 3. ���Կ���ɾ�����ڵ�
                eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
            }
            // ��֮������� WebSocket ���ӣ���ô�Ͳ���Ҫ�ر�����
            else
            {
                // ��������ˣ������ȴ��ͻ��˷�������
                // ֻ��Ҫ�޸� Channel �е��¼�����ʱ�رռ��д�¼�
                writeEventEnable(conn->channel, false);
                // ��ӵ���������У��ȴ������̴߳���
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
    // Ĭ�ϲ��� WebSocket ����
    conn->isWebSocket = false;
    // �ѳ�ʼ�������� Channel ��Ӧ���ļ���������ӵ����������
    eventLoopAddTask(evLoop, conn->channel, ADD);
    Debug("�Ϳͻ��˽������ӣ�threadName: %s, threadID:%s, connName: %s", evLoop->threadName, evLoop->threadID, conn->name);

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
    Debug("���ӶϿ����ͷ���Դ��gameover��connName: %s", conn->name);
    return 0;
}
