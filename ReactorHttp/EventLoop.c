#include "EventLoop.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "WebSocket.h"


struct EventLoop* eventLoopInit()
{
    return eventLoopInitEx(NULL);
}

// д����
void taskWakeUp(struct EventLoop* evLoop)
{
    const char* msg = "����Ҫ��Ϊ������������!!!";
    write(evLoop->socketPair[0], msg, strlen(msg));
}

// ������
int readLocalMessage(void* arg)
{
    struct EventLoop* evLoop = (struct EventLoop*)arg;
    // ���ڻ�������
    char buf[256];
    read(evLoop->socketPair[1], buf, sizeof(buf));
    return 0;
}

struct EventLoop* eventLoopInitEx(const char* threadName)
{
    struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
    evLoop->isQuit = false;
    // �ĸ��߳�ִ������� eventLoopInitEx �������õ����߳� ID ���Ƕ�Ӧ�̵߳� ID
    evLoop->threadID = pthread_self();
    pthread_mutex_init(&evLoop->mutex, NULL);
    strcpy(evLoop->threadName, threadName == NULL ? "MainThread" : threadName);
    // ��Ĭ��ʹ��Ч�����ŵ� EpollDispatcher
    evLoop->dispatcher = &EpollDistatcher;
    evLoop->dispatcherData = evLoop->dispatcher->init();
    // ����
    evLoop->head = evLoop->tail = NULL;
    // map
    evLoop->channelMap = channelMapInit(128);
    // �̼߳���Ϣ���䣬ʹ�� AF_UNIX ���б����ڵ� UNIX �׽���ͨ�ż���
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, evLoop->socketPair);
    if (ret == -1)
    {
        perror("socketpair");
        exit(0);
    }
    // ָ������evLoop->socketPair[0] �������ݣ�evLoop->socketPair[1] ��������
    // Ҳ����˵��socketPair[1] ��Ҫ�ŵ� select, poll, epoll �ļ�⼯����
    struct Channel* channel = channelInit(evLoop->socketPair[1], ReadEvent, readLocalMessage, NULL, NULL, evLoop);
    // channel ���ӵ����������
    eventLoopAddTask(evLoop, channel, ADD);
    return evLoop;
}

int eventLoopRun(struct EventLoop* evLoop)
{
    assert(evLoop != NULL);
    // ȡ���¼��ַ��ͼ��ģ��
    struct Dispatcher* dispatcher = evLoop->dispatcher;
    // �Ƚ��߳� ID �Ƿ���������������̣߳���Ҫִ�� Run ��������ֻ���������ӣ����������������������
    if (evLoop->threadID != pthread_self())
    {
        return -1;
    }
    // ѭ�������¼�����
    while (!evLoop->isQuit)
    {
        dispatcher->dispatch(evLoop, 2);                // ��ʱʱ�� 2s
        // ֻ�����̻߳�ִ�е�������ڴ����¼���ͬʱ��Ҳ��Ҫ���κ��б�Ҫ��ʱ���������Լ���������е�����
        //printf("Thread %s is processing events...\n", evLoop->threadName);
        eventLoopProcessTask(evLoop);
    }
    return 0;
}

int eventActivate(struct EventLoop* evLoop, int fd, int event)
{
    if (fd < 0 || evLoop == NULL)
    {
        return -1;
    }
    // ȡ�� channel
    struct Channel* channel = evLoop->channelMap->list[fd];
    assert(channel->fd == fd);
    if (event & ReadEvent && channel->readCallback)
    {
        channel->readCallback(channel->arg);
    }
    if (event & WriteEvent && channel->writeCallback)
    {
        channel->writeCallback(channel->arg);
    }
    return 0;
}

int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type)
{
    // ����������������Դ
    pthread_mutex_lock(&evLoop->mutex);
    // �����½ڵ�
    struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
    node->channel = channel;
    node->type = type;
    node->next = NULL;
    // ����Ϊ��
    if (evLoop->head == NULL)
    {
        evLoop->head = evLoop->tail = node;
    }
    else
    {
        evLoop->tail->next = node;          // ����
        evLoop->tail = node;                // ����
    }
    pthread_mutex_unlock(&evLoop->mutex);
    // �����ڵ�
    /*
     * ϸ�ڣ�
     *  1. ���������ڵ�����ӣ������ǵ�ǰ�߳�Ҳ�����������̣߳����̣߳�
     *      1). �޸� fd ���¼�����ǰ���̷߳��𣬵�ǰ���̴߳���
     *      2). �����µ� fd����������ڵ�Ĳ����������̷߳����
     *  2. ���������̴߳���������У���Ҫ�ɵ�ǰ�����߳�������
    */
    if (evLoop->threadID == pthread_self())
    {
        // ��ǰ���߳� -- ֱ�Ӵ�����������е���������
        eventLoopProcessTask(evLoop);
    }
    else
    {
        // ���߳�  -- ֪ͨ���̴߳�����������е�����
        // 1. ���߳��ڹ��� 2. ���̱߳������ˣ�select, poll, epoll �е� timeout ����
        // ��ν����� 2��
        // �� select��poll��epoll ������Ӧ�ļ�⼯���ж�������һ�������ڷ������� "�����ļ�������"
        // �����߳��������߳���������������еĽ���ʱ�򣬾�������ļ���������д�����ݼ���
        // ���ֽ��������
        /*
            1. pipe ���� ����Ҫ���ڽ��̼�ͨ�ţ�Ҳ�������̼߳�ͨ��
            2. socket pair ���׽���ͨ�ţ�
        */
        // ���߳�Ĭ�����߳������������� taskWakeUp �����߳̽��� "����"
        taskWakeUp(evLoop);
    }
    return 0;
}

int eventLoopProcessTask(struct EventLoop* evLoop)
{
    // �����������������
    pthread_mutex_lock(&evLoop->mutex);
    // ȡ��ͷ���
    struct ChannelElement* head = evLoop->head;
    while (head)
    {
        struct Channel* channel = head->channel;
        if (head->type == ADD)
        {
            // ����
            //printf("Adding channel with fd: %d\n", channel->fd);
            eventLoopAdd(evLoop, channel);
        }
        else if (head->type == DELETE)
        {
            // ɾ��
            //printf("Removing channel with fd: %d\n", channel->fd);
            eventLoopRemove(evLoop, channel);
        }
        else if (head->type == MODIFY)
        {
            // �޸�
            //printf("Modifying channel with fd: %d\n", channel->fd);
            eventLoopModify(evLoop, channel);
        }
        // ɾ��������ɵĽ��
        struct ChannelElement* tmp = head;
        head = head->next;
        free(tmp);
        tmp = NULL;
    }
    // ���������У��Ƿ���Ҫ����һ����ǰ�� while ѭ���У���
    evLoop->head = evLoop->tail = NULL;
    pthread_mutex_unlock(&evLoop->mutex);
    return 0;
}

int eventLoopAdd(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size)
    {
        // channelMap ��û���㹻�Ŀռ����洢 fd - channel �ļ�ֵ��
        // ��Ҫ����
        // ����ʧ�ܵĻ���ֱ�ӷ��� -1
        if (!makeMapRoom(channelMap, fd, sizeof(struct Channel*)))
        {
            return -1;
        }
    }
    // �ҵ� fd ��Ӧ������Ԫ��λ�ã����洢
    if (channelMap->list[fd] == NULL)
    {
        // �����λ�ã��ͽ��д洢
        channelMap->list[fd] = channel;
        evLoop->dispatcher->add(channel, evLoop);
    }
    return 0;
}

int eventLoopRemove(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size)
    {
        // Ҫɾ�����ļ���������û�洢�� channelMap �У�Ҳ�Ͳ��� dispatcher ���ļ�����
        // ���ǲ������κ���
        return -1;
    }
    int ret = evLoop->dispatcher->remove(channel, evLoop);
    return ret;
}

int eventLoopModify(struct EventLoop* evLoop, struct Channel* channel)
{
    int fd = channel->fd;
    struct ChannelMap* channelMap = evLoop->channelMap;
    if (fd >= channelMap->size || channelMap->list[fd] == NULL)
    {
        // Ҫɾ�����ļ���������û�洢�� channelMap �У�Ҳ�Ͳ��� dispatcher ���ļ�����
        // ���ǲ������κ���
        return -1;
    }
    int ret = evLoop->dispatcher->modify(channel, evLoop);
    return ret;
}

int destroyChannel(struct EventLoop* evLoop, struct Channel* channel)
{
    // ɾ�� channel �� fd �Ķ�Ӧ��ϵ
    // ���ָ��
    evLoop->channelMap->list[channel->fd] = NULL;
    // �ر��ļ�������
    close(channel->fd);
    // �ͷ� channel �ڴ�
    free(channel);
    return 0;
}
