#include <string.h>
#include <WS2tcpip.h>

#include "msg_io.h"
#include "../utils/net_utils.h"

//--============
// -- PRIVATE
//--============

/// @brief Write n bytes from src into ring at *tail, advancing tail
/// @param ring
/// @param tail
/// @param ring_size
/// @param src
/// @param n
static void ring_write(uint8_t *ring, int *tail, int ring_size, const void *src, int n)
{
	const uint8_t *s = src;

	for (int i = 0; i < n; i++) {
		ring[*tail] = s[i];
		*tail = (*tail + 1) % ring_size;
	}
}

//--============
// -- PUBLIC
//--============

/// @brief Serialize and send a complete framed message over a socket
/// @param sock: Destination socket
/// @param type: Protocol opcode
/// @param payload: Pointer to payload bytes, NULL if empty (MSG_PING, MSG_PONG)
/// @param len: Payload length in bytes
/// @param id: Message ID
/// @param key: 32-byte AES-256-GCM encryption key
/// @return MSG_OK on success, MSG_ERR_IO on disconnect or send error
msg_status_t msg_send(SOCKET sock, uint8_t type, const void *payload, uint16_t len, uint64_t id, const uint8_t *key) {
	if (len > MAX_PAYLOAD) return MSG_ERR_FRAME;
	if (len > 0 && payload == NULL) return MSG_ERR_FRAME;

	uint8_t buf[HEADER_SIZE + MAX_PAYLOAD + ENCRYPT_OVERHEAD];
	msg_t *msg = (msg_t *)buf;
	msg->id = id;
	msg->type = type;
	msg->seq = 0;
	msg->timestamp = 0;

	if (len == 0) {
		msg->len = 0;
		return write_exact(sock, buf, HEADER_SIZE) == 0 ? MSG_OK : MSG_ERR_IO;
	}

	uint8_t compressed[MAX_PAYLOAD];
	uint8_t encrypted[MAX_PAYLOAD + ENCRYPT_OVERHEAD];

	size_t compressed_len = compress_data(payload, len, compressed, sizeof(compressed));
	if (compressed_len == 0) return MSG_ERR_FRAME;

	size_t encrypted_len = encrypt_data(key, compressed, compressed_len, encrypted, sizeof(encrypted));
	if (encrypted_len == 0) return MSG_ERR_FRAME;

	msg->len = htons((uint16_t)encrypted_len);
	memcpy(msg->payload, encrypted, encrypted_len);

	return write_exact(sock, buf, HEADER_SIZE + encrypted_len) == 0 ? MSG_OK : MSG_ERR_IO;
}

/// @brief Read a complete framed message from a socket into buf
/// @param sock: Source socket
/// @param buf: Caller-allocated buffer (min: HEADER_SIZE + MAX_PAYLOAD bytes)
/// @param bufsz: Size of buf in bytes
/// @param key: 32-byte AES-256-GCM encryption key
/// @return MSG_OK on success, MSG_ERR_IO on disconnect, MSG_ERR_FRAME on malformed / oversized payload
msg_status_t msg_recv(SOCKET sock, msg_t *buf, size_t bufsz, const uint8_t *key) {
	if (bufsz < HEADER_SIZE + MAX_PAYLOAD) return MSG_ERR_FRAME;

	if (read_exact(sock, buf, HEADER_SIZE) != 0) return MSG_ERR_IO;

	uint16_t payload_len = ntohs(buf->len);

	if (payload_len == 0) return MSG_OK;
	if (payload_len > MAX_PAYLOAD) return MSG_ERR_FRAME;
	if (payload_len <= ENCRYPT_OVERHEAD) return MSG_ERR_FRAME;
	if (payload_len > bufsz - HEADER_SIZE) return MSG_ERR_FRAME;

	if (read_exact(sock, buf->payload, payload_len) != 0) return MSG_ERR_IO;

	uint8_t decrypted[MAX_PAYLOAD];
	size_t decrypted_len = decrypt_data(key, buf->payload, payload_len, decrypted, sizeof(decrypted));
	if (decrypted_len == 0) return MSG_ERR_FRAME;

	uint8_t decompressed[MAX_PAYLOAD];
	size_t decompressed_len = decompressed_data(decompressed, sizeof(decompressed), decrypted, decrypted_len);
	if (decompressed_len == 0) return MSG_ERR_FRAME;

	memcpy(buf->payload, decompressed, decompressed_len);
	buf->len = htons((uint16_t)decompressed_len);

	return MSG_OK;
}

/// @brief Serialize and enqueue a framed message into a client ring buffer (non-blocking, no send())
/// @param ring: Ring buffer
/// @param head: Read cursor
/// @param tail: Write cursor
/// @param pending: Bytes pending in ring
/// @param ring_size: Total ring capacity in bytes
/// @param type: Protocol opcode
/// @param payload: Pointer to payload bytes, NULL if empty
/// @param len: Payload length in bytes
/// @param id: Message ID
/// @param key: 32-byte AES-256-GCM encryption key
/// @return MSG_OK on success, MSG_ERR_FRAME on bad args, MSG_ERR_FULL if ring lacks space
msg_status_t msg_enqueue(uint8_t *ring, int *head, int *tail, int *pending,
		int ring_size, uint8_t type, const void *payload, uint16_t len,
		uint64_t id, const uint8_t *key)
{
	if (len > MAX_PAYLOAD) return MSG_ERR_FRAME;
	if (len > 0 && payload == NULL) return MSG_ERR_FRAME;

	if (len == 0) {
		if (ring_size - *pending < (int)HEADER_SIZE) return MSG_ERR_FULL;

		uint8_t hdr[HEADER_SIZE];
		msg_t *h = (msg_t *)hdr;
		h->id = id;
		h->type = type;
		h->len = 0;
		h->seq = 0;
		h->timestamp = 0;
		ring_write(ring, tail, ring_size, hdr, (int)HEADER_SIZE);
		*pending += (int)HEADER_SIZE;

		return MSG_OK;
	}

	uint8_t compressed[MAX_PAYLOAD];
	uint8_t encrypted[MAX_PAYLOAD + ENCRYPT_OVERHEAD];

	size_t compressed_len = compress_data(payload, len, compressed, sizeof(compressed));
	if (compressed_len == 0) return MSG_ERR_FRAME;

	size_t encrypted_len = encrypt_data(key, compressed, compressed_len, encrypted, sizeof(encrypted));
	if (encrypted_len == 0) return MSG_ERR_FRAME;

	if (ring_size - *pending < (int)(HEADER_SIZE + encrypted_len)) return MSG_ERR_FULL;

	uint8_t hdr[HEADER_SIZE];
	msg_t *h = (msg_t *)hdr;
	h->type = type;
	h->id = id;
	h->len = htons((uint16_t)encrypted_len);
	h->seq = 0;
	h->timestamp = 0;

	ring_write(ring, tail, ring_size, hdr, (int)HEADER_SIZE);
	ring_write(ring, tail, ring_size, encrypted, (int)encrypted_len);

	*pending += (int)(HEADER_SIZE + encrypted_len);

	return MSG_OK;
}

/// @brief Drain as much of the ring as possible via non-blocking send()
/// @param sock: Destination socket
/// @param ring: Ring buffer
/// @param head: Read cursor
/// @param pending: Bytes pending in ring
/// @param ring_size: Total ring capacity in bytes
/// @return MSG_OK on full drain, MSG_AGAIN on EWOULDBLOCK, MSG_ERR_IO on disconnect or error
msg_status_t msg_flush(SOCKET sock, uint8_t *ring, int *head, int *pending, int ring_size)
{
	while (*pending > 0) {
		int contiguous = ring_size - *head;
		int to_send = *pending < contiguous ? *pending : contiguous;

		int s = send(sock, (const char *)ring + *head, to_send, 0);
		if (s == SOCKET_ERROR) {
			int err = WSAGetLastError();
			return (err == WSAEWOULDBLOCK) ? MSG_AGAIN : MSG_ERR_IO;
		}

		*head = (*head + s) % ring_size;
		*pending -= s;
	}

	return MSG_OK;
}
