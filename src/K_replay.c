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

static SDL_IOStream* replay_io = NULL;
static Sint64 replay_frames_offset = 0;

static GameInput last_input[MAX_PLAYERS] = {0};
static Uint32 last_checksum = 0;

const char* load_replay(const char* file) {
    end_replay();

    size_t bsize = 0;
    void* buffer = load_user_file(file, &bsize);
    if (buffer == NULL)
        return "msg_replay_missing";

    replay_io = SDL_IOFromConstMem(buffer, bsize);
    if (replay_io == NULL) {
        SDL_free(buffer);
        return "msg_replay_missing";
    }

    SDL_PropertiesID props = SDL_GetIOProperties(replay_io);
    if (props <= 0) {
        SDL_CloseIO(replay_io);
        SDL_free(buffer);
        return "msg_replay_missing";
    }

    SDL_SetPointerProperty(props, SDL_PROP_IOSTREAM_MEMORY_FREE_FUNC_POINTER, SDL_free);

    char header[sizeof(REPLAY_HEADER)] = "";
    SDL_ReadIO(replay_io, header, sizeof(header));
    if (SDL_strncmp(header, REPLAY_HEADER, sizeof(header)) != 0) {
        end_replay();
        return "msg_replay_invalid";
    }

    char version[sizeof(GAME_VERSION)] = "";
    SDL_ReadIO(replay_io, version, sizeof(version));
    if (SDL_strncmp(version, GAME_VERSION, sizeof(version)) != 0) {
        end_replay();
        return "msg_replay_version_mismatch";
    }

    Uint32 hash = 0;
    SDL_ReadU32LE(replay_io, &hash);
    if (hash != get_game_hash()) {
        end_replay();
        return "msg_replay_hash_mismatch";
    }

    // TODO: Assign view player
    PlayerID view_player = NULL_PLAYER;
    SDL_ReadS8(replay_io, &view_player);

    GameContext ctx = empty_game_context();
    SDL_ReadU64LE(replay_io, &ctx.level);
    SDL_ReadU64LE(replay_io, &ctx.seed);
    SDL_ReadU16LE(replay_io, &ctx.flags);
    SDL_ReadS16LE(replay_io, &ctx.checkpoint);
    SDL_ReadS8(replay_io, &ctx.num_players);
    for (PlayerID i = 0; i < ctx.num_players; i++) {
        SDL_ReadU8(replay_io, &ctx.players[i].xscroll);
        SDL_ReadU8(replay_io, &ctx.players[i].character);
        SDL_ReadU8(replay_io, &ctx.players[i].powerup);
        SDL_ReadS8(replay_io, &ctx.players[i].lives);
        SDL_ReadU8(replay_io, &ctx.players[i].coins);
        SDL_ReadU32LE(replay_io, &ctx.players[i].score);
    }

    INFO("Starting replay: %s", file);
    replay_state = RPS_PLAYING;
    jump_to_game(&ctx, TRUE);
    return NULL;
}

void start_replay() {
    if (!CLIENT.record_replay || replay_state != RPS_NONE)
        return;

    end_replay();

    replay_io = SDL_IOFromDynamicMem();
    ASSUME(replay_io, "Failed to allocate replay buffer");

    SDL_WriteIO(replay_io, REPLAY_HEADER, sizeof(REPLAY_HEADER));
    SDL_WriteIO(replay_io, GAME_VERSION, sizeof(GAME_VERSION));
    SDL_WriteU32LE(replay_io, get_game_hash());

    SDL_WriteS8(replay_io, viewplayer());

    const GameContext* ctx = gamecontext();
    SDL_WriteU64LE(replay_io, ctx->level);
    SDL_WriteU64LE(replay_io, ctx->seed);
    SDL_WriteU16LE(replay_io, ctx->flags);
    SDL_WriteS16LE(replay_io, ctx->checkpoint);
    SDL_WriteS8(replay_io, ctx->num_players);
    for (PlayerID i = 0; i < ctx->num_players; i++) {
        SDL_WriteU8(replay_io, ctx->players[i].xscroll);
        SDL_WriteU8(replay_io, ctx->players[i].character);
        SDL_WriteU8(replay_io, ctx->players[i].powerup);
        SDL_WriteS8(replay_io, ctx->players[i].lives);
        SDL_WriteU8(replay_io, ctx->players[i].coins);
        SDL_WriteU32LE(replay_io, ctx->players[i].score);
    }

    replay_frames_offset = SDL_TellIO(replay_io);
    INFO("Replay frames start at %" SDL_PRIs64, replay_frames_offset);

    replay_state = RPS_RECORDING;
    chat_message(LFMT("chat_recording"), B_U4_GREEN);
}

void end_replay() {
    if (replay_io != NULL) {
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
                chat_message(LFMT("chat_replay_save_failed"), B_U4_RED);
                break;
            }

            SDL_Time time = 0;
            SDL_GetCurrentTime(&time);
            SDL_DateTime dt = {0};
            SDL_TimeToDateTime(time, &dt, TRUE);

            const char* filename
                = fmt("%i-%i-%i %i.%02i.%02i.rpl", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

            if (save_user_stream(fmt("replays/%s", filename), replay_io, SDL_TellIO(replay_io)))
                chat_message(LFMT("chat_replay_saved", 's', filename), B_U4_GREEN);
            else
                chat_message(LFMT("chat_replay_save_failed"), B_U4_RED);

            break;
        }
        }
    }

    SDL_CloseIO(replay_io);
    replay_io = NULL;
    replay_state = RPS_NONE;

    SDL_zeroa(last_input);
    last_checksum = 0;
}

const GameInput* read_replay() {
    if (SDL_TellIO(replay_io) >= SDL_GetIOSize(replay_io))
        return NULL;

    const PlayerID num_players = gamecontext()->num_players;
    for (PlayerID i = 0; i < num_players; i++)
        SDL_ReadU8(replay_io, &last_input[i]);
    SDL_ReadU32LE(replay_io, &last_checksum);

    return last_input;
}

void write_replay(Sint64 frame, const GameInput* inputs, Uint32 checksum) {
    if (frame < 0)
        return;

    const PlayerID num_players = gamecontext()->num_players;

    SDL_SeekIO(replay_io,
        replay_frames_offset + (frame * (((Sint64)num_players * (Sint64)sizeof(GameInput)) + (Sint64)sizeof(Uint32))),
        SDL_IO_SEEK_CUR);

    for (PlayerID i = 0; i < num_players; i++)
        SDL_WriteU8(replay_io, inputs[i]);
    SDL_WriteU32LE(replay_io, checksum);
}

ReplayState get_replay_state() {
    return replay_state;
}

Uint32 get_replay_checksum() {
    return last_checksum;
}
