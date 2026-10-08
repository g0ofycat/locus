#ifndef SOCKET_COMPAT_H
#define SOCKET_COMPAT_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)

#define closesocket(s) close(s)
#define WSAGetLastError() (errno)
#define WSAPoll(fds, nfds, timeout) poll(fds, nfds, timeout)
#endif

static inline int set_socket_nonblocking(SOCKET sock) {
#ifdef _WIN32
	u_long mode = 1;
	return ioctlsocket(sock, FIONBIO, &mode);
#else
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags == -1) return -1;
	return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

static inline int socket_init(void) {
#ifdef _WIN32
	WSADATA wsa;
	return WSAStartup(MAKEWORD(2, 2), &wsa);
#else
	return 0;
#endif
}

static inline void socket_cleanup(void) {
#ifdef _WIN32
	WSACleanup();
#endif
}

#endif
