#pragma once

#include "../client.h"

//--============
// -- CONST
//--============

#define REPLY_GREY "\x1b[38;5;242m"

#define DARK_GREY "\033[90m"
#define RESET "\033[0m"

//--============
// -- PUBLIC
//--============

/// @brief Enter raw terminal mode, draw initial UI chrome
/// @param c: Client state
void render_init(client_state_t *c);

/// @brief Restore original terminal mode
/// @param c: Client state
void render_cleanup(client_state_t *c);

/// @brief Redraw the input line at the bottom of the terminal
/// @param c: Client state
void render_input(client_state_t *c);

/// @brief Erase input line, print formatted chat message, redraw input line
/// @param c: Client state
/// @param username: Sender username
/// @param message: Message content
/// @param id: Message ID from database
/// @param reply_username: Username of reply
/// @param reply_text: Username of text
void render_message(client_state_t *c, const char *username, const char *message, uint64_t id, const char *reply_username, const char *reply_text);

/// @brief Erase input line, print system notice, redraw input line
/// @param c: Client state
/// @param text: Notice text (join, leave, rename)
void render_system(client_state_t *c, const char *text);

/// @brief Erase input line, print user list, redraw input line
/// @param c: Client state
/// @param users: Pointer to packed username\0username\0... buffer
/// @param count: Number of usernames in buffer
void render_user_list(client_state_t *c, const char *users, uint8_t count);

/// @brief Erase input line, print error, redraw input line
/// @param c: Client state
/// @param code: error_code_t value
void render_error(client_state_t *c, uint8_t code);
