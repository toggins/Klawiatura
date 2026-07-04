#include <stdlib.h> // required for `exit(EXIT_FAILURE)`, `fprintf()` and `fflush`

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_platform_defines.h>

#include "K_cmake.h"
#include "K_file.h"
#include "K_log.h"
#include "K_string.h"

void handle_fatal(const char* file, int line, const char* func, const char* format, ...) {
    extern SDL_Window* WINDOW;

    SDL_MessageBoxButtonData buttons[] = {
        {.text = "Exit", .flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT}
    };

    va_list args = {0};
    va_start(args, format);
    const char* message = fmt("Sorry, " GAME_NAME " encountered a fatal error!\n\nFile: %s:%d\nFunction: %s()\n\n%s",
        log_basename(file), line, func, vfmt(format, args));
    va_end(args);

    SDL_MessageBoxData data = {
        .window = WINDOW,
        .message = message,
        .title = GAME_NAME,
        .flags = SDL_MESSAGEBOX_ERROR,
        .buttons = buttons,
        .numbuttons = SDL_arraysize(buttons),
    };

    SDL_ShowMessageBox(&data, NULL);
    exit(EXIT_FAILURE);
}

const char* log_basename(const char* path) {
    return file_basename(path);
}

static const char* log_levels[SDL_LOG_PRIORITY_COUNT] = {
    [SDL_LOG_PRIORITY_INFO] = "INFO",
    [SDL_LOG_PRIORITY_WARN] = "WARN",
    [SDL_LOG_PRIORITY_ERROR] = "ERROR",
    [SDL_LOG_PRIORITY_CRITICAL] = "FATAL",
};

static void klaw_logger(void* userdata, int category, SDL_LogPriority priority, const char* message) {
    (void)userdata;
    (void)category;

    FILE* const stream =
#ifdef SDL_PLATFORM_EMSCRIPTEN
        stdout;
#else
        stderr;
#endif

    fprintf(stream, "[%s] %s\n", log_levels[priority], message);
    fflush(stream);
}

void log_init() {
    SDL_SetLogOutputFunction(klaw_logger, NULL);
}

void log_teardown() {}
