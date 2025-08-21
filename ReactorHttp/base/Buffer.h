#pragma once

struct Buffer
{
    // ָ���ڴ��ָ��
    char* data;
    // ����Ĵ�С
    int capacity;
    // ����Ķ�ͷ�±꣨�ֽ�Ϊ��λ��
    int readPos;
    // �����дͷ�±꣨�ֽ�Ϊ��λ��
    int writePos;
};

// ����ĳ�ʼ��
struct Buffer* bufferInit(int size);
// ���������
void bufferDestroy(struct Buffer* buf);
// �������ݺ���
void bufferExtendRoom(struct Buffer* buffer, int size);
// ��ȡʣ��Ŀ�д���ڴ������
int bufferWriteableSize(struct Buffer* buffer);
// ��ȡʣ��Ŀɶ����ڴ������
int bufferReadableSize(struct Buffer* buffer);
// д�ڴ�
// 1. ֱ��д��
// 2. �����׽������ݣ����ļ��������ж�ȡ���ݵ� buffer �У�
int bufferAppendData(struct Buffer* buffer, const char* data, int size);
int bufferAppendString(struct Buffer* buffer, const char* data);
int bufferSocketRead(struct Buffer* buffer, int fd);
// ���� \r\n ȡ��һ�У��ҵ��������ݿ��е�λ�ã����ظ�λ��
char* bufferFindCRLF(struct Buffer* buffer);
// ��������
int bufferSendData(struct Buffer* buffer, int socket);