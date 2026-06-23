#pragma once

#define CMD_OPT(ident) cmd_set_##ident
#define MAKE_FLAG(ident) MAKE_OPTION_PRO(ident, Bool, FALSE, TRUE)
#define MAKE_OPTION(ident, default) MAKE_OPTION_PRO(ident, const char*, default, next_arg())
#define MAKE_OPTION_PRO(ident, type, default, set_expr)                                                                \
    static type ident = default;                                                                                       \
    static void CMD_OPT(ident)() {                                                                                     \
        (ident) = (set_expr);                                                                                          \
    }

const char* next_arg();
typedef struct {
    const char *shortform, *longform;
    void (*handler)();
} CmdArg;

void handle_cmdline(int, char*[]);
