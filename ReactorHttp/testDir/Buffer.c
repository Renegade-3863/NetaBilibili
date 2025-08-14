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
    // 1. 内存足够用 - 不需要扩容
    if (bufferWriteableSize(buffer) >= size)
    {
        return;
    }
    // 2. 内存够用，但是需要合并 - 也不需要扩容
    // 剩余的可写内存 + 已读的内存 > size
    else if (buffer->readPos + bufferWriteableSize(buffer) > size)
    {
        // 把未读的字节块移动到 buffer 的开头（也就是 data 的地址上）
        // 获取未读的内存大小
        int readable = bufferReadableSize(buffer);
        // 移动内存
        memcpy(buffer->data, buffer->data+buffer->readPos, readable);
        // 更新 readPos 和 writePos
        buffer->readPos = 0;
        buffer->writePos = readable;
    }
    // 3. 内存不够用（合并也不够）- 需要扩容
    else
    {
        printf("Buffer extend room: %d bytes\n", size);
        void* temp = realloc(buffer->data, buffer->capacity + size);
        if (!temp)
        {
            return; // 分配内存失败
        }
        memset(temp + buffer->capacity, 0, size);
        // 更新数据成员
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
        // 无法写入，返回 -1
        return -1;
    }
    // 判断是否需要扩容，如果有需要，这个函数就进行扩容
    bufferExtendRoom(buffer, size);
    // 进行数据的拷贝
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
    // 初始化数组元素
    vec[0].iov_base = buffer->data + buffer->writePos;
    vec[0].iov_len = writeable;
    // malloc 出来的内存，别忘了用完后释放
    char* tmpbuf = (char*)malloc(40960);
    vec[1].iov_base = tmpbuf;
    vec[1].iov_len = 40960;
    // result 返回值代表一共接收到了多少字节，等于 -1 说明调用函数失败
    int result = readv(fd, vec, 2);
    printf("Read %d bytes from fd %d\n", result, fd);
    if (result == -1)
    {
        return -1;
    }
    else if (result <= writeable)
    {
        // 全部写入到了 vec[0] 中，也就是 buffer 中
        buffer->writePos += result;
    }
    else
    {
        // 别忘了更新 buffer->writePos
        buffer->writePos = buffer->capacity;
        // 否则，有些数据写到了 vec[1] 中，也就是额外申请的内存中，需要移动到 buffer 中
        bufferAppendData(buffer, tmpbuf, result - writeable);
    }
    free(tmpbuf);
    tmpbuf = NULL;
    return result;
}

char* bufferFindCRLF(struct Buffer* buffer)
{
    // strstr --> 从一个大字符串中匹配一个子字符串（遇到 \0 就结束了）
    // memmem --> 从一个大数据块中匹配一个子数据块（需要指定数据块的大小）
    char* ptr = memmem(buffer->data + buffer->readPos, bufferReadableSize(buffer), "\r\n", 2);
    return ptr;
}

int bufferSendData(struct Buffer* buffer, int socket)
{
    // 判断 buffer 中有无数据，无数据则不用发送
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
