#include <stdlib.h>

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include <nutpunch.h>

#define FIX_IMPLEMENTATION
#include <S_fixed.h>

#include <SDL3/SDL_stdinc.h>
#define StAlloc SDL_malloc
#define StFree SDL_free
#define StMemset SDL_memset
#define StMemcpy SDL_memcpy

#include "K_log.h"
#define StLog INFO

#define S_TRUCTURES_IMPLEMENTATION
#include <S_tructures.h>

#include "K_audio.h"
#include "K_game.h"
#include "K_net.h"
#include "K_video.h"

static bool bypass_shader = false, play_intro = true;
static char* server_ip = NUTPUNCH_DEFAULT_SERVER;
static char name[NUTPUNCH_FIELD_DATA_MAX + 1] = "PLAYER";
static char level[NUTPUNCH_FIELD_DATA_MAX + 1] = "TEST";
static char skin[NUTPUNCH_FIELD_DATA_MAX + 1] = "";
static bool quickstart = false;
static GameFlags start_flags = 0;

static void parse_args(int argc, char* argv[]) {
    for (size_t i = 0; i < argc; i++) {
        if (SDL_strcmp(argv[i], "-bypass_shader") == 0)
            bypass_shader = true;
        else if (SDL_strcmp(argv[i], "-ip") == 0)
            server_ip = argv[++i];
        else if (SDL_strcmp(argv[i], "-name") == 0)
            SDL_strlcpy(name, argv[++i], sizeof(name));
        else if (SDL_strcmp(argv[i], "-skin") == 0)
            SDL_strlcpy(skin, argv[++i], sizeof(skin));
        else if (SDL_strcmp(argv[i], "-level") == 0) {
            SDL_strlcpy(level, argv[++i], sizeof(level));
            quickstart = true;
        } else if (SDL_strcmp(argv[i], "-kevin") == 0)
            start_flags |= GF_KEVIN;
        else if (!SDL_strcmp(argv[i], "-skip_intro"))
            play_intro = false;
    }
}

static void show_intro(), show_error_screen(const char*);

static GekkoSession* session = NULL;
static PlayerID num_players = 1, local_player = 0;
static GekkoNetAdapter* adapter = NULL;
static GekkoConfig config = {0};
static GameInput inputs[MAX_PLAYERS] = {GI_NONE};

static void start_gekko() {
    config.num_players = num_players;
    config.max_spectators = 0;
    config.input_prediction_window = 8;
    config.state_size = sizeof(struct SaveState);
    config.input_size = sizeof(GameInput);
    config.desync_detection = true;
    gekko_start(session, &config);
    gekko_net_adapter_set(session, adapter);
    local_player = net_fill(session);
    if (num_players > 1)
        gekko_set_local_delay(session, local_player, 2);

    SDL_memset(inputs, GI_NONE, sizeof(inputs));

    struct GameContext ctx = {0};
    ctx.num_players = num_players;
    ctx.local_player = local_player;
    SDL_memcpy(ctx.level, level, sizeof(ctx.level));
    SDL_memcpy(ctx.skin, skin, sizeof(ctx.skin));

    ctx.flags = start_flags;
    if (num_players <= 1)
        ctx.flags |= GF_SINGLE;
    for (size_t i = 0; i < num_players; i++)
        ctx.players[i].lives = -1;

    ctx.checkpoint = NULLOBJ;

    start_audio_state();
    start_state(&ctx);
}

static char errmsg[1024] = "No errors detected.", our_chat[CHAT_MSG_SIZE] = {0};
;
static bool typing = false;

static bool game_loop() {
    uint64_t last_time = SDL_GetPerformanceCounter();
    float ticks = 0;
    typing = false;

    for (;;) {
        bool skip_input = false;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                default:
                    break;
                case SDL_EVENT_WINDOW_MOVED:
                case SDL_EVENT_WINDOW_RESIZED:
                    last_time = SDL_GetPerformanceCounter();
                    break;
                case SDL_EVENT_QUIT:
                    return true;
                case SDL_EVENT_KEY_DOWN:
                    SDL_Scancode key = event.key.scancode;
                    if (key == SDL_SCANCODE_T && !typing && num_players > 1) {
                        SDL_memset(our_chat, 0, sizeof(our_chat));
                        typing = true;
                        break;
                    } else if (key == SDL_SCANCODE_BACKSPACE && typing) {
                        for (int i = CHAT_MSG_SIZE - 1; i >= 0; i--)
                            if (our_chat[i] != 0) {
                                our_chat[i] = 0;
                                break;
                            }
                        break;
                    } else if (typing) {
                        if (key == SDL_SCANCODE_RETURN && SDL_strlen(our_chat))
                            for (int i = 0; i < NUTPUNCH_MAX_PLAYERS; i++)
                                NutPunch_SendReliably(i, our_chat, CHAT_MSG_SIZE);

                        if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_ESCAPE) {
                            SDL_memset(our_chat, 0, sizeof(our_chat));
                            typing = false;
                            skip_input = true;
                            break;
                        }

                        char c = 0;
                        if (key == SDL_SCANCODE_SPACE)
                            c = ' ';
                        else {
                            const char* gross = SDL_GetKeyName(SDL_GetKeyFromScancode(key, SDL_KMOD_NONE, false));
                            if (SDL_strlen(gross) != 1)
                                break;
                            c = *gross;
                        }
                        if (!c)
                            break;
                        for (int i = CHAT_MSG_SIZE - 3; i >= 0; i--)
                            if (our_chat[i] != 0) {
                                our_chat[i + 1] = c;
                                goto good;
                            }
                        our_chat[0] = c;
                    good:
                        break;
                    }
            }
        }
        if (typing)
            skip_input = true;

        const float ahead = gekko_frames_ahead(session);

        const uint64_t current_time = SDL_GetPerformanceCounter();
        ticks += ((float)(current_time - last_time) / (float)SDL_GetPerformanceFrequency()) *
                 ((float)TICKRATE - SDL_clamp(ahead, 0.0f, 2.0f));
        last_time = current_time;

        gekko_network_poll(session);
        net_update(session);

        if (ticks < 1)
            goto skip_interp;

        interp_start();
        while (ticks >= 1) {
            GameInput input = GI_NONE;
            const bool* keyboard = SDL_GetKeyboardState(NULL);
            if (skip_input)
                goto noinput;

            if (keyboard[SDL_SCANCODE_ESCAPE])
                return true;
            if (keyboard[SDL_SCANCODE_UP])
                input |= GI_UP;
            if (keyboard[SDL_SCANCODE_LEFT])
                input |= GI_LEFT;
            if (keyboard[SDL_SCANCODE_DOWN])
                input |= GI_DOWN;
            if (keyboard[SDL_SCANCODE_RIGHT])
                input |= GI_RIGHT;
            if (keyboard[SDL_SCANCODE_Z])
                input |= GI_JUMP;
            if (keyboard[SDL_SCANCODE_X])
                input |= (GI_RUN | GI_FIRE);

        noinput:
            gekko_add_local_input(session, local_player, &input);

            int count = 0;
            GekkoSessionEvent** events = gekko_session_events(session, &count);
            for (int i = 0; i < count; i++) {
                GekkoSessionEvent* event = events[i];
                switch (event->type) {
                    default:
                        break;

                    case DesyncDetected: {
                        dump_state();
                        struct Desynced desync = event->data.desynced;
                        INFO(
                            "OOPS: Out of sync with player %d (tick %d, l %u != r %u)", desync.remote_handle,
                            desync.frame, desync.local_checksum, desync.remote_checksum
                        );
                        SDL_snprintf(
                            errmsg, sizeof(errmsg),
                            "Failed to sync with player %d.\nThe game has now stopped and dumped its state.",
                            desync.remote_handle
                        );
                        return false;
                    }

                    case PlayerConnected: {
                        struct Connected connect = event->data.connected;
                        INFO("Player %i connected", connect.handle);
                        break;
                    }

                    case PlayerDisconnected: {
                        struct Disconnected disconnect = event->data.disconnected;
                        INFO("OOPS: Player %i disconnected", disconnect.handle);
                        SDL_snprintf(
                            errmsg, sizeof(errmsg), "Lost connection to player %i.\nThe game has now stopped.",
                            disconnect.handle
                        );
                        return false;
                    }
                }
            }

            count = 0;
            GekkoGameEvent** updates = gekko_update_session(session, &count);
            for (int i = 0; i < count; i++) {
                GekkoGameEvent* event = updates[i];
                switch (event->type) {
                    default:
                        break;

                    case SaveEvent: {
                        static struct SaveState save;
                        save_state(&save);
                        save_video_state(&(save.video));
                        save_audio_state(&(save.audio));

                        *(event->data.save.state_len) = sizeof(save);
                        *(event->data.save.checksum) = check_state();
                        SDL_memcpy(event->data.save.state, &save, sizeof(save));
                        break;
                    }

                    case LoadEvent: {
                        const struct SaveState* load = (struct SaveState*)(event->data.load.state);
                        load_state(load);
                        load_video_state(&(load->video));
                        load_audio_state(&(load->audio));
                        break;
                    }

                    case AdvanceEvent: {
                        for (size_t j = 0; j < num_players; j++)
                            inputs[j] = ((GameInput*)(event->data.adv.inputs))[j];
                        tick_state(inputs);
                        tick_video_state();
                        tick_audio_state();
                        break;
                    }
                }
            }

            ticks -= 1;
        }
        interp_end();

    skip_interp:
        interp_update(ticks);
        video_update(NULL, typing ? our_chat : NULL);
        audio_update();
    }
}

int main(int argc, char* argv[]) {
    INFO("==========[KLAWIATURA]==========");
    INFO("      MARIO FOREVER ONLINE      ");
    INFO("================================");
    INFO("                                ");
    INFO("         ! DISCLAIMER !         ");
    INFO("   This is a free, open-source  ");
    INFO("project not created for any sort");
    INFO("           of profit.           ");
    INFO(" All assets belong to Nintendo. ");
    INFO("We do not condone any commercial");
    INFO("      use of this project.      ");
    INFO("                                ");
    parse_args(argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
        FATAL("SDL_Init fail: %s", SDL_GetError());
    video_init(bypass_shader);
    audio_init();
    adapter = net_init(server_ip, name, skin, &num_players, level, &start_flags);

    if (play_intro && !quickstart)
        show_intro();

    load_sound("CONNECT");
    load_sound("DCONNECT");
    load_sound("KEVINON");
    load_font(FNT_MAIN);

    for (;;) {
        nuke_state();

        if (!gekko_create(&session))
            FATAL("gekko_create fail");
        if (!quickstart && !net_wait()) {
            NutPunch_Disconnect();
            gekko_destroy(session);
            session = NULL;
            break;
        }

        if ((num_players <= 0 || num_players > MAX_PLAYERS))
            FATAL("Don't think I didn't see you trying to set invalid player indices!! I'll kick your ass!!");
        if (num_players > 1)
            play_sound("CONNECT");

        if (start_flags & GF_KEVIN) {
            play_sound("KEVINON");
            INFO("\n==================================");
            INFO("Kevin Mode activated. Good luck...");
            INFO("==================================\n");
        }

        start_gekko();
        bool success = game_loop();

        NutPunch_Disconnect();
        gekko_destroy(session);
        session = NULL;

        stop_all_sounds();

        if (!success)
            show_error_screen(errmsg);
        if (quickstart)
            break;
    }

    net_teardown();
    video_teardown();
    audio_teardown();
    SDL_Quit();

    return EXIT_SUCCESS;
}

static void show_error_screen(const char* errmsg) {
    stop_all_sounds();
    play_sound("DCONNECT");

    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                default:
                    break;
                case SDL_EVENT_QUIT:
                    return;
            }
        }

        video_update(errmsg, NULL);
        audio_update();

        if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_ESCAPE])
            return;
        SDL_Delay(1000 / TICKRATE);
    }
}

static void show_intro() {
    load_texture("Q_DISCL");
    float duration = 3.f, rem = duration, trans = duration / 5.f;

    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                default:
                    break;
                case SDL_EVENT_QUIT:
                    return;
            }
        }

        float alpha = 1.0f;
        if (rem >= duration - trans)
            alpha = (duration - rem) / trans;
        else if (rem <= trans)
            alpha = rem / trans;
        rem -= 1.0f / TICKRATE;

        video_update_custom_start();
        draw_sprite("Q_DISCL", XYZ(320.f, 240.f, 0.f), (bool[]){false, false}, 0.f, ALPHA((GLubyte)(255.f * alpha)));
        video_update_custom_end();
        audio_update();

        const bool* kbd = SDL_GetKeyboardState(NULL);
        if (kbd[SDL_SCANCODE_ESCAPE] || kbd[SDL_SCANCODE_Z])
            return;
        if (rem < 0.f)
            return;
        SDL_Delay(1000 / TICKRATE);
    }
}
