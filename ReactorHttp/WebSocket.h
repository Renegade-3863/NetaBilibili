#pragma once
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "Buffer.h"
#include "TcpConnection.h"

// WebSocket֡ͷ��󳤶�
#define WS_MAX_HEADER_SIZE 14

// WebSocket ������
typedef enum {
    WS_OPCODE_CONTINUE = 0x0,
    WS_OPCODE_TEXT = 0x1,
    WS_OPCODE_BINARY = 0x2,
    WS_OPCODE_CLOSE = 0x8,
    WS_OPCODE_PING = 0x9,
    WS_OPCODE_PONG = 0xA
} WsOpcode;


// WebSocket ֡��������
// readBuf �б������ WebSocket ֡����
// ����ֵΪ������� payload ���ȣ��������ʧ���򷵻� -1
int parseWebSocketFrame(struct Buffer* readBuf, uint8_t* opcode, char* payload, size_t* payloadLen);
// WebSocket �ı�֡���ͺ���
void sendWebSocketTextFrame(struct TcpConnection* conn, const char* msg);
