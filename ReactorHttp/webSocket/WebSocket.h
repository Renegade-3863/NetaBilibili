#pragma once
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "Buffer.h"
#include "TcpConnection.h"

// WebSocket 帧头最大长度
#define WS_MAX_HEADER_SIZE 14

// WebSocket 帧类型
typedef enum {
    WS_OPCODE_CONTINUE = 0x0,
    WS_OPCODE_TEXT = 0x1,
    WS_OPCODE_BINARY = 0x2,
    WS_OPCODE_CLOSE = 0x8,
    WS_OPCODE_PING = 0x9,
    WS_OPCODE_PONG = 0xA
} WsOpcode;


// WebSocket 帧解析
int parseWebSocketFrame(struct Buffer* readBuf, uint8_t* opcode, char* payload, size_t* payloadLen);    
// WebSocket �ı�֡���ͺ���
void sendWebSocketTextFrame(struct TcpConnection* conn, const char* msg);
