#include "WebSocket.h"

// WebSocket 帧结构
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
    int readableSize = bufferReadableSize(readBuf);
    if (readableSize < 2)
    {
        return -1; // ���ݲ��㣬�޷�����
    }
    unsigned char* data = (unsigned char*)(readBuf->data + readBuf->readPos);
    // FIN 
    uint8_t fin = (data[0] & 0x80) >> 7; 
    // opcode 
    *opcode = data[0] & 0x0F;
    uint8_t mask = (data[1] & 0x80) >> 7;
    uint64_t len = data[1] & 0x7F;
    size_t pos = 2; 

    if (len == 126)
    {
        if (readableSize < 4)
        {
            return -1; 
        }
        len = (data[2] << 8) | data[3]; 
        pos += 2; 
    } 
    else if (len == 127)
    {
        if (readableSize < 10)
        {
            return -1; 
        }
        len = 0;
        for (int i = 0; i < 8; ++i)
        {
            len = (len << 8) | data[pos + i];
        }
        pos += 8; 
    }

    uint8_t maskingKey[4] = { 0 };
    if (mask)
    {
        if (readableSize < pos + 4)
        {
            return 0; 
        }
        memcpy(maskingKey, data + pos, 4);
        pos += 4;
    }

    if (readableSize < pos + len)
    {
        printf("readableSize: %d, pos: %zu, len: %zu\n", readableSize, pos, len);
        printf("Not enough data to read payload, expected %zu bytes but only %d bytes available.\n", len, readableSize - pos);
        return 0; 
    }

    if (payload && len > 0)
    {
        if (mask)
        {
            for (uint64_t i = 0; i < len; ++i)
            {
                payload[i] = data[pos + i] ^ maskingKey[i % 4];
            }
        }
        else
        {
            memcpy(payload, data + pos, len);
        }
        payload[len] = '\0'; 
    }
    if (payloadLen)
    {
        *payloadLen = len;
    }

    readBuf->readPos += pos + len;
    return (int)len;
}

void sendWebSocketTextFrame(struct TcpConnection* conn, const char* msg)
{
    size_t msgLen = strlen(msg);
    unsigned char frame[WS_MAX_HEADER_SIZE + 4194304];
    //unsigned char frame[4194304];

    // ���� FIN λ�Ͳ�����
    size_t pos = 0;
    // printf("Sending WebSocket text frame with message: %s\n", msg);
    frame[pos++] = 0x80 | WS_OPCODE_TEXT;

    if (msgLen <= 125)
    {
        frame[pos++] = (uint8_t)msgLen;
    }
    else if (msgLen <= 65535)
    {
        frame[pos++] = 126; 
        frame[pos++] = (msgLen >> 8) & 0xFF;
        frame[pos++] = msgLen & 0xFF; 
    }
    else
    {
        frame[pos++] = 127; 
        for (int i = 0; i < 8; ++i)
        {
            // 依次取出高位字节
            // note：高位字节存储在数组的低地址，低位字节存储在数组的高地址
            // 需要将高位字节依次放入数组
            frame[pos++] = (msgLen >> (56 - i * 8)) & 0xFF;
        }
    }
    // printf("Sending WebSocket text frame with length: %zu\n", msgLen);
    // printf("frame length: %zu\n", sizeof(frame));
    writeEventEnable(conn->channel, true);
    eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
    // 需要发送的数据大约是 4100 字节，建议分批发送
    memcpy(frame + pos, msg, msgLen);
    // 更新 pos 以指向当前帧的末尾
    pos += msgLen;

    // 将帧数据写入发送缓冲区
    bufferAppendData(conn->writeBuf, (char*)frame, pos);

}
