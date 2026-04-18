#pragma once

#include <winsock2.h>
#include <windows.h>

#include "../protocol/protocol.h"
#include "../io/msg_io.h"

//--============
// -- CONSTS
//--============

#define MSG_CACHE_SIZE 512
#define MSG_CACHE_SNIP 128

#define INPUT_BUF_SIZE 512
#define SERVER_DEFAULT_PORT 6969

//--============
// -- STRUCTS
//--============

typedef struct {
	uint64_t id;
	char username[MAX_USERNAME];
	char text[MSG_CACHE_SNIP];
} cached_msg_t;  // for 0x07

typedef struct {
	cached_msg_t msg_cache[MSG_CACHE_SIZE];
	uint8_t key[32];                // encryption
	char input_buf[INPUT_BUF_SIZE]; // current line being typed
	char session_id[MAX_SESSION_ID];
	char username[MAX_USERNAME];
	HANDLE hin;                     // STD_INPUT_HANDLE
	HANDLE hout;                    // STD_OUTPUT_HANDLE
	HANDLE render_mutex;            // for rendering chats
	SOCKET sock;
	int input_len;                  // cursor position / length
	int msg_cache_next;             // next msg for reply
	DWORD original_mode;            // restored on exit
	volatile int running;           // 0 = threads should exit
} client_state_t;

//--============
// -- PUBLIC
//--============

/// @brief Connect to server, perform MSG_JOIN handshake
/// @param c: Caller-allocated client state
/// @param host: Server IP or hostname
/// @param port: Server port
/// @param username: Desired username
/// @return MSG_OK on success, MSG_ERR_IO on failure
msg_status_t client_connect(client_state_t *c, const char *host, uint16_t port, const char *username);

/// @brief Launch recieved + input threads, block until exit
/// @param c: Connected client state
void client_run(client_state_t *c);

/// @brief Send MSG_LEAVE, restore terminal, close socket
/// @param c: Client state
void client_disconnect(client_state_t *c);
