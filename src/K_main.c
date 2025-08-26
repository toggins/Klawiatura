#include <stdlib.h>

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

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

int main(int argc, char** argv) {
    bool bypass_shader = false;
    int num_players = 1;
    char* server_ip = "95.163.233.200"; // Public NutPunch server
    char* lobby_id = "Klawiatura";
    GameFlags start_flags = 0;
    for (size_t i = 0; i < argc; i++) {
        if (SDL_strcmp(argv[i], "-bypass_shader") == 0) {
            bypass_shader = true;
        } else if (SDL_strcmp(argv[i], "-players") == 0) {
            num_players = SDL_strtol(argv[++i], NULL, 0);
            if (num_players < 1 || num_players > MAX_PLAYERS)
                FATAL("Player amount must be between 1 and %li", MAX_PLAYERS);
        } else if (SDL_strcmp(argv[i], "-ip") == 0) {
            server_ip = argv[++i];
        } else if (SDL_strcmp(argv[i], "-lobby") == 0) {
            lobby_id = argv[++i];
        } else if (SDL_strcmp(argv[i], "-kevin") == 0) {
            start_flags |= GF_KEVIN;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
        FATAL("SDL_Init fail: %s", SDL_GetError());
    video_init(bypass_shader);
    audio_init();

    GekkoSession* session = NULL;
    if (!gekko_create(&session))
        FATAL("gekko_create fail");

    GekkoNetAdapter* adapter = nutpunch_init(num_players, server_ip, lobby_id);
    int local_player = net_wait(&num_players, &start_flags);
    if ((num_players <= 0 || num_players > MAX_PLAYERS) || (local_player < 0 || local_player >= MAX_PLAYERS))
        FATAL("Don't think I didn't see you trying to set invalid player indices! I'll kick your ass!");
    if (num_players > 1) {
        load_sound("CONNECT");
        play_sound("CONNECT");
        load_sound("DCONNECT");
        load_font(FNT_HUD);
    }

    if (start_flags & GF_KEVIN) {
        load_sound("KEVINON");
        play_sound("KEVINON");
        INFO("\n==================================");
        INFO("Kevin Mode activated. Good luck...");
        INFO("==================================\n");
    }

    GekkoConfig config = {0};
    config.num_players = num_players;
    config.max_spectators = 0;
    config.input_prediction_window = 8;
    config.state_size = sizeof(struct SaveState);
    config.input_size = sizeof(GameInput);
    config.desync_detection = true;
    gekko_start(session, &config);
    gekko_net_adapter_set(session, adapter);
    net_fill(session);
    if (num_players > 1)
        gekko_set_local_delay(session, local_player, 2);

    GameInput inputs[MAX_PLAYERS] = {GI_NONE};
    start_state(num_players, local_player, start_flags);

    bool success = true;
    char errmsg[1024] = "No errors detected.";

    uint64_t last_time = SDL_GetTicks();
    float ticks = 0;

    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                default:
                    break;
                case SDL_EVENT_WINDOW_MOVED:
                case SDL_EVENT_WINDOW_RESIZED:
                    last_time = SDL_GetTicks();
                    break;
                case SDL_EVENT_QUIT:
                    goto teardown;
            }
        }

        const float ahead = gekko_frames_ahead(session);
        const float frame_time = 1.0f / ((float)TICKRATE - SDL_clamp(ahead, 0.0f, 2.0f));

        const uint64_t current_time = SDL_GetTicks();
        ticks += (float)(current_time - last_time) / 1000.0f;
        last_time = current_time;

        gekko_network_poll(session);

        if (ticks >= frame_time) {
            interp_start();

            while (ticks >= frame_time) {
                GameInput input = GI_NONE;
                const bool* keyboard = SDL_GetKeyboardState(NULL);
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
                            success = false;
                            goto teardown;
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
                            success = false;
                            goto teardown;
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
                            save_state(&(save.game));
                            save_audio_state(&(save.audio));

                            *(event->data.save.state_len) = sizeof(save);
                            *(event->data.save.checksum) = check_state();
                            SDL_memcpy(event->data.save.state, &save, sizeof(save));
                            break;
                        }

                        case LoadEvent: {
                            const struct SaveState* load = (struct SaveState*)(event->data.load.state);
                            load_state(&(load->game));
                            load_audio_state(&(load->audio));
                            break;
                        }

                        case AdvanceEvent: {
                            for (size_t j = 0; j < num_players; j++)
                                inputs[j] = ((GameInput*)(event->data.adv.inputs))[j];
                            tick_state(inputs);
                            tick_audio_state();
                            break;
                        }
                    }
                }

                ticks -= frame_time;
            }

            interp_end();
        }

        interp_update(ticks);
        video_update(NULL);
        audio_update();
    }

teardown:
    gekko_destroy(session);
    session = NULL;

    if (!success) {
        stop_all_sounds();
        play_sound("DCONNECT");
        SDL_Event event;

        bool running = true;
        while (running) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    default:
                        break;
                    case SDL_EVENT_QUIT:
                        running = false;
                        break;
                }
            }

            video_update(errmsg);
            audio_update();
        }
    }

    net_teardown();
    video_teardown();
    audio_teardown();
    SDL_Quit();

#ifdef DUMP_MEMLEAKS
    _CrtDumpMemoryLeaks();
#endif

    return EXIT_SUCCESS;
}
