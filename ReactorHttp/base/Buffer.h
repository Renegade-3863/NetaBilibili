#pragma once

struct Buffer
{
    // 指向内存的指针
    char* data;
    // 缓存的大小
    int capacity;
    // 缓存的读头下标（字节为单位）
    int readPos;
    // 缓存的写头下标（字节为单位）
    int writePos;
};

// 缓存的初始化
struct Buffer* bufferInit(int size);
// 缓存的销毁
void bufferDestroy(struct Buffer* buf);
// 缓存扩容函数
void bufferExtendRoom(struct Buffer* buffer, int size);
// 获取剩余的可写的内存的容量
int bufferWriteableSize(struct Buffer* buffer);
// 获取剩余的可读的内存的容量
int bufferReadableSize(struct Buffer* buffer);
// 写内存
// 1. 直接写入
// 2. 接收套接字数据（从文件描述符中读取数据到 buffer 中）
int bufferAppendData(struct Buffer* buffer, const char* data, int size);
int bufferAppendString(struct Buffer* buffer, const char* data);
int bufferSocketRead(struct Buffer* buffer, int fd);
// 根据 \r\n 取出一行，找到其在数据块中的位置，返回该位置
char* bufferFindCRLF(struct Buffer* buffer);
// 发送数据
int bufferSendData(struct Buffer* buffer, int socket);