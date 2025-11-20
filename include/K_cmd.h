#pragma once

#include <SDL3/SDL_stdinc.h>

#include "K_game.h"

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

// !!! !!! !!!
// !!! !!! !!!
// !!! !!! !!!

/// CLIENT-SIDE FLAG FOR CONTEXT. DO NOT USE IN STATE!
#define GF_TRY_KEVIN (CLIENT.game.kevin * GF_KEVIN)

/// CLIENT-SIDE FLAG FOR CONTEXT. DO NOT USE IN STATE!
#define GF_TRY_FRED (CLIENT.game.fred * GF_FRED)

/// CLIENT-SIDE FLAG FOR CONTEXT. DO NOT USE IN STATE!
#define GF_TRY_HELL (GF_TRY_KEVIN | GF_TRY_FRED)

// !!! !!! !!!
// !!! !!! !!!
// !!! !!! !!!

typedef struct {
	struct {
		bool aware;
		char name[CLIENT_STRING_MAX];
		char skin[CLIENT_STRING_MAX];
	} user;

	struct {
		uint8_t delay;
	} input;

	struct {
		bool filter;
	} video;

	struct {
		bool background;
	} audio;

	struct {
		PlayerID players;
		bool kevin, fred;
		char level[CLIENT_STRING_MAX];
	} game;

	struct {
		char name[NUTPUNCH_ID_MAX];
		bool public;
	} lobby;
} ClientInfo;
extern ClientInfo CLIENT;

void handle_cmdline(int, char*[]);
