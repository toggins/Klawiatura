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
    SOUND(SND_1UP, "1up"),
    SOUND(SND_BANG1, "bang1"),
    SOUND(SND_BANG2, "bang2"),
    SOUND(SND_BANG3, "bang3"),
    SOUND(SND_BOWSER_DEAD, "bowser_dead"),
    SOUND(SND_BOWSER_FALL, "bowser_fall"),
    SOUND(SND_BOWSER_FIRE, "bowser_fire"),
    SOUND(SND_BOWSER_HURT, "bowser_hurt"),
    SOUND(SND_BOWSER_LAVA, "bowser_lava"),
    SOUND(SND_BREAK, "break"),
    SOUND(SND_BUMP, "bump"),
    SOUND(SND_CENTIPEDE, "centipede"),
    SOUND(SND_COIN, "coin"),
    SOUND(SND_CONNECT, "connect"),
    SOUND(SND_DEAD, "dead"),
    SOUND(SND_DISCONNECT, "disconnect"),
    // SOUND(SND_ENTER, "enter"),
    SOUND(SND_FIRE, "fire"),
    SOUND(SND_FLAMETHROWER, "flamethrower"),
    SOUND(SND_GROW, "grow"),
    SOUND(SND_HAMMER, "hammer"),
    // SOUND(SND_HARDCORE, "hardcore"),
    SOUND(SND_HURRY, "hurry"),
    SOUND(SND_HURT, "hurt"),
    SOUND(SND_JUMP, "jump"),
    SOUND(SND_KEVIN_ACTIVATE, "kevin_activate"),
    SOUND(SND_KEVIN_KILL, "kevin_kill"),
    SOUND(SND_KEVIN_SPAWN, "kevin_spawn"),
    SOUND(SND_KICK, "kick"),
    SOUND(SND_LAKITU1, "lakitu1"),
    SOUND(SND_LAKITU2, "lakitu2"),
    SOUND(SND_LAKITU3, "lakitu3"),
    SOUND(SND_LOSE, "lose"),
    SOUND(SND_MARIO_CHECKPOINT1, "mario_checkpoint1"),
    SOUND(SND_MARIO_CHECKPOINT2, "mario_checkpoint2"),
    SOUND(SND_MARIO_CHECKPOINT3, "mario_checkpoint3"),
    SOUND(SND_MARIO_CHECKPOINT4, "mario_checkpoint4"),
    // SOUND(SND_RESPAWN, "respawn"),
    // SOUND(SND_SELECT, "select"),
    SOUND(SND_SINK, "sink"),
    SOUND(SND_SPRING, "spring"),
    // SOUND(SND_SPRINT, "sprint"),
    SOUND(SND_SPROUT, "sprout"),
    SOUND(SND_STARMAN, "starman"),
    // SOUND(SND_START, "start"),
    SOUND(SND_STOMP, "stomp"),
    SOUND(SND_SWIM, "swim"),
    // SOUND(SND_SWITCH, "switch"),
    // SOUND(SND_TAIL, "tail"),
    SOUND(SND_THWOMP, "thwomp"),
    SOUND(SND_TICK, "tick"),
    // SOUND(SND_TOGGLE, "toggle"),
    SOUND(SND_WARP, "warp"),
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

void save_audio_state(struct SoundState* to) {
    FMOD_BOOL playing = false;
    FMOD_Channel_IsPlaying(music_channel, &playing);
    if (playing)
        FMOD_Channel_GetPosition(music_channel, &(state.track.offset), FMOD_TIMEUNIT_MS);
    SDL_memcpy(to, &state, sizeof(struct SoundState));
}

void load_audio_state(const struct SoundState* from) {
    const struct TrackObject old_track = state.track;
    SDL_memcpy(&state, from, sizeof(struct SoundState));

    if (state.track.index != old_track.index || state.track.loop != old_track.loop) {
        FMOD_ChannelGroup_Stop(music_group);
        if (state.track.index != MUS_NULL) {
            const struct Track* track = &(tracks[state.track.index]);
            if (state.track.offset < track->length || state.track.loop) {
                FMOD_System_PlaySound(speaker, track->stream, state_group, true, &music_channel);
                FMOD_Channel_SetMode(
                    music_channel, (state.track.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME
                );
                FMOD_Channel_SetPosition(music_channel, state.track.offset, FMOD_TIMEUNIT_MS);
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

        FMOD_CHANNEL* channel;
        FMOD_System_PlaySound(speaker, snd->sound, state_group, true, &channel);
        if (sound->pan) {
            FMOD_Channel_SetMode(channel, FMOD_3D | FMOD_3D_LINEARROLLOFF);
            FMOD_Channel_Set3DMinMaxDistance(channel, 320, 640);
            FMOD_Channel_Set3DAttributes(
                channel, &((FMOD_VECTOR){sound->pos[0], sound->pos[1], 0}), &((FMOD_VECTOR){0})
            );
        }
        FMOD_Channel_SetPosition(channel, sound->offset, FMOD_TIMEUNIT_MS);
        FMOD_Channel_SetPaused(channel, false);
    }
}

void tick_audio_state() {
    for (size_t i = 0; i < MAX_SOUNDS; i++) {
        struct SoundObject* sound = &(state.sounds[i]);
        if (sound->index == SND_NULL)
            continue;

        sound->offset += 20L;
        if (sound->offset >= sounds[sound->index].length)
            sound->index = SND_NULL;
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
    FMOD_Sound_GetLength(sound->sound, &(sound->length), FMOD_TIMEUNIT_MS);
}

void load_track(enum TrackIndices index) {
    struct Track* track = &(tracks[index]);
    if (track->stream != NULL)
        return;

    const char* file = find_file(track->name);
    FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESTREAM, NULL, &(track->stream));
    if (result != FMOD_OK)
        FATAL("Track \"%s\" fail: %s", track->name, FMOD_ErrorString(result));
    FMOD_Sound_GetLength(track->stream, &(track->length), FMOD_TIMEUNIT_MS);
    FMOD_Sound_SetLoopPoints(
        track->stream, track->loop[0], FMOD_TIMEUNIT_MS,
        (track->loop[1] <= track->loop[0]) ? track->length : track->loop[1], FMOD_TIMEUNIT_MS
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

    FMOD_CHANNEL* channel;
    FMOD_System_PlaySound(speaker, snd->sound, state_group, false, &channel);

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

    FMOD_CHANNEL* channel;
    FMOD_System_PlaySound(speaker, snd->sound, state_group, true, &channel);
    FMOD_Channel_SetMode(channel, FMOD_3D | FMOD_3D_LINEARROLLOFF);
    FMOD_Channel_Set3DMinMaxDistance(channel, 320, 640);
    FMOD_Channel_Set3DAttributes(channel, &((FMOD_VECTOR){x, y, 0}), &((FMOD_VECTOR){0}));
    FMOD_Channel_SetPaused(channel, false);

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
