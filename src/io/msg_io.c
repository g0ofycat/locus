#include "msg_io.h"

#include <string.h>
#include <WS2tcpip.h>

//--============
// -- PRIVATE
//--============

/// @brief Read n bytes from sock into buf
/// @return 0 on success, -1 on disconnect or error
static int read_exact(SOCKET sock, void *buf, size_t n) {
	size_t received = 0;

	while (received < n) {
		int r = recv(sock, (char *)buf + received, (int)(n - received), 0);
		if (r <= 0) return -1;

		received += r;
	}

	return 0;
}

/// @brief Write n bytes from buf to sock
/// @return 0 on success, -1 on error
static int write_exact(SOCKET sock, const void *buf, size_t n) {
	size_t sent = 0;

	while (sent < n) {
		int s = send(sock, (const char *)buf + sent, (int)(n - sent), 0);
		if (s <= 0) return -1;

		sent += s;
	}

	return 0;
}

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
msg_status_t msg_send(SOCKET sock, uint8_t type, uint8_t flags, const void *payload, uint16_t len) {
	if (len > MAX_PAYLOAD) return MSG_ERR_FRAME;
	if (len > 0 && payload == NULL) return MSG_ERR_FRAME;

	uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];
	msg_t *msg = (msg_t *)buf;

	msg->type      = type;
	msg->flags     = flags;
	msg->len       = htons(len);
	msg->seq       = 0;
	msg->timestamp = 0;

	if (len > 0)
		memcpy(msg->payload, payload, len);

	return write_exact(sock, buf, HEADER_SIZE + len) == 0 ? MSG_OK : MSG_ERR_IO;
}

/// @brief Read a complete framed message from a socket into buf
/// @param sock: Source socket
/// @param buf: Caller-allocated buffer (min: HEADER_SIZE + MAX_PAYLOAD bytes)
/// @param bufsz: Size of buf in bytes
/// @return MSG_OK on success, MSG_ERR_IO on disconnect, MSG_ERR_FRAME on malformed / oversized payload
msg_status_t msg_recv(SOCKET sock, msg_t *buf, size_t bufsz) {
	if (bufsz < HEADER_SIZE + MAX_PAYLOAD) return MSG_ERR_FRAME;

	if (read_exact(sock, buf, HEADER_SIZE) != 0) return MSG_ERR_IO;

	uint16_t payload_len = ntohs(buf->len);

	if (payload_len > MAX_PAYLOAD) return MSG_ERR_FRAME;
	if (payload_len > bufsz - HEADER_SIZE) return MSG_ERR_FRAME;

	if (payload_len == 0) return MSG_OK;

	return read_exact(sock, buf->payload, payload_len) == 0 ? MSG_OK : MSG_ERR_IO;
}
