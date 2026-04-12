#include "render.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

//--================
// -- PRIVATE
//--================

/// @brief Get current time as [HH:MM] string
/// @param out
/// @param len
static void time_str(char *out, size_t len) {
	time_t t = time(NULL);
	struct tm tm_local;

	if (localtime_s(&tm_local, &t) == 0) {
		strftime(out, len, "[%H:%M]", &tm_local);
	} else {
		if (len > 0) out[0] = '\0';
	}
}

/// @brief Move cursor to beginning of current line and erase it
/// @param c
static void erase_input_line(client_state_t *c) {
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(c->hout, &info);

	COORD pos = {
		.X = 0,
		.Y = info.dwCursorPosition.Y,
	};
	SetConsoleCursorPosition(c->hout, pos);

	DWORD written;
	FillConsoleOutputCharacter(c->hout, ' ', info.dwSize.X, pos, &written);
	SetConsoleCursorPosition(c->hout, pos);
}

/// @brief Print a line then move to next line
/// @param c
/// @param line
static void print_line(client_state_t *c, const char *line) {
	DWORD written;
	WriteConsole(c->hout, line, (DWORD)strlen(line), &written, NULL);
	WriteConsole(c->hout, "\r\n", 2, &written, NULL);
}

/// @brief Redraw input line without acquiring mutex (caller must hold)
/// @param c
static void render_input_unlocked(client_state_t *c) {
	erase_input_line(c);

	char line[INPUT_BUF_SIZE + 4];
	snprintf(line, sizeof(line), "> %s", c->input_buf);

	DWORD written;
	WriteConsole(c->hout, line, (DWORD)strlen(line), &written, NULL);
}

//--================
// -- API
//--================

/// @brief Enter raw terminal mode, draw initial UI chrome
/// @param c: Client state
void render_init(client_state_t *c) {
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

	char sep[256];
	int width = info.dwSize.X < 255 ? info.dwSize.X : 255;
	memset(sep, '-', width);
	sep[width] = '\0';
	print_line(c, sep);
	render_input(c);
}

/// @brief Restore original terminal mode
/// @param c: Client state
void render_cleanup(client_state_t *c) {
	SetConsoleMode(c->hin, c->original_mode);
}

/// @brief Redraw the input line at the bottom of the terminal
/// @param c: Client state
void render_input(client_state_t *c) {
	WaitForSingleObject(c->render_mutex, INFINITE);
	render_input_unlocked(c);
	ReleaseMutex(c->render_mutex);
}

/// @brief Erase input line, print formatted chat message, redraw input line
/// @param c: Client state
/// @param username: Sender username
/// @param message: Message content
void render_message(client_state_t *c, const char *username, const char *message) {
	WaitForSingleObject(c->render_mutex, INFINITE);

	erase_input_line(c);

	char ts[16];
	char line[MAX_USERNAME + MAX_PAYLOAD + 32];

	time_str(ts, sizeof(ts));
	snprintf(line, sizeof(line), "%s %s: %s", ts, username, message);
	print_line(c, line);

	render_input_unlocked(c);

	ReleaseMutex(c->render_mutex);
}

/// @brief Erase input line, print system notice, redraw input line
/// @param c: Client state
/// @param text: Notice text (join, leave, rename)
void render_system(client_state_t *c, const char *text) {
	WaitForSingleObject(c->render_mutex, INFINITE);

	erase_input_line(c);

	char ts[16];
	char line[MAX_PAYLOAD + 32];

	time_str(ts, sizeof(ts));
	snprintf(line, sizeof(line), "%s * %s", ts, text);
	print_line(c, line);

	render_input_unlocked(c);

	ReleaseMutex(c->render_mutex);
}

/// @brief Erase input line, print user list, redraw input line
/// @param c: Client state
/// @param users: Pointer to packed username\0username\0... buffer
/// @param count: Number of usernames in buffer
void render_user_list(client_state_t *c, const char *users, uint8_t count) {
	WaitForSingleObject(c->render_mutex, INFINITE);

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

	ReleaseMutex(c->render_mutex);
}

/// @brief Erase input line, print error, redraw input line
/// @param c: Client state
/// @param code: error_code_t value
void render_error(client_state_t *c, uint8_t code) {
	WaitForSingleObject(c->render_mutex, INFINITE);

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

	ReleaseMutex(c->render_mutex);
}
