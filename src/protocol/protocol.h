#pragma once

// -- SCHEMAS: --

// MSG_CHAT      -> username\0message\0
// MSG_JOIN      -> username\0
// MSG_LEAVE     -> username\0
// MSG_RENAME    -> old\0new\0
// MSG_PING      -> n/a
// MSG_USER_LIST_REQ -> n/a

// MSG_WELCOME   -> session_id\0
// MSG_ERROR     -> error_code_t(uint8_t), optional string\0
// MSG_USER_LIST -> uint8_t count, count x username\0
// MSG_PONG      -> n/a

// -- RANGES: --

// 0x01 - 0x0F: client -> server
// 0x10 - 0x1F: server -> client
// 0x20 - 0x2F: error codes

#include <stdint.h>

//--============
// -- OPCODES
//--============

#define MSG_CHAT           0x01 // client -> server
#define MSG_JOIN           0x02 // client -> server
#define MSG_LEAVE          0x03 // client -> server -> all other clients
#define MSG_RENAME         0x04 // client -> server -> all other clients
#define MSG_PING           0x05 // client -> server
#define MSG_USER_LIST_REQ  0x06 // client -> server

#define MSG_WELCOME        0x10 // server -> client; confirm join, return session_id
#define MSG_ERROR          0x11 // server -> client; carry error w/ payload
#define MSG_USER_LIST      0x12 // server -> client
#define MSG_PONG           0x13 // server -> client

//--============
// -- CONSTS
//--============

#define HEADER_SIZE (sizeof(msg_t))

#define MAX_PAYLOAD    4096 // bytes
#define MAX_USERNAME   32   // chars
#define MAX_SESSION_ID 64   // len

//--============
// -- STRUCTS
//--============

typedef enum __attribute__((packed)) {
	ERR_USERNAME_TAKEN   = 0x20,
	ERR_USERNAME_INVALID = 0x21,
	ERR_BAD_FRAME        = 0x22,
	ERR_NOT_JOINED       = 0x23,
} error_code_t;

typedef struct __attribute__((packed)) {
	uint64_t timestamp; // timestamp: unix epoch ms, from server
	uint64_t id;        // id: msg_id
	uint32_t seq;       // seq: message sequence #; client sends 0
	uint16_t len;       // len: payload (Bytes, MSB)
	uint8_t type;       // type: current opcode
	char payload[];
} msg_t;
