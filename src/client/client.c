#include <string.h>
#include <stdio.h>

#include "client.h"
#include "./input/input.h"
#include "./rendering/render.h"
#include "../keys/key_exchange.h"

//--============
// -- PRIVATE
//--============

/// @brief Cache message for reply
/// @param c
/// @param id
/// @param username
/// @param text
static void cache_message(client_state_t *c, uint64_t id, const char *username, const char *text)
{
	cached_msg_t *e = &c->msg_cache[c->msg_cache_next % MSG_CACHE_SIZE];
	e->id = id;

	snprintf(e->username, MAX_USERNAME, "%s", username);
	snprintf(e->text, MSG_CACHE_SNIP, "%s", text);

	c->msg_cache_next++;
}

/// @brief Get from cache
/// @param c
/// @param id
/// @return const cached_msg_t
static const cached_msg_t *cache_lookup(client_state_t *c, uint64_t id)
{
	for (int i = 0; i < MSG_CACHE_SIZE; i++) {
		if (c->msg_cache[i].id == id)
			return &c->msg_cache[i];
	}

	return NULL;
}

/// @brief Handle client state
/// @param arg
/// @return DWORD
#ifdef _WIN32
static DWORD WINAPI recv_thread(void *arg)
#else
static void *recv_thread(void *arg)
#endif
{
	client_state_t *c = (client_state_t *)arg;
	uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];
	msg_t *msg = (msg_t *)buf;

	while (c->running)
	{
		if (msg_recv(c->sock, msg, sizeof(buf), c->key) != MSG_OK)
		{
			if (c->running)
				render_system(c, "disconnected from server");
			c->running = 0;
			break;
		}

		switch (msg->type)
		{
			case MSG_CHAT:
				{
					char *username = msg->payload;
					char *message = msg->payload + strlen(msg->payload) + 1;
					cache_message(c, msg->id, username, message);
					render_message(c, username, message, msg->id, NULL, NULL);
					break;
				}
			case MSG_JOIN:
				{
					char notice[MAX_USERNAME + 16];
					snprintf(notice, sizeof(notice), "%s joined", msg->payload);
					render_system(c, notice);
					break;
				}
			case MSG_LEAVE:
				{
					char notice[MAX_USERNAME + 16];
					snprintf(notice, sizeof(notice), "%s left", msg->payload);
					render_system(c, notice);
					break;
				}
			case MSG_RENAME:
				{
					char *old_name = msg->payload;
					char *new_name = msg->payload + strlen(msg->payload) + 1;
					if (strncmp(old_name, c->username, MAX_USERNAME) == 0)
						snprintf(c->username, MAX_USERNAME, "%s", new_name);
					char notice[MAX_USERNAME * 2 + 16];
					snprintf(notice, sizeof(notice), "%s renamed to %s", old_name, new_name);
					render_system(c, notice);
					break;
				}
			case MSG_USER_LIST:
				{
					uint8_t count = (uint8_t)msg->payload[0];
					const char *cursor = msg->payload + 1;
					render_user_list(c, cursor, count);
					break;
				}
			case MSG_REPLY:
				{
					uint64_t reply_id;
					memcpy(&reply_id, msg->payload, sizeof(uint64_t));
					char *username = msg->payload + sizeof(uint64_t);
					char *message  = username + strlen(username) + 1;

					cache_message(c, msg->id, username, message);

					const cached_msg_t *ref = cache_lookup(c, reply_id);
					render_message(c, username, message, msg->id,
							ref ? ref->username : NULL,
							ref ? ref->text : NULL);
					break;
				}
			case MSG_PONG:
				{
					break;
				}
			case MSG_ERROR:
				{
					render_error(c, (uint8_t)msg->payload[0]);
					break;
				}
			default:
				break;
		}
	}

#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}

//--============
// -- PUBLIC
//--============

/// @brief Connect to server, perform MSG_JOIN handshake
/// @param c: Caller-allocated client state
/// @param host: Server IP or hostname
/// @param port: Server port
/// @param username: Desired username
/// @return MSG_OK on success, MSG_ERR_IO on failure
msg_status_t client_connect(client_state_t *c, const char *host, uint16_t port, const char *username)
{
	memset(c, 0, sizeof(client_state_t));
	c->sock = SOCKET_INVALID;
	c->running = 1;

	if (socket_init() != 0)
	{
		fprintf(stderr, "[client]: socket initialization failed: %d\n", socket_last_error());
		return MSG_ERR_IO;
	}

	struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
	struct addrinfo *res;
	if (getaddrinfo(host, NULL, &hints, &res) != 0)
	{
		fprintf(stderr, "[client]: failed to resolve host: %s\n", host);
		socket_cleanup();
		return MSG_ERR_IO;
	}

	struct sockaddr_in addr = *(struct sockaddr_in *)res->ai_addr;
	addr.sin_port = htons(port);
	freeaddrinfo(res);

	c->sock = socket_open(AF_INET, SOCK_STREAM, 0);
	if (c->sock == SOCKET_INVALID)
	{
		fprintf(stderr, "[client]: socket creation failed: %d\n", socket_last_error());
		socket_cleanup();
		return MSG_ERR_IO;
	}

	if (socket_connect(c->sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		fprintf(stderr, "[client]: connect failed: %d\n", socket_last_error());
		socket_close(c->sock);
		socket_cleanup();
		return MSG_ERR_IO;
	}

	if (key_exchange(c->sock, c->key, 0) != 0)
	{
		fprintf(stderr, "[client]: key exchange failed\n");
		socket_close(c->sock);
		socket_cleanup();
		return MSG_ERR_IO;
	}

	uint16_t ulen = (uint16_t)(strlen(username) + 1);
	if (msg_send(c->sock, MSG_JOIN, username, ulen, 0, c->key) != MSG_OK)
	{
		fprintf(stderr, "[client]: MSG_JOIN failed\n");
		socket_close(c->sock);
		socket_cleanup();
		return MSG_ERR_IO;
	}

	uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];
	msg_t *msg = (msg_t *)buf;
	if (msg_recv(c->sock, msg, sizeof(buf), c->key) != MSG_OK)
	{
		fprintf(stderr, "[client]: handshake failed\n");
		socket_close(c->sock);
		socket_cleanup();
		return MSG_ERR_IO;
	}

	if (msg->type == MSG_ERROR)
	{
		fprintf(stderr, "[client]: join rejected: 0x%02X\n", (uint8_t)msg->payload[0]);
		socket_close(c->sock);
		socket_cleanup();
		return MSG_ERR_IO;
	}

	if (msg->type != MSG_WELCOME)
	{
		fprintf(stderr, "[client]: unexpected opcode: 0x%02X\n", msg->type);
		socket_close(c->sock);
		socket_cleanup();
		return MSG_ERR_IO;
	}

	snprintf(c->username, MAX_USERNAME, "%s", username);
	snprintf(c->session_id, MAX_SESSION_ID, "%s", msg->payload);
	return MSG_OK;
}

/// @brief Launch recieved + input threads, block until exit
/// @param c: Connected client state
void client_run(client_state_t *c)
{
#ifdef _WIN32
	c->hin = GetStdHandle(STD_INPUT_HANDLE);
	c->hout = GetStdHandle(STD_OUTPUT_HANDLE);
	c->render_mutex = CreateMutex(NULL, FALSE, NULL);
	if (c->render_mutex == NULL)
	{
		fprintf(stderr, "[client]: failed to create render mutex\n");
		c->running = 0;
		return;
	}
#else
	if (pthread_mutex_init(&c->render_mutex, NULL) != 0)
	{
		fprintf(stderr, "[client]: failed to initialize render mutex\n");
		c->running = 0;
		return;
	}
#endif
	c->render_mutex_initialized = 1;

	render_init(c);
	input_init(c);

#ifdef _WIN32
	HANDLE thread = CreateThread(NULL, 0, recv_thread, c, 0, NULL);
	if (thread == NULL)
	{
		render_system(c, "failed to create recv thread");
		c->running = 0;
		return;
	}

	input_run(c);

	c->running = 0;
	WaitForSingleObject(thread, 2000);
	CloseHandle(thread);
#else
	pthread_t thread;
	if (pthread_create(&thread, NULL, recv_thread, c) != 0)
	{
		render_system(c, "failed to create recv thread");
		c->running = 0;
		render_cleanup(c);
		return;
	}

	input_run(c);
	c->running = 0;
	socket_shutdown(c->sock);
	pthread_join(thread, NULL);
#endif
	render_cleanup(c);
}

/// @brief Send MSG_LEAVE, restore terminal, close socket
/// @param c: Client state
void client_disconnect(client_state_t *c)
{
	if (c->sock != SOCKET_INVALID)
	{
		msg_send(c->sock, MSG_LEAVE, c->username, (uint16_t)(strlen(c->username) + 1), 0, c->key);
		socket_close(c->sock);
		c->sock = SOCKET_INVALID;
	}

	if (c->render_mutex_initialized)
	{
#ifdef _WIN32
		CloseHandle(c->render_mutex);
#else
		pthread_mutex_destroy(&c->render_mutex);
#endif
		c->render_mutex_initialized = 0;
	}

	socket_cleanup();
}
