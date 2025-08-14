#pragma once
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "Buffer.h"
#include "TcpConnection.h"

// WebSocket帧头最大长度
#define WS_MAX_HEADER_SIZE 14

// WebSocket 操作码
typedef enum {
    WS_OPCODE_CONTINUE = 0x0,
    WS_OPCODE_TEXT = 0x1,
    WS_OPCODE_BINARY = 0x2,
    WS_OPCODE_CLOSE = 0x8,
    WS_OPCODE_PING = 0x9,
    WS_OPCODE_PONG = 0xA
} WsOpcode;


// WebSocket 帧解析函数
// readBuf 中保存的是 WebSocket 帧数据
// 返回值为解析后的 payload 长度，如果解析失败则返回 -1
int parseWebSocketFrame(struct Buffer* readBuf, uint8_t* opcode, char* payload, size_t* payloadLen);
// WebSocket 文本帧发送函数
void sendWebSocketTextFrame(struct TcpConnection* conn, const char* msg);
