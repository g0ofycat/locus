#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "input.h"
#include "../rendering/render.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

//--============
// -- PRIVATE
//--============

/// @brief Return trimmed arg
/// @param line
/// @param prefix
/// @return const char
static const char *skip_prefix(const char *line, const char *prefix)
{
	size_t len = strlen(prefix);
	if (strncmp(line, prefix, len) != 0) return NULL;
	const char *arg = line + len;
	while (*arg == ' ') arg++;
	return arg;
}

/// @brief Command Type Dispatch
/// @param c
/// @param cmd
static void dispatch(client_state_t *c, cmd_t *cmd)
{
	switch (cmd->type)
	{
		case CMD_TYPE_CHAT:
			{
				char payload[MAX_USERNAME + INPUT_BUF_SIZE + 2]; // + 2 worst case w/ max buffer sizes

				int ulen = (int)strlen(c->username) + 1;
				int mlen = c->input_len + 1;

				memcpy(payload, c->username, ulen);
				memcpy(payload + ulen, c->input_buf, mlen);

				msg_send(c->sock, MSG_CHAT, payload, (uint16_t)(ulen + mlen), 0, c->key);
				break;
			}
		case CMD_TYPE_RENAME:
			{
				if (cmd->arg[0] == '\0')
				{
					render_system(c, "usage: /rename <new_name>");
					return;
				}

				char payload[MAX_USERNAME * 2];
				int olen = (int)strlen(c->username) + 1;
				memcpy(payload, c->username, olen);

				int nlen = (int)strlen(cmd->arg) + 1;
				memcpy(payload + olen, cmd->arg, nlen);

				uint16_t len = (uint16_t)(olen + nlen);

				msg_send(c->sock, MSG_RENAME, payload, len, 0, c->key);
				break;
			}
		case CMD_TYPE_USERS:
			{
				msg_send(c->sock, MSG_USER_LIST_REQ, NULL, 0, 0, c->key);
				break;
			}
		case CMD_TYPE_REPLY:
			{
				if (cmd->msg[0] == '\0') {
					render_system(c, "usage: /reply <msg_id> <reply_msg>");
					return;
				}

				char payload[sizeof(uint64_t) + INPUT_BUF_SIZE + 1];
				memcpy(payload, &cmd->reply_id, sizeof(uint64_t));
				int mlen = (int)strlen(cmd->msg) + 1;
				memcpy(payload + sizeof(uint64_t), cmd->msg, mlen);
				msg_send(c->sock, MSG_REPLY, payload, (uint16_t)(sizeof(uint64_t) + mlen), 0, c->key);
				break;
			}
		case CMD_TYPE_QUIT:
			{
				c->running = 0;
				break;
			}
		case CMD_TYPE_UNKNOWN:
			{
				render_system(c, "unknown command");
				break;
			}
	}
}

//--============
// -- PUBLIC
//--============

/// @brief Initialize input buffer state
/// @param c: Client state
void input_init(client_state_t *c)
{
	memset(c->input_buf, 0, INPUT_BUF_SIZE);
	c->input_len = 0;

#ifdef _WIN32
	GetConsoleMode(c->hin, &c->original_mode);
	SetConsoleMode(c->hin, c->original_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
#else
	if (tcgetattr(STDIN_FILENO, &c->original_mode) == 0) {
		struct termios raw = c->original_mode;
		raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
		if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
			c->terminal_mode_initialized = 1;
	}
#endif
}

/// @brief Parse a completed input line into a cmd_t
/// @param line: Null-terminated input string
/// @param out: Caller-allocated cmd_t to populate
void input_parse(const char *line, cmd_t *out)
{
	memset(out, 0, sizeof(cmd_t));

	if (line[0] != '/') {
		out->type = CMD_TYPE_CHAT;
		return;
	}

	if (strcmp(line, CMD_USERS) == 0) { out->type = CMD_TYPE_USERS; return; }
	if (strcmp(line, CMD_QUIT)  == 0) { out->type = CMD_TYPE_QUIT; return; }

	const char *arg;

	if ((arg = skip_prefix(line, CMD_RENAME))) {
		out->type = CMD_TYPE_RENAME;
		snprintf(out->arg, MAX_USERNAME, "%s", arg);
		return;
	}

	if ((arg = skip_prefix(line, CMD_REPLY))) {
		out->type = CMD_TYPE_REPLY;
		char *end;
		out->reply_id = strtoull(arg, &end, 10);
		while (*end == ' ') end++;
		snprintf(out->msg, INPUT_BUF_SIZE, "%s", end);
		return;
	}

	out->type = CMD_TYPE_UNKNOWN;
}

/// @brief Append a character to the input buffer and redraw
/// @param c:  Client state
/// @param ch: Character to append
void input_push(client_state_t *c, char ch)
{
	if (c->input_len >= INPUT_BUF_SIZE - 1)
		return;

	c->input_buf[c->input_len++] = ch;
	c->input_buf[c->input_len] = '\0';

	render_input(c);
}

/// @brief Delete last character from input buffer and redraw
/// @param c: Client state
void input_pop(client_state_t *c)
{
	if (c->input_len == 0)
		return;
	c->input_buf[--c->input_len] = '\0';

	render_input(c);
}

/// @brief Clear the input buffer and redraw
/// @param c: Client state
void input_clear(client_state_t *c)
{
	memset(c->input_buf, 0, INPUT_BUF_SIZE);
	c->input_len = 0;

	render_input(c);
}

/// @brief Block and process keypresses until c->running == 0
/// @param c: Client state
void input_run(client_state_t *c)
{
#ifdef _WIN32
	INPUT_RECORD rec;
	DWORD read;

	while (c->running)
	{
		if (!ReadConsoleInput(c->hin, &rec, 1, &read))
			break;
		if (rec.EventType != KEY_EVENT)
			continue;
		if (!rec.Event.KeyEvent.bKeyDown)
			continue;

		WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
		char ch = rec.Event.KeyEvent.uChar.AsciiChar;

		if (vk == VK_RETURN)
		{
			if (c->input_len == 0)
				continue;

			cmd_t cmd;
			input_parse(c->input_buf, &cmd);

			dispatch(c, &cmd);

			input_clear(c);
		}
		else if (vk == VK_BACK)
		{
			input_pop(c);
		}
		else if (vk == VK_ESCAPE)
		{
			c->running = 0;
		}
		else if (ch >= 32 && ch < 127)
		{
			input_push(c, ch);
		}
	}
#else
	while (c->running)
	{
		char ch;
		ssize_t count = read(STDIN_FILENO, &ch, 1);
		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0)
			break;

		if (ch == '\r' || ch == '\n')
		{
			if (c->input_len == 0)
				continue;

			cmd_t cmd;
			input_parse(c->input_buf, &cmd);
			dispatch(c, &cmd);
			input_clear(c);
		}
		else if (ch == 8 || ch == 127)
		{
			input_pop(c);
		}
		else if (ch == 3 || ch == 27)
		{
			c->running = 0;
		}
		else if ((unsigned char)ch >= 32 && (unsigned char)ch < 127)
		{
			input_push(c, ch);
		}
	}
#endif
}
