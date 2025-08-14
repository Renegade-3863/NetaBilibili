#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct ChannelMap
{
    int size;       // ��¼ָ��ָ��������Ԫ�ظ���
    // struct Channel* list[];      ���������ľ�̬���飬�����ö�̬����
    struct Channel** list;
};

// ��ʼ��
struct ChannelMap* channelMapInit(int size);
// ��� map 
void ChannelMapClear(struct ChannelMap* map);
// ���·����ڴ�ռ�
// newSize: Ҫ��ӵ�Ԫ�صĸ���
// unitSize: Ҫ��ӵ�Ԫ�صĴ�С���ֽ�Ϊ��λ��
bool makeMapRoom(struct ChannelMap* map, int newSize, int unitSize);