#pragma once

#include <winsock2.h>

#include "../encryption/encrypt.h"
#include "../compression/compress.h"
#include "../protocol/protocol.h"

//--============
// -- STRUCTS
//--============

typedef enum {
	MSG_AGAIN = 1,       // MSG_AGAIN: EWOULDBLOCK on flush
	MSG_OK = 0,
	MSG_ERR_IO = -1,     // MSG_ERR_IO: disconnect or socket error
	MSG_ERR_FRAME = -2,  // MSG_ERR_FRAME: malformed frame / oversized payload
	MSG_ERR_FULL = -3,   // MSG_ERR_FULL: ring buf full
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
/// @param key: 32-byte AES-256-GCM encryption key
/// @return MSG_OK on success, MSG_ERR_IO on disconnect or send error
msg_status_t msg_send(SOCKET sock, uint8_t type, uint8_t flags, const void *payload, uint16_t len, const uint8_t *key);

/// @brief Read a complete framed message from a socket into buf
/// @param sock: Source socket
/// @param buf: Caller-allocated buffer (min: HEADER_SIZE + MAX_PAYLOAD bytes)
/// @param bufsz: Size of buf in bytes
/// @param key: 32-byte AES-256-GCM encryption key
/// @return MSG_OK on success, MSG_ERR_IO on disconnect, MSG_ERR_FRAME on malformed / oversized payload
msg_status_t msg_recv(SOCKET sock, msg_t *buf, size_t bufsz, const uint8_t *key);

/// @brief Serialize and enqueue a framed message into a client ring buffer (non-blocking, no send())
/// @param ring: Ring buffer
/// @param head: Read cursor
/// @param tail: Write cursor
/// @param pending: Bytes pending in ring
/// @param ring_size: Total ring capacity in bytes
/// @param type: Protocol opcode
/// @param flags: TODO: make flags
/// @param payload: Pointer to payload bytes, NULL if empty
/// @param len: Payload length in bytes
/// @param key: 32-byte AES-256-GCM encryption key
/// @return MSG_OK on success, MSG_ERR_FRAME on bad args, MSG_ERR_FULL if ring lacks space
msg_status_t msg_enqueue(uint8_t *ring, int *head, int *tail, int *pending,
		int ring_size, uint8_t type, uint8_t flags,
		const void *payload, uint16_t len, const uint8_t *key);

/// @brief Drain as much of the ring as possible via non-blocking send()
/// @param sock: Destination socket
/// @param ring: Ring buffer
/// @param head: Read cursor
/// @param pending: Bytes pending in ring
/// @param ring_size: Total ring capacity in bytes
/// @return MSG_OK on full drain, MSG_AGAIN on EWOULDBLOCK, MSG_ERR_IO on disconnect or error
msg_status_t msg_flush(SOCKET sock, uint8_t *ring, int *head, int *pending, int ring_size);
