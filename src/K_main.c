#ifdef _MSC_VER
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <SDL3/SDL_main.h>

#include <SDL3/SDL.h>
#define FIX_IMPLEMENTATION
#include <S_fixed.h>

#include "K_audio.h"
#include "K_game.h"
#include "K_log.h"
#include "K_video.h"

int main(int argc, char** argv) {
#ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
#endif

    bool bypass_shader = false;
    int local_port = 6969;
    int local_player = 0;
    int num_players = 1;
    const char* ip[MAX_PLAYERS] = {NULL};
    for (size_t i = 0; i < argc; i++) {
        if (SDL_strcmp(argv[i], "-players") == 0) {
            num_players = SDL_strtol(argv[++i], NULL, 0);
            if (num_players < 1 || num_players > MAX_PLAYERS)
                FATAL("Player amount must be between 1 and %li", MAX_PLAYERS);
            for (size_t j = 0; j < num_players; j++) {
                char* player = argv[++i];
                if (SDL_strlen(player) <= 5) {
                    local_port = SDL_strtol(player, NULL, 0);
                    local_player = (int)j;
                    ip[j] = NULL;
                } else {
                    ip[j] = player;
                }
            }
        } else if (SDL_strcmp(argv[i], "-bypass_shader") == 0) {
            bypass_shader = true;
        }
    }

    INFO("Playing as player %i (port %i)", local_player, local_port);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
        FATAL("SDL_Init fail: %s", SDL_GetError());
    video_init(bypass_shader);
    audio_init();

    GekkoSession* session = NULL;
    if (!gekko_create(&session))
        FATAL("gekko_create fail");
    GekkoConfig config = {0};
    config.num_players = num_players;
    config.max_spectators = 0;
    config.input_prediction_window = 3;
    config.state_size = sizeof(struct GameState);
    config.input_size = sizeof(struct GameInput);
    config.desync_detection = true;
    gekko_start(session, &config);
    gekko_net_adapter_set(session, gekko_default_adapter(local_port));

    for (size_t i = 0; i < num_players; i++) {
        if (ip[i] == NULL)
            gekko_add_actor(session, LocalPlayer, NULL);
        else
            gekko_add_actor(session, RemotePlayer, &((GekkoNetAddress){(void*)ip[i], SDL_strlen(ip[i])}));
    }
    if (num_players > 1)
        gekko_set_local_delay(session, local_player, 3);

    struct GameInput inputs[MAX_PLAYERS] = {0};
    start_state(num_players, local_player);

    uint64_t last_time = 0;
    float ticks = 0;
    uint64_t tick = 0;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                default:
                    break;
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
            }
        }

        const uint64_t current_time = SDL_GetTicks();
        ticks += (float)(current_time - last_time) *
                 ((float)((gekko_frames_ahead(session) >= 0.75f) ? (TICKRATE - 1L) : TICKRATE) / 1000.0f);
        last_time = current_time;

        gekko_network_poll(session);

        while (ticks >= 1) {
            struct GameInput input = {0};
            const bool* keyboard = SDL_GetKeyboardState(NULL);
            input.action.up = keyboard[SDL_SCANCODE_UP];
            input.action.left = keyboard[SDL_SCANCODE_LEFT];
            input.action.down = keyboard[SDL_SCANCODE_DOWN];
            input.action.right = keyboard[SDL_SCANCODE_RIGHT];
            input.action.jump = keyboard[SDL_SCANCODE_Z];
            input.action.run = input.action.fire = keyboard[SDL_SCANCODE_X];
            gekko_add_local_input(session, local_player, &input);

            int count = 0;
            GekkoSessionEvent** events = gekko_session_events(session, &count);
            for (int i = 0; i < count; i++) {
                GekkoSessionEvent* event = events[i];
                switch (event->type) {
                    default:
                        break;

                    case DesyncDetected: {
                        struct Desynced desync = event->data.desynced;
                        FATAL(
                            "DESYNC!!! f:%d, rh:%d, lc:%u, rc:%u", desync.frame, desync.remote_handle,
                            desync.local_checksum, desync.remote_checksum
                        );
                        break;
                    }

                    case PlayerConnected: {
                        struct Connected connect = event->data.connected;
                        INFO("Player %i connected", connect.handle);
                        break;
                    }

                    case PlayerDisconnected: {
                        struct Disconnected disconnect = event->data.disconnected;
                        FATAL("Player %i disconnected", disconnect.handle);
                        break;
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
                        save_state(event);
                        save_audio_state();
                        break;
                    }

                    case LoadEvent: {
                        load_state(event);
                        load_audio_state();
                        break;
                    }

                    case AdvanceEvent: {
                        for (size_t j = 0; j < num_players; j++)
                            inputs[j].value = event->data.adv.inputs[j];
                        tick = event->data.adv.frame;
                        tick_state(inputs);
                        audio_update();
                        break;
                    }
                }
            }

            ticks -= 1;
        }

        video_update();
    }

    gekko_destroy(session);
    video_teardown();
    audio_teardown();
    SDL_Quit();

#ifdef _MSC_VER
    _CrtDumpMemoryLeaks();
#endif

    return 0;
}
