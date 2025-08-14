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
        // 容量每次扩大原来的一倍
        while (curSize < newSize)
        {
            curSize = curSize << 1;
        }
        // 扩容 realloc
        struct Channel** temp = realloc(map->list, curSize * unitSize);
        if (!temp)
        {
            return false;
        }
        // realloc 函数可能会修改分配的内存的实际起始地址，所以 map->list 的地址可能会发生改变
        // 只有内存分配成功的时候，我们才修改 map 中存储的内存地址
        map->list = temp;
        
        memset(&map->list[map->size], 0, (curSize - map->size) * unitSize);
        map->size = curSize;    
    }
    return true;
}
