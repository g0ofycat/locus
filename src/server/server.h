#pragma once

#include "../utils/socket_compact.h"
#include "../protocol/protocol.h"
#include "../io/msg_io.h"

//--============
// -- CONSTS
//--============

#define SESSION_ID_LEN 20

#define MAX_CLIENTS 256
#define SERVER_PORT 6969

#define SEND_BUF_SIZE 65536

//--============
// -- STRUCTS
//--============

typedef struct {
	uint8_t sendbuf[SEND_BUF_SIZE]; // ring buf so socket non-blocking
	uint8_t key[32];                // encryption
	char session_id[MAX_SESSION_ID];
	char username[MAX_USERNAME];
	socket_t sock;
	int joined;	// (0, 1)
	int send_head;
	int send_tail;
	int send_len;
} server_client_t;

//--============
// -- PUBLIC
//--============

/// @brief Add a newly accepted socket to the client list
/// @param sock
/// @return Index on success, -1 if full
int client_add(socket_t sock);

/// @brief Remove a client by socket, broadcasts MSG_LEAVE to others
/// @param sock
void client_remove(socket_t sock);

/// @brief Broadcast a framed message to all joined clients except sender
/// @param sender_sock: Socket to exclude
/// @param type: Protocol Opcode
/// @param payload: Data
/// @param len: Length of payload
/// @param id: Message ID
/// @param sender_sock: Pass SOCKET_INVALID to broadcast to everyone
void broadcast(socket_t sender_sock, uint8_t type, const void *payload, uint16_t len, uint64_t id);

/// @brief Read and dispatch one message from a client
/// @param c: Message
void client_handle(server_client_t *c);

/// @brief Initialize sockets, bind, listen, and enter poll loop
void server_run(void);
