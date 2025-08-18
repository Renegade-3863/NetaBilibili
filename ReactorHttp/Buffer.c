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

// 最大缓冲区上限，防止内存被无限制扩展（可根据需要调整）
#define MAX_BUFFER_CAPACITY (100 * 1024 * 1024) // 100MB

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
        // printf("Buffer extend room: %d bytes\n", size);
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
    if (!buffer || !data || size <= 0)
    {
        return -1;
    }
    // �ж��Ƿ���Ҫ���ݣ��������Ҫ����������ͽ�������
    bufferExtendRoom(buffer, size);
    int writeable = bufferWriteableSize(buffer);
    if (writeable < size)
    {
        // 扩容失败或超过上限
        printf("bufferAppendData: not enough space, need=%d, have=%d\n", size, writeable);
        return -1;
    }
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
    if (result == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            free(tmpbuf);
            return 0;
        }
        // 如果都不是，那么大概率连接已经断开，我们返回 -2 给调用者，用于后续处理
        perror("readv");
        free(tmpbuf);
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
        if (bufferAppendData(buffer, tmpbuf, result - writeable) != 0)
        {
            // append 失败，返回错误以便上层处理回压
        free(tmpbuf);
        return -1;
        }
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
        if (count > 0)
        {
            buffer->readPos += count;
            // 若已读空，重置指针
            if (buffer->readPos >= buffer->writePos)
            {
                buffer->readPos = buffer->writePos = 0;
            }
            else if (buffer->readPos > buffer->capacity / 2)
            {
                int remain = buffer->writePos - buffer->readPos;
                memmove(buffer->data, buffer->data + buffer->readPos, remain);
                buffer->readPos = 0;
                buffer->writePos = remain;
            }
            usleep(1);
            return count;
        }
        else if (count == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 0;
            }
            perror("send");
            return -1;
        }
        return 0;
    }
    return 0;
}
