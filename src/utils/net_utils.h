#pragma once

#include <winsock2.h>

//--============
// -- PUBLIC
//--============

/// @brief Read n bytes from sock into buf
/// @return 0 on success, -1 on disconnect or error
static int read_exact(SOCKET sock, void *buf, size_t n) {
	size_t received = 0;

	while (received < n) {
		int r = recv(sock, (char *)buf + received, (int)(n - received), 0);
		if (r == SOCKET_ERROR) return -1;
		if (r == 0) return -1;

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
