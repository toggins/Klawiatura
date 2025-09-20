#include <SDL3/SDL.h>

#include "K_cmd.h"

static int g_argc, g_argi = 1;
static char** g_argv;

static const char* g_iter_args() {
	return ++g_argi < g_argc ? g_argv[g_argi] : NULL;
}

static void handle_cmdline_fr() {
	while (g_argi < g_argc) {
		const CmdArg *cmd = CMDLINE, nullcmd = {0};
		for (; SDL_memcmp(cmd, &nullcmd, sizeof(*cmd)); cmd++) {
			if ((cmd->shortform != NULL && !SDL_strcmp(g_argv[g_argi], cmd->shortform))
				|| (cmd->longform != NULL && !SDL_strcmp(g_argv[g_argi], cmd->longform)))
			{
				g_argi++;
				cmd->handler(g_iter_args);
				goto next;
			};
		}
		g_argi++;
	next:
		continue;
	}
}

void handle_cmdline(int argc, char* argv[]) {
	g_argc = argc;
	g_argv = argv;
	handle_cmdline_fr(); // separate fn to cull argc & argv out of scope
}
