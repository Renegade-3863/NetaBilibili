#include "TcpServer.h"
#include "TcpConnection.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "Log.h"

struct TcpServer* tcpServerInit(unsigned short port, int threadNum)
{
    struct TcpServer* tcp = (struct TcpServer*)malloc(sizeof(struct TcpServer));
    tcp->listener = listenerInit(port);
    // ��ʼ�����¼�ѭ�������̵߳ģ����õ��� eventLoopInitEx ����
    tcp->mainLoop = eventLoopInit();
    tcp->threadNum = threadNum;
    tcp->threadPool = threadPoolInit(tcp->mainLoop, threadNum);
    //printf("TCP : %d\n", tcp->threadPool->threadNum);
    return tcp;
}

struct Listener* listenerInit(unsigned short port)
{
    struct Listener* listener = (struct Listener*)malloc(sizeof(struct Listener));
    // 1. ���ü������ļ���������fd��
    // ʹ�� Ipv4 �� TCP �׽��֣�TCP ����ʽЭ�飬������ SOCK_STREAM�������ʹ�õ�����ʽЭ�飩
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket");
        return NULL;
    }
    // 2. ���ö˿ڸ���
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        perror("setsockopt");
        return NULL;
    }
    // 3. �󶨶˿ں�
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    // ���������ݴ洢˳��ΪС������Ҫת�������ֽ��򣨴�ˣ�
    addr.sin_port = htons(port);
    // ���� 0 ��ַ����ʾ���԰󶨱������е� IP ��ַ
    addr.sin_addr.s_addr = INADDR_ANY;
    // ���ļ����������׽��ֵ�ַ
    ret = bind(lfd, (struct sockaddr*)(&addr), sizeof(addr));
    if (ret == -1)
    {
        perror("bind");
        return NULL;
    }
    // 4. ���ü���
    //printf("Listening on port %d\n", port);
    ret = listen(lfd, 128);
    //// ���÷������ļ���ģʽ
    //int flags = fcntl(lfd, F_GETFL, 0);
    //fcntl(lfd, F_SETFL, flags | O_NONBLOCK);
    listener->lfd = lfd;
    listener->port = port;
    // 5. ���ؼ������ļ�������
    return listener;
}

// �Ϳͻ��˽������ӣ�Ҫ���� accept ����
static int acceptConnection(void* arg)
{
    struct TcpServer* server = (struct TcpServer*)arg;
    // �ڶ�����������������������������ӵĿͻ��˵� IP �Ͷ˿���Ϣ��HTTP ����������Ҫ��¼�ⲿ����Ϣ
    // �������������������׽��ֵĳ��ȣ�Ҳ����Ҫ
    int cfd = accept(server->listener->lfd, NULL, NULL);
    if (cfd == -1)
    {
        perror("accept");
        return -1;
    }
    // ���̳߳���ȡ��һ���̵߳ķ�Ӧ��ģ��ʵ������������� cfd
    //printf("Accepted connection on fd: %d\n", cfd);
    struct EventLoop* evLoop = takeWorkerEventLoop(server->threadPool);
    if (evLoop == NULL)
    {
        perror("takeWorkerEventLoop");
        close(cfd);
        return -1;
    }
    // �� cfd �ŵ� TcpConnection ������
    tcpConnectionInit(cfd, evLoop);
    //printf("Accepted connection on fd: %d\n", cfd);
    return 0;
}

void tcpServerRun(struct TcpServer* server)
{
    // �����̳߳�
    threadPoolRun(server->threadPool);
    // ��ʼ�������������ļ��������� Channel ʵ��
    struct Channel* channel = channelInit(server->listener->lfd, ReadEvent, acceptConnection, NULL, NULL, server);
    //printf("Channel initialization successful\n");
    // ��� "��Ӽ����ļ�������" ���������������
    eventLoopAddTask(server->mainLoop, channel, ADD);
    //printf("Adding Channel Task successful\n");
    // ������Ӧ��ģ��
    //printf("Running EventLoop\n");    
    Debug("����������������...");
    eventLoopRun(server->mainLoop);

}
