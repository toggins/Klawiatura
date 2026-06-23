#include "K_cmd.h"
#include "K_log.h"

extern CmdArg CMDLINE[]; // in K_main.c
static int argc = 0, argi = 1;
static char** argv = NULL;

const char* next_arg() {
    EXPECT(argi < argc, "Expected another command-line argument, last argument was \"%s\"",
        (argc > 0) ? argv[argc - 1] : NULL);
    return argv[argi++];
}

static void handle_cmdline_fr() {
    while (argi < argc) {
        const CmdArg *cmd = CMDLINE, nullcmd = {0};
        for (; SDL_memcmp(cmd, &nullcmd, sizeof(*cmd)) != 0; cmd++) {
            if ((cmd->shortform != NULL && SDL_strcmp(argv[argi], cmd->shortform) == 0)
                || (cmd->longform != NULL && SDL_strcmp(argv[argi], cmd->longform) == 0))
            {
                argi++;
                cmd->handler();
                goto hclfr_next;
            }
        }
        WARN("Unrecognized command-line option: '%s'", argv[argi++]);

    hclfr_next:
        continue;
    }
}

void handle_cmdline(int _argc, char* _argv[]) {
    argc = _argc, argv = _argv;
    handle_cmdline_fr(); // separate fn to cull argc & argv out of scope
}
