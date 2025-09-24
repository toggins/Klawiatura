#pragma once

#include <SDL3/SDL_stdinc.h>

#include "K_game.h"

#include <nutpunch.h>

#define CMD_FLAG(ident) cmd_set_##ident
#define MAKE_FLAG(ident) MAKE_OPTION_PRO(ident, bool, false, true)
#define MAKE_OPTION(ident, default) MAKE_OPTION_PRO(ident, const char*, default, next())
#define MAKE_OPTION_PRO(ident, type, default, set_expr)                                                                \
	static type ident = default;                                                                                   \
	static void CMD_FLAG(ident)(IterArg next) {                                                                    \
		(ident) = (set_expr);                                                                                  \
	}

typedef const char* (*IterArg)();
typedef struct {
	const char *shortform, *longform;
	void (*handler)(IterArg);
} CmdArg;

#define CLIENT_STRING_MAX (NUTPUNCH_FIELD_DATA_MAX + 1)
typedef struct {
	struct {
		char name[CLIENT_STRING_MAX];
		char skin[CLIENT_STRING_MAX];
	} user;

	struct {
		PlayerID players;
		char level[CLIENT_STRING_MAX];
	} game;

	struct {
		char name[CLIENT_STRING_MAX];
		bool public;
	} lobby;
} ClientInfo;

void handle_cmdline(int, char*[]);
