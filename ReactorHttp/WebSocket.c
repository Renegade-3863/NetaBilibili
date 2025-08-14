#include "WebSocket.h"

// WebSocket 帧格式说明
/*

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |   Extended payload length continued, if payload len == 127    |
     +-------------------------------+-------------------------------+
     |                               | Masking-key, if MASK set to 1 |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +---------------------------------------------------------------+
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+

*/

int parseWebSocketFrame(struct Buffer* readBuf, uint8_t* opcode, char* payload, size_t* payloadLen)
{
    // 如果不够两字节，这个帧中连长度信息都没有，无法解析
    int readableSize = bufferReadableSize(readBuf);
    if (readableSize < 2)
    {
        return -1; // 数据不足，无法解析
    }
    // 读取第一个字节，获取 FIN 和 opcode
    unsigned char* data = (unsigned char*)(readBuf->data + readBuf->readPos);
    // FIN 位
    uint8_t fin = (data[0] & 0x80) >> 7; 
    // opcode 位
    *opcode = data[0] & 0x0F;
    // 检查掩码位
    uint8_t mask = (data[1] & 0x80) >> 7;
    // 取出 payload 长度，取第二个字节的低 7 位
    uint64_t len = data[1] & 0x7F;
    // 读取位置，从第三个字节开始
    size_t pos = 2; 

    if (len == 126)
    {
        // 如果长度为 126，接下来两个字节是扩展长度
        if (readableSize < 4)
        {
            return -1; // 数据不足，无法解析
        }
        // 读取扩展长度
        len = (data[2] << 8) | data[3]; 
        // 更新位置
        pos += 2; 
    } 
    else if (len == 127)
    {
        // 如果长度值是 127，那么接下来是一个 8 字节的扩展长度
        if (readableSize < 10)
        {
            return -1; // 数据不足，无法解析
        }
        len = 0;
        for (int i = 0; i < 8; ++i)
        {
            len = (len << 8) | data[pos + i];
        }
        // 更新位置
        pos += 8; 
    }

    uint8_t maskingKey[4] = { 0 };
    if (mask)
    {
        // 如果掩码位为 1，那么接下来的 4 字节是掩码键
        if (readableSize < pos + 4)
        {
            return 0; // 数据不足，无法解析
        }
        // 读取掩码键
        memcpy(maskingKey, data + pos, 4);
        pos += 4;
    }

    // 检查是否有足够的空间来存储 payload
    if (readableSize < pos + len)
    {
        printf("readableSize: %d, pos: %zu, len: %zu\n", readableSize, pos, len);
        printf("Not enough data to read payload, expected %zu bytes but only %d bytes available.\n", len, readableSize - pos);
        return 0; // 数据不全，不做处理
    }

    // 读取 payload 数据
    if (payload && len > 0)
    {
        if (mask)
        {
            // 如果有掩码，那么就需要对数据进行解码
            for (uint64_t i = 0; i < len; ++i)
            {
                payload[i] = data[pos + i] ^ maskingKey[i % 4];
            }
        }
        else
        {
            // 如果没有掩码，那么直接复制数据
            memcpy(payload, data + pos, len);
        }
        payload[len] = '\0'; // 确保字符串以 null 结尾
    }
    // 更新 payload 长度
    if (payloadLen)
    {
        *payloadLen = len;
    }

    // 更新读头位置
    readBuf->readPos += pos + len;
    return (int)len;
}

void sendWebSocketTextFrame(struct TcpConnection* conn, const char* msg)
{
    size_t msgLen = strlen(msg);
    // 设定发送负载 4 MB
    unsigned char frame[WS_MAX_HEADER_SIZE + 4194304];
    // 不带帧头
    //unsigned char frame[4194304];

    // 设置 FIN 位和操作码
    size_t pos = 0;
    printf("Sending WebSocket text frame with message: %s\n", msg);
    // FIN 位为 1，表示这是一个完整的消息
    // 操作码为 0x1，表示这是一个文本帧
    frame[pos++] = 0x80 | WS_OPCODE_TEXT;

    if (msgLen <= 125)
    {
        // 如果 payload 长度小于等于 125，那么直接使用 1 字节表示长度即可
        frame[pos++] = (uint8_t)msgLen;
    }
    else if (msgLen <= 65535)
    {
        frame[pos++] = 126; // 使用 126 表示接下来是 2 字节的长度
        frame[pos++] = (msgLen >> 8) & 0xFF; // 高字节
        frame[pos++] = msgLen & 0xFF; // 低字节
    }
    else
    {
        frame[pos++] = 127; // 使用 127 表示接下来是 8 字节的长度
        for (int i = 0; i < 8; ++i)
        {
            // 依然是大端字节序
            // note：大端字节序是指高位字节存储在低地址，低位字节存储在高地址
            // 网络字节序是大端字节序
            frame[pos++] = (msgLen >> (56 - i * 8)) & 0xFF;
        }
    }
    printf("Sending WebSocket text frame with length: %zu\n", msgLen);
    printf("frame length: %zu\n", sizeof(frame));
    // 确保 WebSocket 连接的写事件被启用
    writeEventEnable(conn->channel, true);
    // 添加到事件循环的任务队列中，等待工作线程处理
    eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
    // 要发送的数据可能超过 4110 个字节，如何解决？
    memcpy(frame + pos, msg, msgLen);
    // 更新后，pos 代表了当前帧的长度
    pos += msgLen;

    // 发送数据写入到写缓冲区
    bufferAppendData(conn->writeBuf, (char*)frame, pos);

}
