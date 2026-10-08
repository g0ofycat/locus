#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "server.h"
#include "db/msg_db.hpp"
#include "../keys/key_exchange.h"

//--================
// -- PRIVATE STATE
//--================

static server_client_t clients[MAX_CLIENTS];
static socket_pollfd_t pfds[MAX_CLIENTS + 1]; // + 1 = listener
static int nfds = 1;						// pfds[0] = listener

//--============
// -- PRIVATE
//--============

/// @brief Check if element is in array
/// @param element
/// @param array
/// @param sizeof_array
/// @return 0 on find, -1 else
static int element_in_array(uint8_t element, const uint8_t array[], uint8_t sizeof_array)
{
	for (size_t i = 0; i < sizeof_array; i++) {
		if (element == array[i]) {
			return 0;
		}
	}

	return -1;
}

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
/// @return 0 on find, 1 on taken
static int username_taken(const char *username)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].sock == SOCKET_INVALID)
			continue;

		if (strncmp(clients[i].username, username, MAX_USERNAME) == 0)
			return 1;
	}

	return 0;
}

// @brief Get client based on the socket
/// @param sock
/// @return server_client_t
static server_client_t *client_by_sock(socket_t sock)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].sock == sock)
			return &clients[i];
	}

	return NULL;
}

/// @param Mark client as writable for flushing
/// @param c
static void client_mark_writable(server_client_t *c)
{
	for (int i = 1; i < nfds; i++) {
		if (pfds[i].fd == c->sock) {
			pfds[i].events |= POLLOUT;
			return;
		}
	}
}

/// @brief Flush client ring buffer
/// @param c
/// @return msg_status_t
static msg_status_t client_flush(server_client_t *c)
{
	return msg_flush(c->sock, c->sendbuf, &c->send_head, &c->send_len, SEND_BUF_SIZE);
}

/// @param Check if sending len > 0
/// @param c
/// @return 1 if true, else 0
static int client_has_pending(server_client_t *c)
{
	return c->send_len > 0;
}

/// @brief Queue a message
/// @param c
/// @param type
/// @param payload
/// @param len
/// @param id
/// @return msg_status_t
static msg_status_t client_enqueue(server_client_t *c, uint8_t type, const void *payload, uint16_t len, uint64_t id)
{
	msg_status_t s = msg_enqueue(c->sendbuf, &c->send_head, &c->send_tail, &c->send_len,
			SEND_BUF_SIZE, type, payload, len, id, c->key);

	if (s != MSG_OK)
		return s;

	if (client_flush(c) == MSG_ERR_IO)
		return MSG_ERR_IO;

	if (client_has_pending(c))
		client_mark_writable(c);

	return MSG_OK;
}

/// @brief Render all messages from the current port database
/// @param entry
/// @param user_data
static void client_join_callback(const db_entry* entry, void* user_data) {
	server_client_t *c = user_data;
	client_enqueue(c, entry->type, entry->payload, entry->payload_len, entry->id);
}

//--============
// -- PUBLIC
//--============

/// @brief Add a newly accepted socket to the client list
/// @param sock
/// @return Index on success, -1 if full
int client_add(socket_t sock)
{
	int sndbuf = 256 * 1024;
	socket_set_option(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].sock != SOCKET_INVALID)
			continue;

		clients[i].sock = sock;
		clients[i].joined = 0;
		clients[i].send_head = 0;
		clients[i].send_tail = 0;
		clients[i].send_len = 0;

		memset(clients[i].username, 0, MAX_USERNAME);
		memset(clients[i].session_id, 0, MAX_SESSION_ID);

		if (key_exchange(sock, clients[i].key, 1) != 0) {
			clients[i].sock = SOCKET_INVALID;
			socket_close(sock);
			return -1;
		}

		set_socket_nonblocking(sock);

		pfds[nfds].fd = sock;
		pfds[nfds].events = SOCKET_POLLIN;
		pfds[nfds].revents = 0;
		nfds++;

		return i;
	}

	return -1;
}

/// @brief Remove a client by socket, broadcasts MSG_LEAVE to others
void client_remove(socket_t sock)
{
	server_client_t *c = client_by_sock(sock);
	if (c == NULL)
		return;

	if (c->joined)
	{
		char payload[MAX_USERNAME];
		snprintf(payload, MAX_USERNAME, "%s", c->username);
		broadcast(sock, MSG_LEAVE, payload, (uint16_t)strlen(payload) + 1, 0);
	}

	socket_close(sock);
	memset(c, 0, sizeof(server_client_t));
	c->sock = SOCKET_INVALID;

	for (int i = 1; i < nfds; i++)
	{
		if (pfds[i].fd != sock)
			continue;
		pfds[i] = pfds[nfds - 1];
		nfds--;
		i--;
		break;
	}
}

/// @brief Broadcast a framed message to all joined clients except sender
/// @param sender_sock: Socket to exclude
/// @param type: Protocol Opcode
/// @param payload: Data
/// @param len: Length of payload
/// @param id: Message ID
/// @param sender_sock: Pass SOCKET_INVALID to broadcast to everyone
void broadcast(socket_t sender_sock, uint8_t type, const void *payload, uint16_t len, uint64_t id)
{
	static const uint8_t bypass_opcodes[] = { 0x01, 0x02, 0x04, 0x07, 0x10 };

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].sock == SOCKET_INVALID)
			continue;
		if (clients[i].sock == sender_sock && element_in_array(type, bypass_opcodes, sizeof(bypass_opcodes) / sizeof(bypass_opcodes[0])) != 0)
			continue;
		if (!clients[i].joined)
			continue;

		client_enqueue(&clients[i], type, payload, len, id);
	}
}

/// @brief Read and dispatch one message from a client
/// @param c: Message
void client_handle(server_client_t *c)
{
	uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];
	msg_t *msg = (msg_t *)buf;

	if (msg_recv(c->sock, msg, sizeof(buf), c->key) != MSG_OK)
	{
		client_remove(c->sock);
		return;
	}

	if (!c->joined && msg->type != MSG_JOIN)
	{
		error_code_t err = ERR_NOT_JOINED;
		msg_send(c->sock, MSG_ERROR, &err, sizeof(err), 0, c->key);
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
					msg_send(c->sock, MSG_ERROR, &err, sizeof(err), 0, c->key);
					return;
				}

				snprintf(c->username, MAX_USERNAME, "%s", username);
				session_id_generate(c->session_id);
				c->joined = 1;

				msg_send(c->sock, MSG_WELCOME, c->session_id, (uint16_t)strlen(c->session_id) + 1, 0, c->key);
				broadcast(c->sock, MSG_JOIN, c->username, (uint16_t)strlen(c->username) + 1, 0);
				for_each_message_c(SERVER_PORT, client_join_callback, c);
				break;
			}
		case MSG_CHAT:
			{
				char *msg_text = msg->payload + strlen(msg->payload) + 1;
				int ulen = (int)strlen(c->username) + 1;
				int mlen = (int)strlen(msg_text) + 1;

				char payload[MAX_USERNAME + MAX_PAYLOAD];
				memcpy(payload, c->username, ulen);
				memcpy(payload + ulen, msg_text, mlen);

				uint16_t payload_len = (uint16_t)(ulen + mlen);
				uint64_t msg_id = insert_message_c(SERVER_PORT, payload, payload_len);

				broadcast(c->sock, MSG_CHAT, payload, payload_len, msg_id);
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
					msg_send(c->sock, MSG_ERROR, &err, sizeof(err), 0, c->key);
					return;
				}

				char payload[MAX_USERNAME * 2];
				int olen = (int)strlen(c->username) + 1;
				memcpy(payload, c->username, olen);
				snprintf(payload + olen, MAX_USERNAME, "%s", new_name);

				snprintf(c->username, MAX_USERNAME, "%s", new_name);
				broadcast(c->sock, MSG_RENAME, payload, (uint16_t)(olen + strlen(new_name) + 1), 0);
				break;
			}
		case MSG_USER_LIST_REQ:
			{
				char payload[MAX_CLIENTS * MAX_USERNAME + 1];
				uint8_t count = 0;
				int offset = 1;

				for (int i = 0; i < MAX_CLIENTS; i++)
				{
					if (clients[i].sock == SOCKET_INVALID) continue;
					if (!clients[i].joined) continue;

					int ulen = (int)strlen(clients[i].username) + 1;

					if (offset + ulen >= sizeof(payload))
						break;

					memcpy(payload + offset, clients[i].username, ulen);

					offset += ulen;
					count++;
				}

				payload[0] = (char)count;
				client_enqueue(c, MSG_USER_LIST, payload, (uint16_t)offset, 0);
				break;
			}
		case MSG_REPLY:
			{
				uint64_t reply_id;
				memcpy(&reply_id, msg->payload, sizeof(uint64_t));
				char *msg_text = msg->payload + sizeof(uint64_t);

				int ulen = (int)strlen(c->username) + 1;
				int mlen = (int)strlen(msg_text) + 1;

				char payload[sizeof(uint64_t) + MAX_USERNAME + MAX_PAYLOAD];
				memcpy(payload, &reply_id, sizeof(uint64_t));
				memcpy(payload + sizeof(uint64_t), c->username, ulen);
				memcpy(payload + sizeof(uint64_t) + ulen, msg_text, mlen);

				uint16_t payload_len = (uint16_t)(sizeof(uint64_t) + ulen + mlen);
				uint64_t msg_id = insert_message_ex_c(SERVER_PORT, MSG_REPLY, payload, payload_len);

				broadcast(c->sock, MSG_REPLY, payload, payload_len, msg_id);
				break;
			}
		case MSG_PING:
			{
				client_enqueue(c, MSG_PONG, NULL, 0, 0);
				break;
			}
		default:
			{
				error_code_t err = ERR_BAD_FRAME;
				client_enqueue(c, MSG_ERROR, &err, sizeof(err), 0);
				break;
			}
	}
}

/// @brief Initialize sockets, bind, listen, and enter the poll loop
void server_run(void)
{
	if (socket_init() != 0)
	{
		fprintf(stderr, "[server]: socket initialization failed: %d\n", socket_last_error());
		return;
	}

	socket_t listener = socket_open(AF_INET, SOCK_STREAM, 0);
	if (listener == SOCKET_INVALID)
	{
		fprintf(stderr, "[server]: socket creation failed: %d\n", socket_last_error());
		socket_cleanup();
		return;
	}

	int opt = 1;
	if (socket_set_option(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == SOCKET_ERROR)
	{
		fprintf(stderr, "[server]: setsockopt failed: %d\n", socket_last_error());
		socket_close(listener);
		socket_cleanup();
		return;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
		.sin_addr.s_addr = INADDR_ANY,
	};

	if (socket_bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		fprintf(stderr, "[server]: bind failed: %d\n", socket_last_error());
		socket_close(listener);
		socket_cleanup();
		return;
	}

	if (socket_listen(listener, SOMAXCONN) == SOCKET_ERROR)
	{
		fprintf(stderr, "[server]: listen failed: %d\n", socket_last_error());
		socket_close(listener);
		socket_cleanup();
		return;
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
		clients[i].sock = SOCKET_INVALID;

	pfds[0].fd = listener;
	pfds[0].events = SOCKET_POLLIN;

	srand((unsigned int)time(NULL));
	printf("[server]: listening on port %d\n", SERVER_PORT);

	for (;;)
	{
		if (socket_poll(pfds, nfds, -1) == SOCKET_ERROR)
		{
			fprintf(stderr, "[server]: socket poll failed: %d\n", socket_last_error());
			break;
		}

		if (pfds[0].revents & SOCKET_POLLIN)
		{
			socket_t sock = socket_accept(listener);
			if (sock == SOCKET_INVALID)
			{
				fprintf(stderr, "[server]: accept failed: %d\n", socket_last_error());
				continue;
			}
			if (client_add(sock) == -1)
				fprintf(stderr, "[server]: max clients reached\n");
		}

		for (int i = nfds - 1; i >= 1; --i)
		{
			if (pfds[i].revents & (SOCKET_POLLHUP | SOCKET_POLLERR | SOCKET_POLLNVAL))
			{
				client_remove(pfds[i].fd);
				continue;
			}

			server_client_t *c = client_by_sock(pfds[i].fd);
			if (!c) continue;

			if (pfds[i].revents & SOCKET_POLLIN)
				client_handle(c);

			if (pfds[i].revents & SOCKET_POLLOUT)
			{
				if (client_flush(c) == MSG_ERR_IO)
				{
					client_remove(pfds[i].fd);
					continue;
				}
			}

			pfds[i].events = SOCKET_POLLIN | (client_has_pending(c) ? SOCKET_POLLOUT : 0);
		}
	}

	socket_close(listener);
	socket_cleanup();
}
