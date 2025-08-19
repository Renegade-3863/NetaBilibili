#include "WebSocket.h"

// WebSocket ֡��ʽ˵��
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
    // ����������ֽڣ����֡����������Ϣ��û�У��޷�����
    int readableSize = bufferReadableSize(readBuf);
    if (readableSize < 2)
    {
        return -1; // ���ݲ��㣬�޷�����
    }
    // ��ȡ��һ���ֽڣ���ȡ FIN �� opcode
    unsigned char* data = (unsigned char*)(readBuf->data + readBuf->readPos);
    // FIN λ
    uint8_t fin = (data[0] & 0x80) >> 7; 
    // opcode λ
    *opcode = data[0] & 0x0F;
    // �������λ
    uint8_t mask = (data[1] & 0x80) >> 7;
    // ȡ�� payload ���ȣ�ȡ�ڶ����ֽڵĵ� 7 λ
    uint64_t len = data[1] & 0x7F;
    // ��ȡλ�ã��ӵ������ֽڿ�ʼ
    size_t pos = 2; 

    if (len == 126)
    {
        // �������Ϊ 126�������������ֽ�����չ����
        if (readableSize < 4)
        {
            return -1; // ���ݲ��㣬�޷�����
        }
        // ��ȡ��չ����
        len = (data[2] << 8) | data[3]; 
        // ����λ��
        pos += 2; 
    } 
    else if (len == 127)
    {
        // �������ֵ�� 127����ô��������һ�� 8 �ֽڵ���չ����
        if (readableSize < 10)
        {
            return -1; // ���ݲ��㣬�޷�����
        }
        len = 0;
        for (int i = 0; i < 8; ++i)
        {
            len = (len << 8) | data[pos + i];
        }
        // ����λ��
        pos += 8; 
    }

    uint8_t maskingKey[4] = { 0 };
    if (mask)
    {
        // �������λΪ 1����ô�������� 4 �ֽ��������
        if (readableSize < pos + 4)
        {
            return 0; // ���ݲ��㣬�޷�����
        }
        // ��ȡ�����
        memcpy(maskingKey, data + pos, 4);
        pos += 4;
    }

    // ����Ƿ����㹻�Ŀռ����洢 payload
    if (readableSize < pos + len)
    {
        printf("readableSize: %d, pos: %zu, len: %zu\n", readableSize, pos, len);
        printf("Not enough data to read payload, expected %zu bytes but only %d bytes available.\n", len, readableSize - pos);
        return 0; // ���ݲ�ȫ����������
    }

    // ��ȡ payload ����
    if (payload && len > 0)
    {
        if (mask)
        {
            // ��������룬��ô����Ҫ�����ݽ��н���
            for (uint64_t i = 0; i < len; ++i)
            {
                payload[i] = data[pos + i] ^ maskingKey[i % 4];
            }
        }
        else
        {
            // ���û�����룬��ôֱ�Ӹ�������
            memcpy(payload, data + pos, len);
        }
        payload[len] = '\0'; // ȷ���ַ����� null ��β
    }
    // ���� payload ����
    if (payloadLen)
    {
        *payloadLen = len;
    }

    // ���¶�ͷλ��
    readBuf->readPos += pos + len;
    return (int)len;
}

void sendWebSocketTextFrame(struct TcpConnection* conn, const char* msg)
{
    size_t msgLen = strlen(msg);
    // �趨���͸��� 4 MB
    unsigned char frame[WS_MAX_HEADER_SIZE + 4194304];
    // ����֡ͷ
    //unsigned char frame[4194304];

    // ���� FIN λ�Ͳ�����
    size_t pos = 0;
    // printf("Sending WebSocket text frame with message: %s\n", msg);
    // FIN λΪ 1����ʾ����һ����������Ϣ
    // ������Ϊ 0x1����ʾ����һ���ı�֡
    frame[pos++] = 0x80 | WS_OPCODE_TEXT;

    if (msgLen <= 125)
    {
        // ��� payload ����С�ڵ��� 125����ôֱ��ʹ�� 1 �ֽڱ�ʾ���ȼ���
        frame[pos++] = (uint8_t)msgLen;
    }
    else if (msgLen <= 65535)
    {
        frame[pos++] = 126; // ʹ�� 126 ��ʾ�������� 2 �ֽڵĳ���
        frame[pos++] = (msgLen >> 8) & 0xFF; // ���ֽ�
        frame[pos++] = msgLen & 0xFF; // ���ֽ�
    }
    else
    {
        frame[pos++] = 127; // ʹ�� 127 ��ʾ�������� 8 �ֽڵĳ���
        for (int i = 0; i < 8; ++i)
        {
            // ��Ȼ�Ǵ���ֽ���
            // note������ֽ�����ָ��λ�ֽڴ洢�ڵ͵�ַ����λ�ֽڴ洢�ڸߵ�ַ
            // �����ֽ����Ǵ���ֽ���
            frame[pos++] = (msgLen >> (56 - i * 8)) & 0xFF;
        }
    }
    // printf("Sending WebSocket text frame with length: %zu\n", msgLen);
    // printf("frame length: %zu\n", sizeof(frame));
    // ȷ�� WebSocket ���ӵ�д�¼�������
    writeEventEnable(conn->channel, true);
    // ���ӵ��¼�ѭ������������У��ȴ������̴߳���
    eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
    // Ҫ���͵����ݿ��ܳ��� 4110 ���ֽڣ���ν����
    memcpy(frame + pos, msg, msgLen);
    // ���º�pos �����˵�ǰ֡�ĳ���
    pos += msgLen;

    // ��������д�뵽д������
    bufferAppendData(conn->writeBuf, (char*)frame, pos);

}
