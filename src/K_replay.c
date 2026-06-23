#include <SDL3/SDL_time.h>

#include "K_chat.h"
#include "K_cmd.h"
#include "K_file.h"
#include "K_locale.h"
#include "K_log.h"
#include "K_replay.h"
#include "K_string.h"
#include "K_video.h"

#define REPLAY_HEADER "krpl0"

static ReplayState replay_state = RPS_NONE;
static Uint8* replay_buffer = NULL;
static size_t replay_cursor = 0, replay_size = 0;

static size_t last_frame = 0;
static GameInput last_input[MAX_PLAYERS] = {0};
static Uint32 last_checksum = 0;

static void rbuffer_read8(Uint8* dest) {
    if ((replay_cursor + sizeof(Uint8)) <= replay_size) {
        *dest = replay_buffer[replay_cursor];
        replay_cursor += sizeof(Uint8);
    }
}

#define RBUFFER_READX(bits, swap)                                                                                      \
    static void rbuffer_read##bits(Uint##bits* dest) {                                                                 \
        if ((replay_cursor + sizeof(Uint##bits)) <= replay_size) {                                                     \
            *dest = swap(*(Uint##bits*)(replay_buffer + replay_cursor));                                               \
            replay_cursor += sizeof(Uint##bits);                                                                       \
        }                                                                                                              \
    }

RBUFFER_READX(16, SDL_Swap16BE);
RBUFFER_READX(32, SDL_Swap32BE);
RBUFFER_READX(64, SDL_Swap64BE);

#undef RBUFFER_READX

static void rbuffer_read_string(char* dest, size_t maxlen) {
    char c = ' ';
    size_t len = 0;
    while (replay_cursor < replay_size && c != '\0') {
        c = *(char*)(replay_buffer + replay_cursor);
        replay_cursor += sizeof(char);

        if (len < maxlen)
            dest[len++] = c;
    }
}

static void grow_rbuffer(size_t inc) {
    while ((replay_cursor + inc) >= replay_size) {
        replay_size *= 2;
        replay_buffer = SDL_realloc(replay_buffer, replay_size);
        EXPECT(replay_buffer, "Failed to reallocate replay buffer");
    }
}

static void rbuffer_write8(const Uint8* src) {
    grow_rbuffer(sizeof(Uint8));
    replay_buffer[replay_cursor] = *src;
    replay_cursor += sizeof(Uint8);
}

#define RBUFFER_WRITEX(bits, swap)                                                                                     \
    static void rbuffer_write##bits(const Uint##bits* src) {                                                           \
        grow_rbuffer(sizeof(Uint##bits));                                                                              \
        *(Uint##bits*)(replay_buffer + replay_cursor) = swap(*src);                                                    \
        replay_cursor += sizeof(Uint##bits);                                                                           \
    }

RBUFFER_WRITEX(16, SDL_Swap16BE);
RBUFFER_WRITEX(32, SDL_Swap32BE);
RBUFFER_WRITEX(64, SDL_Swap64BE);

#undef RBUFFER_WRITEX

static void rbuffer_write_string(const char* src) {
    const size_t len = SDL_strlen(src) + 1;
    grow_rbuffer(len);
    SDL_memcpy(replay_buffer + replay_cursor, src, len);
    replay_cursor += len;
}

const char* load_replay(const char* file) {
    (void)file;

    end_replay();

    return "msg_replay_missing";
}

void start_replay() {
    if (!CLIENT.record_replay || replay_state != RPS_NONE)
        return;

    end_replay();

    CLIENT.record_replay = FALSE;
    replay_state = RPS_RECORDING;
    chat_message(LFMT("chat_recording"), B_GREEN);
}

void end_replay() {
    if (replay_buffer != NULL) {
        switch (replay_state) {
        default: {
            INFO("Freeing invalid buffer");
            break;
        }

        case RPS_PLAYING: {
            INFO("Stopping");
            break;
        }

        case RPS_RECORDING: {
            if (!save_user_folder("replays")) {
                chat_message(LFMT("chat_replay_save_failed"), B_RED);
                break;
            }

            SDL_Time time = 0;
            SDL_GetCurrentTime(&time);
            SDL_DateTime dt = {0};
            SDL_TimeToDateTime(time, &dt, TRUE);

            const char* filename
                = fmt("%i-%i-%i %i.%02i.%02i.rpl", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

            if (save_user_file(fmt("replays/%s", filename), replay_buffer, replay_cursor))
                chat_message(LFMT("chat_replay_saved", 's', filename), B_GREEN);
            else
                chat_message(LFMT("chat_replay_save_failed"), B_RED);

            break;
        }
        }
    }

    SDL_free(replay_buffer);
    replay_buffer = NULL;
    replay_cursor = replay_size = 0;
    replay_state = RPS_NONE;

    last_frame = 0;
    SDL_zeroa(last_input);
    last_checksum = 0;
}

const GameInput* read_replay() {
    if (replay_cursor >= replay_size)
        return NULL;

    const PlayerID num_players = gamecontext()->num_players;
    for (PlayerID i = 0; i < num_players; i++)
        rbuffer_read8(&last_input[i]);
    rbuffer_read32(&last_checksum);

    return last_input;
}

void write_replay(int frame, const GameInput* inputs, Uint32 checksum) {
    const PlayerID num_players = gamecontext()->num_players;
    if (last_frame > frame)
        replay_cursor -= (last_frame - frame) * ((num_players * sizeof(GameInput)) + sizeof(Uint32));

    for (PlayerID i = 0; i < num_players; i++)
        rbuffer_write8(&inputs[i]);
    rbuffer_write32(&checksum);

    last_frame = frame;
}

ReplayState get_replay_state() {
    return replay_state;
}

Uint32 get_replay_checksum() {
    return last_checksum;
}
