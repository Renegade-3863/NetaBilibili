#define _GNU_SOURCE
#include "Buffer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <strings.h>
#include <errno.h>

struct Buffer* bufferInit(int size)
{
    struct Buffer* buffer = (struct Buffer*)malloc(sizeof(struct Buffer));
    if (buffer)
    {
        buffer->data = (char*)malloc(sizeof(char) * size);
        buffer->capacity = size;
        buffer->readPos = buffer->writePos = 0;
        // �ڴ��ʼ��
        memset(buffer->data, 0, size);
    }
    return buffer;
}

void bufferDestroy(struct Buffer* buf)
{
    if (buf)
    {
        if (buf->data)
        {
            free(buf->data);
            buf->data = NULL;
        }
    }
    free(buf);
}

void bufferExtendRoom(struct Buffer* buffer, int size)
{
    // 1. �ڴ��㹻�� - ����Ҫ����
    if (bufferWriteableSize(buffer) >= size)
    {
        return;
    }
    // 2. �ڴ湻�ã�������Ҫ�ϲ� - Ҳ����Ҫ����
    // ʣ��Ŀ�д�ڴ� + �Ѷ����ڴ� > size
    else if (buffer->readPos + bufferWriteableSize(buffer) > size)
    {
        // ��δ�����ֽڿ��ƶ��� buffer �Ŀ�ͷ��Ҳ���� data �ĵ�ַ�ϣ�
        // ��ȡδ�����ڴ��С
        int readable = bufferReadableSize(buffer);
        // �ƶ��ڴ�
        memcpy(buffer->data, buffer->data+buffer->readPos, readable);
        // ���� readPos �� writePos
        buffer->readPos = 0;
        buffer->writePos = readable;
    }
    // 3. �ڴ治���ã��ϲ�Ҳ������- ��Ҫ����
    else
    {
        printf("Buffer extend room: %d bytes\n", size);
        void* temp = realloc(buffer->data, buffer->capacity + size);
        if (!temp)
        {
            return; // �����ڴ�ʧ��
        }
        memset(temp + buffer->capacity, 0, size);
        // �������ݳ�Ա
        buffer->data = temp;
        buffer->capacity += size;
    }
}

int bufferWriteableSize(struct Buffer* buffer)
{
    return buffer->capacity-buffer->writePos;
}

int bufferReadableSize(struct Buffer* buffer)
{
    return buffer->writePos-buffer->readPos;
}

int bufferAppendData(struct Buffer* buffer, const char* data, int size)
{
    if (!buffer || !data || data <= 0)
    {
        // �޷�д�룬���� -1
        return -1;
    }
    // �ж��Ƿ���Ҫ���ݣ��������Ҫ����������ͽ�������
    bufferExtendRoom(buffer, size);
    // �������ݵĿ���
    memcpy(buffer->data + buffer->writePos, data, size);
    buffer->writePos += size;
    return 0;
}

int bufferAppendString(struct Buffer* buffer, const char* data)
{
    int size = strlen(data); 
    int ret = bufferAppendData(buffer, data, size);
    return ret;
}

int bufferSocketRead(struct Buffer* buffer, int fd)
{
    // read/recv/readv
    struct iovec vec[2];
    int writeable = bufferWriteableSize(buffer);
    // ��ʼ������Ԫ��
    vec[0].iov_base = buffer->data + buffer->writePos;
    vec[0].iov_len = writeable;
    // malloc �������ڴ棬������������ͷ�
    char* tmpbuf = (char*)malloc(40960);
    vec[1].iov_base = tmpbuf;
    vec[1].iov_len = 40960;
    // result ����ֵ����һ�����յ��˶����ֽڣ����� -1 ˵�����ú���ʧ��
    int result = readv(fd, vec, 2);
    printf("Read %d bytes from fd %d\n", result, fd);
    if (result == -1)
    {
        return -1;
    }
    else if (result <= writeable)
    {
        // ȫ��д�뵽�� vec[0] �У�Ҳ���� buffer ��
        buffer->writePos += result;
    }
    else
    {
        // �����˸��� buffer->writePos
        buffer->writePos = buffer->capacity;
        // ������Щ����д���� vec[1] �У�Ҳ���Ƕ���������ڴ��У���Ҫ�ƶ��� buffer ��
        bufferAppendData(buffer, tmpbuf, result - writeable);
    }
    free(tmpbuf);
    tmpbuf = NULL;
    return result;
}

char* bufferFindCRLF(struct Buffer* buffer)
{
    // strstr --> ��һ�����ַ�����ƥ��һ�����ַ��������� \0 �ͽ����ˣ�
    // memmem --> ��һ�������ݿ���ƥ��һ�������ݿ飨��Ҫָ�����ݿ�Ĵ�С��
    char* ptr = memmem(buffer->data + buffer->readPos, bufferReadableSize(buffer), "\r\n", 2);
    return ptr;
}

int bufferSendData(struct Buffer* buffer, int socket)
{
    // �ж� buffer ���������ݣ����������÷���
    int readable = bufferReadableSize(buffer);
    if (readable > 0)
    {
        //printf("Sending %d bytes data to socket %d\n", readable, socket);
        //printf("The thread that is sending data is %ld\n", pthread_self());
        int count = send(socket, buffer->data + buffer->readPos, readable, MSG_NOSIGNAL);
        //printf("Send Successful\n");
        if (count > 0)
        {
            buffer->readPos += count;
            usleep(1);
        }
        return count;
    }
    return 0;
}
