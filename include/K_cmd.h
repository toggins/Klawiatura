#pragma once

#include <SDL3/SDL_stdinc.h>

#include <nutpunch.h>

#define CMD_OPT(ident) cmd_set_##ident
#define MAKE_FLAG(ident) MAKE_OPTION_PRO(ident, bool, false, true)
#define MAKE_OPTION(ident, default) MAKE_OPTION_PRO(ident, const char*, default, next_arg())
#define MAKE_OPTION_PRO(ident, type, default, set_expr)                                                                \
	static type ident = default;                                                                                   \
	static void CMD_OPT(ident)() {                                                                                 \
		(ident) = (set_expr);                                                                                  \
	}

const char* next_arg();
typedef struct {
	const char *shortform, *longform;
	void (*handler)();
} CmdArg;

#define CLIENT_STRING_MAX (NUTPUNCH_FIELD_DATA_MAX)
typedef struct {
	struct {
		char name[CLIENT_STRING_MAX];
		char skin[CLIENT_STRING_MAX];
	} user;

	struct {
		uint8_t delay;
	} input;

	struct {
		int8_t players; // Supposed to be PlayerID, but including `K_game.h` causes errors.
		bool kevin;
		char level[CLIENT_STRING_MAX];
	} game;

	struct {
		char name[CLIENT_STRING_MAX];
		bool public;
	} lobby;
} ClientInfo;
extern ClientInfo CLIENT;

void handle_cmdline(int, char*[]);
