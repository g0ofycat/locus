#pragma once

#include <winsock2.h>

#include "../protocol/protocol.h"
#include "../io/msg_io.h"

//--============
// -- CONSTS
//--============

#define SESSION_ID_LEN 20

#define MAX_CLIENTS 256
#define SERVER_PORT 6969

//--============
// -- STRUCTS
//--============

typedef struct {
	SOCKET  sock;
	char    username[MAX_USERNAME];
	char    session_id[MAX_SESSION_ID];
	int     joined;	// (0, 1)
} client_t;

//--============
// -- API
//--============

/// @brief Add a newly accepted socket to the client list
/// @param sock
/// @return Index on success, -1 if full
int client_add(SOCKET sock);

/// @brief Remove a client by socket, broadcasts MSG_LEAVE to others
/// @param sock
void client_remove(SOCKET sock);

/// @brief Broadcast a framed message to all joined clients except sender
/// @param sender_sock: Socket to exclude
/// @param type: Protocol Opcode
/// @param payload: Data
/// @param len: Length of payload
/// @param sender_sock: Pass INVALID_SOCKET to broadcast to everyone
void broadcast(SOCKET sender_sock, uint8_t type, const void *payload, uint16_t len);

/// @brief Read and dispatch one message from a client
/// @param c: Message
void client_handle(client_t *c);

/// @brief Initialize Winsock, bind, listen, enter poll loop
void server_run(void);
