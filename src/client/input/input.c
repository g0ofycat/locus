#include <string.h>
#include <stdio.h>

#include "input.h"
#include "../rendering/render.h"

//--================
// -- PRIVATE
//--================

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

				msg_send(c->sock, MSG_CHAT, 0, payload, (uint16_t)(ulen + mlen));
				break;
			}
		case CMD_TYPE_RENAME:
			{
				if (cmd->arg[0] == '\0')
				{
					render_system(c, "usage: /rename <new username>");
					return;
				}

				char payload[MAX_USERNAME * 2];
				int olen = (int)strlen(c->username) + 1;
				memcpy(payload, c->username, olen);

				int nlen = (int)strlen(cmd->arg) + 1;
				memcpy(payload + olen, cmd->arg, nlen);

				uint16_t len = (uint16_t)(olen + nlen);

				msg_send(c->sock, MSG_RENAME, 0, payload, len);
				break;
			}
		case CMD_TYPE_USERS:
			{
				msg_send(c->sock, MSG_USER_LIST_REQ, 0, NULL, 0);
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

//--================
// -- API
//--================

/// @brief Initialize input buffer state
/// @param c: Client state
void input_init(client_state_t *c)
{
	memset(c->input_buf, 0, INPUT_BUF_SIZE);
	c->input_len = 0;

	GetConsoleMode(c->hin, &c->original_mode);
	SetConsoleMode(c->hin, c->original_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
}

/// @brief Parse a completed input line into a cmd_t
/// @param line: Null-terminated input string
/// @param out: Caller-allocated cmd_t to populate
void input_parse(const char *line, cmd_t *out)
{
	memset(out, 0, sizeof(cmd_t));

	if (line[0] != '/')
	{
		out->type = CMD_TYPE_CHAT;
		return;
	}

	if (strncmp(line, CMD_RENAME, strlen(CMD_RENAME)) == 0)
	{
		out->type = CMD_TYPE_RENAME;
		const char *arg = line + strlen(CMD_RENAME);

		while (*arg == ' ')
			arg++;

		snprintf(out->arg, MAX_USERNAME, "%s", arg);
		return;
	}

	if (strcmp(line, CMD_USERS) == 0)
	{
		out->type = CMD_TYPE_USERS;
		return;
	}

	if (strcmp(line, CMD_QUIT) == 0)
	{
		out->type = CMD_TYPE_QUIT;
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
}
