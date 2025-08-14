#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"

struct Listener
{
    int lfd;
    unsigned short port;
};

struct TcpServer
{
    int threadNum;
    struct EventLoop* mainLoop;
    struct ThreadPool* threadPool;
    struct Listener* listener;
};

// ��ʼ�� TCP ������ʵ����ָ��������Ҫ�󶨵Ķ˿ڣ��Լ�ʹ�õ��̳߳��е��̸߳���
struct TcpServer* tcpServerInit(unsigned short port, int threadNum);
// ��ʼ�������ļ�������
struct Listener* listenerInit(unsigned short port);
// ���� TCP ������
void tcpServerRun(struct TcpServer* server);