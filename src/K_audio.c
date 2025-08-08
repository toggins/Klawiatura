#include <fmod_errors.h>

#include "K_audio.h"
#include "K_file.h"
#include "K_log.h"

static FMOD_SYSTEM* speaker = NULL;
static FMOD_CHANNELGROUP* state_group = NULL;

#define SOUND(i, nm)                                                                                                   \
    [(i)] = {                                                                                                          \
        .name = "sounds/" nm,                                                                                          \
        .sound = NULL,                                                                                                 \
        .length = 0,                                                                                                   \
    }

static struct Sound sounds[SND_SIZE] = {
    [SND_NULL] = {0},
    SOUND(SND_JUMP, "jump"),
    SOUND(SND_FIRE, "fire"),
    SOUND(SND_BUMP, "bump"),
};

static struct SoundState state = {0};
static struct SoundState saved_state = {0};
static FMOD_CHANNEL* channels[MAX_SOUNDS] = {0};

void audio_init() {
    FMOD_RESULT result = FMOD_System_Create(&speaker, FMOD_VERSION);
    if (result != FMOD_OK)
        FATAL("Audio create fail: %s", FMOD_ErrorString(result));

    result = FMOD_System_Init(speaker, 2 * MAX_SOUNDS, FMOD_INIT_NORMAL, NULL);
    if (result != FMOD_OK)
        FATAL("Audio init fail: %s", FMOD_ErrorString(result));

    uint32_t version, buildnumber;
    FMOD_System_GetVersion(speaker, &version, &buildnumber);
    INFO(
        "FMOD version: %u.%02u.%02u - Build %u", (version >> 16) & 0xFFFF, (version >> 8) & 0xFF, version & 0xFF,
        buildnumber
    );

    FMOD_System_CreateChannelGroup(speaker, "state", &state_group);
}

void audio_update() {
    for (size_t i = 0; i < MAX_SOUNDS; i++) {
        if (channels[i] == NULL)
            continue;

        FMOD_BOOL playing = false;
        FMOD_Channel_IsPlaying(channels[i], &playing);
        if (playing) {
            FMOD_Channel_GetPosition(channels[i], &(state.sounds[i].offset), FMOD_TIMEUNIT_PCM);
        } else {
            state.sounds[i].index = SND_NULL;
            channels[i] = NULL;
        }
    }
    FMOD_System_Update(speaker);
}

void audio_teardown() {
    for (size_t i = 0; i < SND_SIZE; i++)
        if (sounds[i].sound != NULL)
            FMOD_Sound_Release(sounds[i].sound);
    FMOD_ChannelGroup_Release(state_group);
    FMOD_System_Release(speaker);
}

void save_audio_state() {
    SDL_memcpy(&saved_state, &state, sizeof(state));
}

void load_audio_state() {
    SDL_memcpy(&state, &saved_state, sizeof(state));

    FMOD_ChannelGroup_Stop(state_group);
    for (size_t i = 0; i < MAX_SOUNDS; i++) {
        const struct SoundObject* sound = &(state.sounds[i]);
        if (sound->index == SND_NULL)
            continue;
        const struct Sound* snd = &(sounds[sound->index]);
        if (sound->offset >= snd->length)
            continue;

        FMOD_System_PlaySound(speaker, snd->sound, state_group, true, &(channels[i]));
        FMOD_Channel_SetPosition(channels[i], sound->offset, FMOD_TIMEUNIT_PCM);
        FMOD_Channel_SetPaused(channels[i], false);
    }
}

void load_sound(enum SoundIndices index) {
    struct Sound* sound = &(sounds[index]);
    if (sound->sound != NULL)
        return;

    const char* file = find_file(sound->name);
    FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESAMPLE, NULL, &(sound->sound));
    if (result != FMOD_OK)
        FATAL("Sound \"%s\" fail: %s", sound->name, FMOD_ErrorString(result));
    FMOD_Sound_GetLength(sound->sound, &(sound->length), FMOD_TIMEUNIT_PCM);
}

void play_sound(enum SoundIndices index) {
    const struct Sound* snd = &(sounds[index]);
    if (snd->sound == NULL)
        FATAL("Invalid sound index %u", index);

    struct SoundObject* sound = &(state.sounds[state.next_sound]);
    sound->index = index;
    sound->offset = 0;
    FMOD_System_PlaySound(speaker, snd->sound, state_group, false, &(channels[state.next_sound]));
    state.next_sound = (state.next_sound + 1) % MAX_SOUNDS;
}
