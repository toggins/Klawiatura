#include <fmod_errors.h>

#include "K_audio.h"
#include "K_file.h"
#include "K_log.h"

static FMOD_SYSTEM* speaker = NULL;
static FMOD_CHANNELGROUP* state_group = NULL;
static FMOD_CHANNELGROUP* music_group = NULL;

#define SOUND(i, nm)                                                                                                   \
    [(i)] = {                                                                                                          \
        .name = "sounds/" nm,                                                                                          \
        .sound = NULL,                                                                                                 \
        .length = 0,                                                                                                   \
    }

static struct Sound sounds[SND_SIZE] = {
    [SND_NULL] = {0},

    SOUND(SND_JUMP, "jump"), SOUND(SND_FIRE, "fire"),   SOUND(SND_BUMP, "bump"), SOUND(SND_COIN, "coin"),
    SOUND(SND_WARP, "warp"), SOUND(SND_GROW, "grow"),   SOUND(SND_KICK, "kick"), SOUND(SND_1UP, "1up"),
    SOUND(SND_HURT, "hurt"), SOUND(SND_STOMP, "stomp"),
};

#define TLOOP(i, nm, start, end)                                                                                       \
    [(i)] = {                                                                                                          \
        .name = "music/" nm,                                                                                           \
        .stream = NULL,                                                                                                \
        .length = 0,                                                                                                   \
        .loop = {(start), (end)},                                                                                      \
    }
#define TRACK(i, nm) TLOOP(i, nm, 0, 0)

static struct Track tracks[MUS_SIZE] = {
    [MUS_NULL] = {0},

    TLOOP(MUS_OVERWORLD1, "overworld1", 78982, 0),
    TRACK(MUS_OVERWORLD2, "overworld2"),
    TRACK(MUS_OVERWORLD3, "overworld3"),
    TRACK(MUS_OVERWORLD4, "overworld4"),
    TRACK(MUS_OVERWORLD5, "overworld5"),
    TRACK(MUS_OVERWORLD6, "overworld6"),

    TRACK(MUS_SNOW, "snow"),
};

static struct SoundState state = {0};
static struct SoundState saved_state = {0};
static FMOD_CHANNEL* channels[MAX_SOUNDS] = {0};
static FMOD_CHANNEL* music_channel = NULL;

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
    FMOD_System_CreateChannelGroup(speaker, "music", &music_group);
}

void audio_update() {
    FMOD_System_Update(speaker);
}

void audio_teardown() {
    for (size_t i = 0; i < SND_SIZE; i++)
        if (sounds[i].sound != NULL)
            FMOD_Sound_Release(sounds[i].sound);
    for (size_t i = 0; i < MUS_SIZE; i++)
        if (tracks[i].stream != NULL)
            FMOD_Sound_Release(tracks[i].stream);
    FMOD_ChannelGroup_Release(state_group);
    FMOD_ChannelGroup_Release(music_group);
    FMOD_System_Release(speaker);
}

void save_audio_state() {
    FMOD_BOOL playing = false;
    FMOD_Channel_IsPlaying(music_channel, &playing);
    if (playing)
        FMOD_Channel_GetPosition(music_channel, &(state.track.offset), FMOD_TIMEUNIT_PCM);

    for (size_t i = 0; i < MAX_SOUNDS; i++) {
        if (channels[i] == NULL)
            continue;

        FMOD_Channel_IsPlaying(channels[i], &playing);
        if (playing) {
            FMOD_Channel_GetPosition(channels[i], &(state.sounds[i].offset), FMOD_TIMEUNIT_PCM);
        } else {
            state.sounds[i].index = SND_NULL;
            channels[i] = NULL;
        }
    }
    SDL_memcpy(&saved_state, &state, sizeof(state));
}

void load_audio_state() {
    const struct TrackObject old_track = state.track;
    SDL_memcpy(&state, &saved_state, sizeof(state));

    if (state.track.index != old_track.index || state.track.loop != old_track.loop) {
        FMOD_ChannelGroup_Stop(music_group);
        if (state.track.index != MUS_NULL) {
            const struct Track* track = &(tracks[state.track.index]);
            if (state.track.offset < track->length || state.track.loop) {
                FMOD_System_PlaySound(speaker, track->stream, state_group, true, &music_channel);
                FMOD_Channel_SetMode(
                    music_channel, (state.track.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME
                );
                FMOD_Channel_SetPosition(music_channel, state.track.offset, FMOD_TIMEUNIT_PCM);
                FMOD_Channel_SetPaused(music_channel, false);
            }
        }
    }

    FMOD_ChannelGroup_Stop(state_group);
    for (size_t i = 0; i < MAX_SOUNDS; i++) {
        const struct SoundObject* sound = &(state.sounds[i]);
        if (sound->index == SND_NULL)
            continue;
        const struct Sound* snd = &(sounds[sound->index]);
        if (sound->offset >= snd->length)
            continue;

        FMOD_System_PlaySound(speaker, snd->sound, state_group, true, &(channels[i]));
        if (sound->pan) {
            FMOD_Channel_SetMode(channels[i], FMOD_3D);
            FMOD_Channel_Set3DMinMaxDistance(channels[i], 320, 640);
            FMOD_Channel_Set3DAttributes(
                channels[i], &((FMOD_VECTOR){sound->pos[0], sound->pos[1], 0}), &((FMOD_VECTOR){0})
            );
        }
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

void load_track(enum TrackIndices index) {
    struct Track* track = &(tracks[index]);
    if (track->stream != NULL)
        return;

    const char* file = find_file(track->name);
    FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESTREAM, NULL, &(track->stream));
    if (result != FMOD_OK)
        FATAL("Track \"%s\" fail: %s", track->name, FMOD_ErrorString(result));
    FMOD_Sound_GetLength(track->stream, &(track->length), FMOD_TIMEUNIT_PCM);
    FMOD_Sound_SetLoopPoints(
        track->stream, track->loop[0], FMOD_TIMEUNIT_PCM,
        (track->loop[1] <= track->loop[0]) ? track->length : track->loop[1], FMOD_TIMEUNIT_PCM
    );
}

void move_ears(float x, float y) {
    FMOD_System_Set3DListenerAttributes(
        speaker, 0, &((FMOD_VECTOR){x, y, -320}), &((FMOD_VECTOR){0}), &((FMOD_VECTOR){0, 0, -1}),
        &((FMOD_VECTOR){0, -1, 0})
    );
}

void play_sound(enum SoundIndices index) {
    const struct Sound* snd = &(sounds[index]);
    if (snd->sound == NULL)
        FATAL("Invalid sound index %u", index);

    struct SoundObject* sound = &(state.sounds[state.next_sound]);
    sound->index = index;
    sound->offset = 0;
    sound->pan = false;

    FMOD_System_PlaySound(speaker, snd->sound, state_group, false, &(channels[state.next_sound]));

    state.next_sound = (state.next_sound + 1) % MAX_SOUNDS;
}

void play_sound_at(enum SoundIndices index, float x, float y) {
    const struct Sound* snd = &(sounds[index]);
    if (snd->sound == NULL)
        FATAL("Invalid sound index %u", index);

    struct SoundObject* sound = &(state.sounds[state.next_sound]);
    sound->index = index;
    sound->offset = 0;

    sound->pan = true;
    sound->pos[0] = x;
    sound->pos[1] = y;

    FMOD_System_PlaySound(speaker, snd->sound, state_group, true, &(channels[state.next_sound]));
    FMOD_Channel_SetMode(channels[state.next_sound], FMOD_3D);
    FMOD_Channel_Set3DMinMaxDistance(channels[state.next_sound], 320, 640);
    FMOD_Channel_Set3DAttributes(channels[state.next_sound], &((FMOD_VECTOR){x, y, 0}), &((FMOD_VECTOR){0}));
    FMOD_Channel_SetPaused(channels[state.next_sound], false);

    state.next_sound = (state.next_sound + 1) % MAX_SOUNDS;
}

void play_track(enum TrackIndices index, bool loop) {
    const struct Track* mus = &(tracks[index]);
    if (mus->stream == NULL)
        FATAL("Invalid track index %u", index);

    struct TrackObject* track = &(state.track);
    track->index = index;
    track->offset = 0;
    track->loop = loop;

    FMOD_ChannelGroup_Stop(music_group);
    FMOD_System_PlaySound(speaker, mus->stream, music_group, true, &music_channel);
    FMOD_Channel_SetMode(music_channel, (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME);
    FMOD_Channel_SetPaused(music_channel, false);
}
