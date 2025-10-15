#include <SDL3/SDL.h>

#include "K_cmd.h"
#include "K_log.h"

extern CmdArg CMDLINE[]; // in K_main.c
static int argc = 0, argi = 1;
static char** argv = NULL;

const char* next_arg() {
	EXPECT(argi < argc, "Expected another command-line argument");
	return argv[argi++];
}

static void handle_cmdline_fr() {
	while (argi < argc) {
		const CmdArg *cmd = CMDLINE, nullcmd = {0};
		for (; SDL_memcmp(cmd, &nullcmd, sizeof(*cmd)); cmd++)
			if ((cmd->shortform != NULL && !SDL_strcmp(argv[argi], cmd->shortform))
				|| (cmd->longform != NULL && !SDL_strcmp(argv[argi], cmd->longform)))
			{
				argi++;
				cmd->handler();
				goto next;
			};
		WARN("Unrecognized command-line option: '%s'", argv[argi++]);
	next:
		continue;
	}
}

void handle_cmdline(int _argc, char* _argv[]) {
	argc = _argc, argv = _argv;
	handle_cmdline_fr(); // separate fn to cull argc & argv out of scope
}
