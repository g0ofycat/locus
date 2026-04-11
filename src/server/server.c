#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "server.h"

//--================
// -- PRIVATE STATE
//--================

static server_client_t clients[MAX_CLIENTS];
static struct pollfd pfds[MAX_CLIENTS + 1]; // + 1 = listener
static int nfds = 1;						// pfds[0] = listener

//--================
// -- PRIVATE
//--================

/// @brief Generate random session ID (a-z, 0-9)
/// @out: Caller-allocated buffer
static void session_id_generate(char *out)
{
	static const char alphanum[] = "abcdefghijklmnopqrstuvwxyz0123456789";
	for (size_t i = 0; i < MAX_SESSION_ID - 1; i++)
		out[i] = alphanum[rand() % 36];
	out[MAX_SESSION_ID - 1] = '\0';
}

/// @brief Username taken
/// @param username
static int username_taken(const char *username)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].sock == INVALID_SOCKET)
			continue;

		if (strncmp(clients[i].username, username, MAX_USERNAME) == 0)
			return 1;
	}

	return 0;
}

// @brief Get client based on the socket
/// @param sock
static server_client_t *client_by_sock(SOCKET sock)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].sock == sock)
			return &clients[i];
	}

	return NULL;
}

//--================
// -- API
//--================

/// @brief Add a newly accepted socket to the client list
/// @param sock
/// @return Index on success, -1 if full
int client_add(SOCKET sock)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].sock != INVALID_SOCKET)
			continue;

		clients[i].sock = sock;
		clients[i].joined = 0;
		memset(clients[i].username, 0, MAX_USERNAME);
		memset(clients[i].session_id, 0, MAX_SESSION_ID);

		pfds[nfds].fd = sock;
		pfds[nfds].events = POLLIN;
		pfds[nfds].revents = 0;
		nfds++;

		return i;
	}

	return -1;
}

/// @brief Remove a client by socket, broadcasts MSG_LEAVE to others
void client_remove(SOCKET sock)
{
	server_client_t *c = client_by_sock(sock);
	if (c == NULL)
		return;

	if (c->joined)
	{
		char payload[MAX_USERNAME];
		snprintf(payload, MAX_USERNAME, "%s", c->username);
		broadcast(sock, MSG_LEAVE, payload, (uint16_t)strlen(payload) + 1);
	}

	closesocket(sock);
	memset(c, 0, sizeof(server_client_t));
	c->sock = INVALID_SOCKET;

	for (int i = 1; i < nfds; i++)
	{
		if (pfds[i].fd != sock)
			continue;
		pfds[i] = pfds[nfds - 1];
		nfds--;
		break;
	}
}

/// @brief Broadcast a framed message to all joined clients except sender
/// @param sender_sock: Socket to exclude
/// @param type: Protocol Opcode
/// @param payload: Data
/// @param len: Length of payload
/// @param sender_sock: Pass INVALID_SOCKET to broadcast to everyone
void broadcast(SOCKET sender_sock, uint8_t type, const void *payload, uint16_t len)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].sock == INVALID_SOCKET)
			continue;
		if (clients[i].sock == sender_sock)
			continue;
		if (!clients[i].joined)
			continue;

		msg_send(clients[i].sock, type, 0, payload, len);
	}
}

/// @brief Read and dispatch one message from a client
/// @param c: Message
void client_handle(server_client_t *c)
{
	uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];
	msg_t *msg = (msg_t *)buf;

	if (msg_recv(c->sock, msg, sizeof(buf)) != MSG_OK)
	{
		client_remove(c->sock);
		return;
	}

	if (!c->joined && msg->type != MSG_JOIN)
	{
		error_code_t err = ERR_NOT_JOINED;
		msg_send(c->sock, MSG_ERROR, 0, &err, sizeof(err));
		return;
	}

	switch (msg->type)
	{
		case MSG_JOIN:
			{
				char *username = msg->payload;
				username[MAX_USERNAME - 1] = '\0';

				if (username_taken(username))
				{
					error_code_t err = ERR_USERNAME_TAKEN;
					msg_send(c->sock, MSG_ERROR, 0, &err, sizeof(err));
					return;
				}

				snprintf(c->username, MAX_USERNAME, "%s", username);
				session_id_generate(c->session_id);
				c->joined = 1;

				msg_send(c->sock, MSG_WELCOME, 0, c->session_id, (uint16_t)strlen(c->session_id) + 1);
				broadcast(c->sock, MSG_JOIN, c->username, (uint16_t)strlen(c->username) + 1);
				break;
			}
		case MSG_CHAT:
			{
				char payload[MAX_USERNAME + MAX_PAYLOAD];
				int ulen = (int)strlen(c->username) + 1;
				int mlen = (int)ntohs(msg->len);

				memcpy(payload, c->username, ulen);
				memcpy(payload + ulen, msg->payload, mlen);
				broadcast(c->sock, MSG_CHAT, payload, (uint16_t)(ulen + mlen));
				break;
			}
		case MSG_LEAVE:
			{
				client_remove(c->sock);
				break;
			}
		case MSG_RENAME:
			{
				char *new_name = msg->payload + strlen(msg->payload) + 1;
				new_name[MAX_USERNAME - 1] = '\0';

				if (username_taken(new_name))
				{
					error_code_t err = ERR_USERNAME_TAKEN;
					msg_send(c->sock, MSG_ERROR, 0, &err, sizeof(err));
					return;
				}

				char payload[MAX_USERNAME * 2];
				int olen = (int)strlen(c->username) + 1;
				memcpy(payload, c->username, olen);
				snprintf(payload + olen, MAX_USERNAME, "%s", new_name);

				snprintf(c->username, MAX_USERNAME, "%s", new_name);
				broadcast(c->sock, MSG_RENAME, payload, (uint16_t)(olen + strlen(new_name) + 1));
				break;
			}
		case MSG_PING:
			{
				msg_send(c->sock, MSG_PONG, 0, NULL, 0);
				break;
			}
		default:
			{
				error_code_t err = ERR_BAD_FRAME;
				msg_send(c->sock, MSG_ERROR, 0, &err, sizeof(err));
				break;
			}
	}
}

/// @brief Initialize Winsock, bind, listen, enter poll loop
void server_run(void)
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		fprintf(stderr, "[server]: WSAStartup failed: %d\n", WSAGetLastError());
		return;
	}

	SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == INVALID_SOCKET)
	{
		fprintf(stderr, "[server]: socket failed: %d\n", WSAGetLastError());
		WSACleanup();
		return;
	}

	int opt = 1;
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) == SOCKET_ERROR)
	{
		fprintf(stderr, "[server]: setsockopt failed: %d\n", WSAGetLastError());
		closesocket(listener);
		WSACleanup();
		return;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
		.sin_addr.s_addr = INADDR_ANY,
	};

	if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		fprintf(stderr, "[server]: bind failed: %d\n", WSAGetLastError());
		closesocket(listener);
		WSACleanup();
		return;
	}

	if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
	{
		fprintf(stderr, "[server]: listen failed: %d\n", WSAGetLastError());
		closesocket(listener);
		WSACleanup();
		return;
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
		clients[i].sock = INVALID_SOCKET;

	pfds[0].fd = listener;
	pfds[0].events = POLLIN;

	srand((unsigned int)time(NULL));
	printf("[server]: listening on port %d\n", SERVER_PORT);

	for (;;)
	{
		if (WSAPoll(pfds, nfds, -1) == SOCKET_ERROR)
		{
			fprintf(stderr, "[server]: WSAPoll failed: %d\n", WSAGetLastError());
			break;
		}

		if (pfds[0].revents & POLLIN)
		{
			SOCKET sock = accept(listener, NULL, NULL);
			if (sock == INVALID_SOCKET)
			{
				fprintf(stderr, "[server]: accept failed: %d\n", WSAGetLastError());
				continue;
			}
			if (client_add(sock) == -1)
			{
				fprintf(stderr, "[server]: max clients reached\n");
				closesocket(sock);
			}
		}

		for (int i = nfds - 1; i >= 1; i--)
		{
			if (pfds[i].revents & POLLIN)
			{
				server_client_t *c = client_by_sock(pfds[i].fd);
				if (!c)
					continue;

				client_handle(c);
			}
		}
	}

	closesocket(listener);
	WSACleanup();
}
