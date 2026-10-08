#ifndef SOCKET_COMPACT_H
#define SOCKET_COMPACT_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
typedef WSAPOLLFD socket_pollfd_t;
typedef int socket_socklen_t;

#define SOCKET_INVALID INVALID_SOCKET
#define SOCKET_POLLIN POLLRDNORM
#define SOCKET_POLLOUT POLLWRNORM
#define SOCKET_POLLHUP POLLHUP
#define SOCKET_POLLERR POLLERR
#define SOCKET_POLLNVAL 0
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

typedef int socket_t;
typedef struct pollfd socket_pollfd_t;
typedef socklen_t socket_socklen_t;

#define SOCKET_INVALID (-1)
#define SOCKET_ERROR   (-1)

#define SOCKET_POLLIN POLLIN
#define SOCKET_POLLOUT POLLOUT
#define SOCKET_POLLHUP POLLHUP
#define SOCKET_POLLERR POLLERR
#define SOCKET_POLLNVAL POLLNVAL
#endif

static inline socket_t socket_open(int domain, int type, int protocol) {
	return socket(domain, type, protocol);
}

static inline int socket_connect(socket_t sock, const struct sockaddr *addr, socket_socklen_t len) {
	return connect(sock, addr, len);
}

static inline int socket_bind(socket_t sock, const struct sockaddr *addr, socket_socklen_t len) {
	return bind(sock, addr, len);
}

static inline int socket_listen(socket_t sock, int backlog) {
	return listen(sock, backlog);
}

static inline socket_t socket_accept(socket_t sock) {
	return accept(sock, NULL, NULL);
}

static inline int socket_send(socket_t sock, const void *buf, int len, int flags) {
#ifndef _WIN32
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
#endif
	return (int)send(sock, (const char *)buf, len, flags);
}

static inline int socket_recv(socket_t sock, void *buf, int len, int flags) {
	return (int)recv(sock, (char *)buf, len, flags);
}

static inline int socket_set_option(socket_t sock, int level, int option, const void *value, socket_socklen_t len) {
#ifdef _WIN32
	return setsockopt(sock, level, option, (const char *)value, len);
#else
	return setsockopt(sock, level, option, value, len);
#endif
}

static inline int socket_close(socket_t sock) {
#ifdef _WIN32
	return closesocket(sock);
#else
	return close(sock);
#endif
}

static inline int socket_shutdown(socket_t sock) {
#ifdef _WIN32
	return shutdown(sock, SD_BOTH);
#else
	return shutdown(sock, SHUT_RDWR);
#endif
}

static inline int socket_poll(socket_pollfd_t *fds, unsigned long count, int timeout) {
#ifdef _WIN32
	return WSAPoll(fds, count, timeout);
#else
	return poll(fds, count, timeout);
#endif
}

static inline int socket_last_error(void) {
#ifdef _WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

static inline int socket_would_block(int error) {
#ifdef _WIN32
	return error == WSAEWOULDBLOCK;
#else
	return error == EWOULDBLOCK || error == EAGAIN;
#endif
}

static inline int set_socket_nonblocking(socket_t sock) {
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
