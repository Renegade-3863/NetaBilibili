#pragma once
#include "Channel.h"
#include "EventLoop.h"

struct EventLoop;
struct Dispatcher
{
    // init -- ��ʼ�� epoll, poll ���� select ��Ҫ�����ݿ�
    // ��ʼ�����������ݿ鲻�Ƽ���ȫ�ֱ������б��棬ʵ�ַ���Ϊ���浽 EventLoop �ṹ��ĳ�Ա��
    void* (*init) ();
    // ����
    int (*add) (struct Channel* channel, struct EventLoop* evLoop);
    // ɾ��
    int (*remove) (struct Channel* channel, struct EventLoop* evLoop);
    // �޸�
    int (*modify) (struct Channel* channel, struct EventLoop* evLoop);
    // �¼����
    int (*dispatch) (struct EventLoop* evLoop, int timeout);    // ��ʱʱ�䵥λ��s
    // ������� (�ر� fd �����ͷ��ڴ�)
    int (*clear) (struct EventLoop* evLoop);
};