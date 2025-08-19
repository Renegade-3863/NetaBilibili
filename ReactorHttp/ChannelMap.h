#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct ChannelMap
{
    int size;       // 记录当前数组的大小
    // struct Channel* list[];      这是一个灵活数组成员，表示动态数组
    struct Channel** list;
};

// 初始化
struct ChannelMap* channelMapInit(int size);
// 清空 map
void ChannelMapClear(struct ChannelMap* map);
// 扩展内存
// newSize: 需要扩展的元素的大小
// unitSize: 需要扩展的元素的大小，单位为字节
bool makeMapRoom(struct ChannelMap* map, int newSize, int unitSize);