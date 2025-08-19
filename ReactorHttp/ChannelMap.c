#include "ChannelMap.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct ChannelMap* channelMapInit(int size)
{
    struct ChannelMap* map = (struct ChannelMap*)malloc(sizeof(struct ChannelMap));
    map->size = size;
    map->list = (struct Channel**)malloc(sizeof(struct Channel*) * size);
    return map;
}

void ChannelMapClear(struct ChannelMap* map)
{
    if (map)
    {
        for (int i = 0; i < map->size; ++i)
        {
            if (map->list[i])
            {
                free(map->list[i]);
            }
        }
        free(map->list);
        map->list = NULL;
    }
    map->size = 0;
}

bool makeMapRoom(struct ChannelMap* map, int newSize, int unitSize)
{
    if (map->size < newSize)
    {
        int curSize = map->size;
        // ����ÿ������ԭ����һ��
        while (curSize < newSize)
        {
            curSize = curSize << 1;
        }
        // ���� realloc
        struct Channel** temp = realloc(map->list, curSize * unitSize);
        if (!temp)
        {
            return false;
        }
        // realloc �������ܻ��޸ķ�����ڴ��ʵ����ʼ��ַ������ map->list �ĵ�ַ���ܻᷢ���ı�
        // ֻ���ڴ����ɹ���ʱ�����ǲ��޸� map �д洢���ڴ��ַ
        map->list = temp;
        
        memset(&map->list[map->size], 0, (curSize - map->size) * unitSize);
        map->size = curSize;    
    }
    return true;
}
