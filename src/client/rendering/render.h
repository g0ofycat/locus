#pragma once

#include "../client.h"

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
void render_message(client_state_t *c, const char *username, const char *message);

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
