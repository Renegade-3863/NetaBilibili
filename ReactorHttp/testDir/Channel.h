#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// ���庯��ָ�룬��������Ƕ�/д�ص������Ĳ���ģ��
typedef int(*handleFunc)(void* arg);

// �����ļ��������Ķ�д�¼�
enum FDEvent
{
    Timeout = 0x01,
    ReadEvent = 0x02,
    WriteEvent = 0x04
};

struct Channel
{
    // �ļ�������
    int fd;
    // �¼�
    int events;
    // �ص�����
    handleFunc readCallback;
    handleFunc writeCallback;
    // ������ Channel ʱ����
    handleFunc destroyCallback;
    // �ص������Ĳ���
    void* arg;
};

// ��ʼ��һ�� Channel
// ָ����� Channel ��װ���ļ������� fd���Լ���Ӧ�ļ��������ϵĶ�/д�ص������Լ�����
struct Channel* channelInit(int fd, int events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void* arg);
/*
    Ϊʲô�������޸Ķ��¼��ĺ�����
    һ����˵���������˲����ܽ��ö��ļ��������Ķ��¼��������û���������Կͻ��˵�����������
*/
// �޸� fd ��д�¼� (��� or �����)
void writeEventEnable(struct Channel* channel, bool flag);
// �ж��Ƿ���Ҫ����ļ���������д�¼�
bool isWriteEventEnable(struct Channel* channel);