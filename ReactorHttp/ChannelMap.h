#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct ChannelMap
{
    int size;       // 记录指针指向的数组的元素个数
    // struct Channel* list[];      不用这样的静态数组，我们用动态数组
    struct Channel** list;
};

// 初始化
struct ChannelMap* channelMapInit(int size);
// 清空 map 
void ChannelMapClear(struct ChannelMap* map);
// 重新分配内存空间
// newSize: 要添加的元素的个数
// unitSize: 要添加的元素的大小（字节为单位）
bool makeMapRoom(struct ChannelMap* map, int newSize, int unitSize);