#pragma once

#include "../client.h"

//--============
// -- CONSTS
//--============

#define CMD_RENAME "/rename"
#define CMD_USERS  "/users"
#define CMD_QUIT   "/quit"

//--============
// -- STRUCTS
//--============

typedef enum {
	CMD_TYPE_CHAT,    // MSG_CHAT
	CMD_TYPE_RENAME,  // /rename <new>
	CMD_TYPE_USERS,   // /users
	CMD_TYPE_QUIT,    // /quit
	CMD_TYPE_UNKNOWN
} cmd_type_t;

typedef struct {
	cmd_type_t type;
	char arg[MAX_USERNAME]; // CMD_TYPE_RENAME
} cmd_t;

//--============
// -- API
//--============

/// @brief Initialize input buffer state
/// @param c: Client state
void input_init(client_state_t *c);

/// @brief Parse a completed input line into a cmd_t
/// @param line: Null-terminated input string
/// @param out: Caller-allocated cmd_t to populate
void input_parse(const char *line, cmd_t *out);

/// @brief Append a character to the input buffer and redraw
/// @param c:  Client state
/// @param ch: Character to append
void input_push(client_state_t *c, char ch);

/// @brief Delete last character from input buffer and redraw
/// @param c: Client state
void input_pop(client_state_t *c);

/// @brief Clear the input buffer and redraw
/// @param c: Client state
void input_clear(client_state_t *c);

/// @brief Block and process keypresses until c->running == 0
/// @param c: Client state
void input_run(client_state_t *c);
