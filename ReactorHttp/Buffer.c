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
        // 内存初始化
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
    // 1. 内存空间足够 - 无需扩展
    if (bufferWriteableSize(buffer) >= size)
    {
        return;
    }
    // 2. 需要合并 - 但不需要扩展
    else if (buffer->readPos + bufferWriteableSize(buffer) > size)
    {
        // 将未读数据移动到 buffer 的开头，并更新指针
        int readable = bufferReadableSize(buffer);
        memcpy(buffer->data, buffer->data+buffer->readPos, readable);
        // 更新 readPos 和 writePos
        buffer->readPos = 0;
        buffer->writePos = readable;
    }
    // 3. 需要扩展，且无法合并
    else
    {
        void* temp = realloc(buffer->data, buffer->capacity + size);
        if (!temp)
        {
            return; 
        }
        memset(temp + buffer->capacity, 0, size);
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
    // 判断是否需要扩展内存，若需要则进行扩展
    bufferExtendRoom(buffer, size);
    int writeable = bufferWriteableSize(buffer);
    if (writeable < size)
    {
        // 扩容失败或超过上限
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
    // 初始化 iovec 结构体数组
    vec[0].iov_base = buffer->data + buffer->writePos;
    vec[0].iov_len = writeable;
    // malloc 申请额外的内存空间，用于存储从 socket 读取的数据
    char* tmpbuf = (char*)malloc(40960);
    vec[1].iov_base = tmpbuf;
    vec[1].iov_len = 40960;
    // result 是 readv 的返回值，表示实际读取的字节数，-1 表示读取失败
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
        // 完全写入到 vec[0] 中，也就是 buffer 中
        buffer->writePos += result;
    }
    else
    {
        // 完全写入到 vec[0] 中，也就是 buffer 中
        buffer->writePos = buffer->capacity;
        // 将 vec[1] 中的数据移动到 buffer 中
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
    // strstr --> 查找第一个出现的子字符串
    // memmem --> 查找第一个出现的子数据
    char* ptr = memmem(buffer->data + buffer->readPos, bufferReadableSize(buffer), "\r\n", 2);
    return ptr;
}

int bufferSendData(struct Buffer* buffer, int socket)
{
    // 判断 buffer 中是否有可读数据，若没有则直接返回
    int readable = bufferReadableSize(buffer);
    if (readable > 0)
    {
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
