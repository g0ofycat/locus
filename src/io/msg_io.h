#pragma once

#include <winsock2.h>

#include "../protocol/protocol.h"

//--============
// -- STRUCTS
//--============

typedef enum {
	MSG_OK        =  0,
	MSG_ERR_IO    = -1,  // MSG_ERR_IO: disconnect or socket error
	MSG_ERR_FRAME = -2,  // MSG_ERR_FRAME: malformed frame / oversized payload
} msg_status_t;

//--============
// -- PUBLIC
//--============

/// @brief Serialize and send a complete framed message over a socket
/// @param sock: Destination socket
/// @param type: Protocol opcode
/// @param flags: TODO: make flags
/// @param payload: Pointer to payload bytes, NULL if empty (MSG_PING, MSG_PONG)
/// @param len: Payload length in bytes
/// @return MSG_OK on success, MSG_ERR_IO on disconnect or send error
msg_status_t msg_send(SOCKET sock, uint8_t type, uint8_t flags, const void *payload, uint16_t len);

/// @brief Read a complete framed message from a socket into buf
/// @param sock: Source socket
/// @param buf: Caller-allocated buffer (min: HEADER_SIZE + MAX_PAYLOAD bytes)
/// @param bufsz: Size of buf in bytes
/// @return MSG_OK on success, MSG_ERR_IO on disconnect, MSG_ERR_FRAME on malformed / oversized payload
msg_status_t msg_recv(SOCKET sock, msg_t *buf, size_t bufsz);
