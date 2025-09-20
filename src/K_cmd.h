#pragma once

#include <SDL3/SDL_stdinc.h>

#define CMD_SET_FLAG(ident) cmd_set_##ident
#define MAKE_FLAG(ident)                                                                                               \
	static bool ident = false;                                                                                     \
	static void CMD_SET_FLAG(ident)(IterArg _) {                                                                   \
		(ident) = true;                                                                                        \
	}

typedef const char*(IterArg)();
typedef struct {
	const char *shortform, *longform;
	void (*handler)(IterArg);
} CmdArg;

extern CmdArg CMDLINE[];
void handle_cmdline(int, char*[]);
