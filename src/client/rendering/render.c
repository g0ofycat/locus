#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "render.h"

#include "../../parser/message_parser.hpp"

//--============
// -- PRIVATE
//--============

static void render_lock(client_state_t *c) {
#ifdef _WIN32
	WaitForSingleObject(c->render_mutex, INFINITE);
#else
	pthread_mutex_lock(&c->render_mutex);
#endif
}

static void render_unlock(client_state_t *c) {
#ifdef _WIN32
	ReleaseMutex(c->render_mutex);
#else
	pthread_mutex_unlock(&c->render_mutex);
#endif
}

static void console_write(client_state_t *c, const char *text, size_t len) {
#ifdef _WIN32
	DWORD written;
	WriteConsole(c->hout, text, (DWORD)len, &written, NULL);
#else
	(void)c;
	fwrite(text, 1, len, stdout);
	fflush(stdout);
#endif
}

static int terminal_width(client_state_t *c) {
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO info;
	if (GetConsoleScreenBufferInfo(c->hout, &info) && info.dwSize.X > 0)
		return info.dwSize.X;
#else
	struct winsize size;
	(void)c;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0)
		return size.ws_col;
#endif
	return 80;
}

/// @brief Get current time as [HH:MM] string
/// @param out
/// @param len
static void time_str(char *out, size_t len) {
	time_t t = time(NULL);
	struct tm tm_local;

#ifdef _WIN32
	if (localtime_s(&tm_local, &t) == 0) {
		strftime(out, len, "[%H:%M]", &tm_local);
	} else {
		if (len > 0) out[0] = '\0';
	}
#else
	if (localtime_r(&t, &tm_local)) {
		strftime(out, len, "[%H:%M]", &tm_local);
	} else {
		if (len > 0) out[0] = '\0';
	}
#endif
}

/// @brief Move cursor to beginning of current line and erase it
/// @param c
static void erase_input_line(client_state_t *c) {
	int width = terminal_width(c);
	size_t current_len = strlen(c->input_buf) + 2;
	size_t max_len = (c->last_input_len > current_len) ? c->last_input_len : current_len;

	int rows = (int)((max_len + width - 1) / width);

	char cmd[64];
	if (rows > 1) {
		snprintf(cmd, sizeof(cmd), "\r\x1b[%dA\x1b[J", rows - 1);
	} else {
		snprintf(cmd, sizeof(cmd), "\r\x1b[J");
	}

	console_write(c, cmd, strlen(cmd));

	c->last_input_len = current_len;
}

/// @brief Print a line then move to next line
/// @param c
/// @param line
static void print_line(client_state_t *c, const char *line) {
	console_write(c, line, strlen(line));
	console_write(c, "\r\n", 2);
}

/// @brief Redraw input line without acquiring mutex (caller must hold)
/// @param c
static void render_input_unlocked(client_state_t *c) {
	erase_input_line(c);

	char line[INPUT_BUF_SIZE + 4];
	snprintf(line, sizeof(line), "> %s", c->input_buf);

	console_write(c, line, strlen(line));
}

//--============
// -- PUBLIC
//--============

/// @brief Enter raw terminal mode, draw initial UI chrome
/// @param c: Client state
void render_init(client_state_t *c) {
#ifdef _WIN32
	GetConsoleMode(c->hin, &c->original_mode);

	DWORD out_mode;
	GetConsoleMode(c->hout, &out_mode);
	SetConsoleMode(c->hout, out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	COORD origin = {0, 0};
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(c->hout, &info);
	DWORD written;
	FillConsoleOutputCharacter(c->hout, ' ', info.dwSize.X * info.dwSize.Y, origin, &written);
	SetConsoleCursorPosition(c->hout, origin);
#else
	fputs("\x1b[2J\x1b[H", stdout);
	fflush(stdout);
#endif

	char sep[256];
	int width = terminal_width(c);
	if (width > 255) width = 255;
	memset(sep, '-', width);
	sep[width] = '\0';
	print_line(c, sep);
	render_input(c);
}

/// @brief Restore original terminal mode
/// @param c: Client state
void render_cleanup(client_state_t *c) {
#ifdef _WIN32
	SetConsoleMode(c->hin, c->original_mode);
#else
	if (c->terminal_mode_initialized)
		tcsetattr(STDIN_FILENO, TCSANOW, &c->original_mode);
#endif
}

/// @brief Redraw the input line at the bottom of the terminal
/// @param c: Client state
void render_input(client_state_t *c) {
	render_lock(c);
	render_input_unlocked(c);
	render_unlock(c);
}

/// @brief Erase input line, print formatted chat message, redraw input line
/// @param c: Client state
/// @param username: Sender username
/// @param message: Message content
/// @param id: Message ID from database
/// @param reply_username: Username of reply
/// @param reply_text: Username of text
void render_message(client_state_t *c, const char *username, const char *message, uint64_t id, const char *reply_username, const char *reply_text)
{
	render_lock(c);
	erase_input_line(c);

	if (reply_username && reply_text) {
		char quote[MAX_USERNAME + MSG_CACHE_SNIP + 16];
		snprintf(quote, sizeof(quote), REPLY_GREY "> \"%s: %s\"" RESET, reply_username, reply_text);
		print_line(c, quote);
	}

	char *parsed_msg = parse_message_c(message);
	char ts[16], line[MAX_USERNAME + MAX_PAYLOAD + 128];
	time_str(ts, sizeof(ts));
	snprintf(line, sizeof(line), "%s %s: %s" DARK_GREY " { id: %" PRIu64 " }" RESET,
			ts, username, parsed_msg, id);
	print_line(c, line);

	render_input_unlocked(c);
	free(parsed_msg);
	render_unlock(c);
}

/// @brief Erase input line, print system notice, redraw input line
/// @param c: Client state
/// @param text: Notice text (join, leave, rename)
void render_system(client_state_t *c, const char *text) {
	render_lock(c);

	erase_input_line(c);

	char ts[16];
	char line[MAX_PAYLOAD + 32];

	time_str(ts, sizeof(ts));
	snprintf(line, sizeof(line), "%s * %s", ts, text);
	print_line(c, line);

	render_input_unlocked(c);

	render_unlock(c);
}

/// @brief Erase input line, print user list, redraw input line
/// @param c: Client state
/// @param users: Pointer to packed username\0username\0... buffer
/// @param count: Number of usernames in buffer
void render_user_list(client_state_t *c, const char *users, uint8_t count) {
	render_lock(c);

	erase_input_line(c);

	char ts[16];
	time_str(ts, sizeof(ts));

	char header[64];
	snprintf(header, sizeof(header), "%s * online (%d):", ts, count);
	print_line(c, header);

	const char *cursor = users;
	for (uint8_t i = 0; i < count; i++) {
		char line[MAX_USERNAME + 8];
		snprintf(line, sizeof(line), "    %s", cursor);
		print_line(c, line);
		cursor += strlen(cursor) + 1;
	}

	render_input_unlocked(c);

	render_unlock(c);
}

/// @brief Erase input line, print error, redraw input line
/// @param c: Client state
/// @param code: error_code_t value
void render_error(client_state_t *c, uint8_t code) {
	render_lock(c);

	erase_input_line(c);

	const char *desc;
	switch ((error_code_t)code) {
		case ERR_USERNAME_TAKEN: desc = "username already taken"; break;
		case ERR_USERNAME_INVALID: desc = "username invalid"; break;
		case ERR_BAD_FRAME: desc = "bad frame"; break;
		case ERR_NOT_JOINED: desc = "not joined"; break;
		default: desc = "unknown error"; break;
	}

	char ts[16];
	char line[64];

	time_str(ts, sizeof(ts));
	snprintf(line, sizeof(line), "%s ! error: %s (0x%02X)", ts, desc, code);
	print_line(c, line);

	render_input_unlocked(c);

	render_unlock(c);
}
