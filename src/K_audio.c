#include <fmod_common.h>
#include <fmod_errors.h>

#include "K_audio.h"
#include "K_file.h"
#include "K_log.h"

static FMOD_SYSTEM* speaker = NULL;
static FMOD_CHANNELGROUP* state_group = NULL;
static FMOD_CHANNELGROUP* music_group = NULL;

static struct SoundState state = {0};
static FMOD_CHANNEL* music_channels[TS_SIZE] = {0};

static StTinyMap* sounds = NULL;
static StTinyMap* tracks = NULL;

void audio_init() {
    FMOD_Debug_Initialize(FMOD_DEBUG_LEVEL_WARNING, FMOD_DEBUG_MODE_TTY, NULL, NULL);
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
    FMOD_ChannelGroup_SetVolume(music_group, 0.5f);

    sounds = NewTinyMap();
    tracks = NewTinyMap();
}

void audio_update() {
    FMOD_System_Update(speaker);
}

void audio_teardown() {
    FreeTinyMap(sounds);
    FreeTinyMap(tracks);
    FMOD_ChannelGroup_Release(state_group);
    FMOD_ChannelGroup_Release(music_group);
    FMOD_System_Release(speaker);
}

void save_audio_state(struct SoundState* to) {
    SDL_memcpy(to, &state, sizeof(struct SoundState));
}

void load_audio_state(const struct SoundState* from) {
    for (size_t i = 0; i < TS_SIZE; i++) {
        const struct TrackObject* load_track = &(from->tracks[i]);
        struct TrackObject* cur_track = &(state.tracks[i]);
        if (cur_track->index != load_track->index || cur_track->loop != load_track->loop) {
            stop_track(i);
            if (load_track->index == NULL || (!(load_track->loop) && load_track->offset >= load_track->index->length))
                continue;

            FMOD_System_PlaySound(speaker, load_track->index->stream, music_group, true, &(music_channels[i]));
            FMOD_Channel_SetMode(
                music_channels[i], (load_track->loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME
            );
            FMOD_Channel_SetVolume(music_channels[i], (float)(state.top_track <= i));
            FMOD_Channel_SetPosition(music_channels[i], load_track->offset, FMOD_TIMEUNIT_MS);
            FMOD_Channel_SetPaused(music_channels[i], false);

            if (state.top_track < i) {
                for (size_t j = 0; j < i; j++)
                    if (music_channels[j] != NULL)
                        FMOD_Channel_SetVolume(music_channels[j], 0);
                state.top_track = i;
            }
        }
    }

    SDL_memcpy(&state, from, sizeof(struct SoundState));

    FMOD_ChannelGroup_Stop(state_group);
    for (size_t i = 0; i < MAX_SOUNDS; i++) {
        const struct SoundObject* sound = &(state.sounds[i]);
        if (sound->index == NULL || sound->offset >= sound->index->length)
            continue;

        FMOD_CHANNEL* channel;
        FMOD_System_PlaySound(speaker, sound->index->sound, state_group, true, &channel);
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
    for (size_t i = 0; i < TS_SIZE; i++) {
        struct TrackObject* track = &(state.tracks[i]);
        if (track->index == NULL)
            continue;

        track->offset += 20L;
        if (!(track->loop) && track->offset >= track->index->length)
            stop_track(i);
    }

    for (size_t i = 0; i < MAX_SOUNDS; i++) {
        struct SoundObject* sound = &(state.sounds[i]);
        if (sound->index == NULL)
            continue;

        sound->offset += 20L;
        if (sound->offset >= sound->index->length)
            sound->index = NULL;
    }
}

static void nuke_sound(void* ptr) {
    FMOD_Sound_Release(((struct Sound*)ptr)->sound);
}

void load_sound(const char* index) {
    if (get_sound(index) != NULL)
        return;

    struct Sound sound = {0};

    const char* file = find_file(file_pattern("data/sounds/%s.*", index), ".json");
    FMOD_SOUND* data = NULL;
    FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESAMPLE, NULL, &data);
    if (result != FMOD_OK)
        FATAL("Sound \"%s\" fail: %s", index, FMOD_ErrorString(result));

    SDL_strlcpy(sound.name, index, sizeof(sound.name));
    sound.sound = data;
    FMOD_Sound_GetLength(data, &(sound.length), FMOD_TIMEUNIT_MS);

    const StTinyKey key = StStrKey(index);
    StMapPut(sounds, key, &sound, sizeof(sound));
    StMapFind(sounds, key)->cleanup = nuke_sound;
}

const struct Sound* get_sound(const char* index) {
    return (struct Sound*)StMapGet(sounds, StStrKey(index));
}

static void nuke_track(void* ptr) {
    FMOD_Sound_Release(((struct Track*)ptr)->stream);
}

void load_track(const char* index) {
    if (get_track(index) != NULL)
        return;

    struct Track track = {0};

    const char* file = find_file(file_pattern("data/music/%s.*", index), ".json");
    FMOD_SOUND* data = NULL;
    FMOD_RESULT result = FMOD_System_CreateSound(speaker, file, FMOD_CREATESTREAM | FMOD_ACCURATETIME, NULL, &data);
    if (result != FMOD_OK)
        FATAL("Track \"%s\" fail: %s", index, FMOD_ErrorString(result));

    SDL_strlcpy(track.name, index, sizeof(track.name));
    track.stream = data;
    FMOD_Sound_GetLength(data, &(track.length), FMOD_TIMEUNIT_MS);

    file = find_file(file_pattern("data/music/%s.json", index), NULL);
    if (file != NULL) {
        yyjson_doc* json =
            yyjson_read_file(file, YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, NULL);
        if (json != NULL) {
            yyjson_val* root = yyjson_doc_get_root(json);
            if (yyjson_is_obj(root)) {
                unsigned int loop_start = yyjson_get_uint(yyjson_obj_get(root, "loop_start"));
                unsigned int loop_end = yyjson_get_uint(yyjson_obj_get(root, "loop_end"));
                if (loop_end <= 0)
                    loop_end = track.length;
                FMOD_Sound_SetLoopPoints(data, loop_start, FMOD_TIMEUNIT_MS, loop_end, FMOD_TIMEUNIT_MS);
            }
            yyjson_doc_free(json);
        }
    }

    StMapPut(tracks, StStrKey(index), &track, sizeof(track));
}

const struct Track* get_track(const char* index) {
    return (struct Track*)StMapGet(tracks, StStrKey(index));
}

void move_ears(float x, float y) {
    FMOD_System_Set3DListenerAttributes(
        speaker, 0, &((FMOD_VECTOR){x, y, -320}), &((FMOD_VECTOR){0}), &((FMOD_VECTOR){0, 0, -1}),
        &((FMOD_VECTOR){0, -1, 0})
    );
}

void play_ui_sound(const char* index) {
    const struct Sound* snd = get_sound(index);
    if (snd == NULL) {
        INFO("Unknown sound \"%s\"", index);
        return;
    }

    FMOD_System_PlaySound(speaker, snd->sound, NULL, false, NULL);
}

void play_sound(const char* index) {
    const struct Sound* snd = get_sound(index);
    if (snd == NULL) {
        INFO("Unknown sound \"%s\"", index);
        return;
    }

    struct SoundObject* sound = &(state.sounds[state.next_sound]);
    sound->index = snd;
    sound->offset = 0;
    sound->pan = false;

    FMOD_CHANNEL* channel;
    FMOD_System_PlaySound(speaker, snd->sound, state_group, false, &channel);

    state.next_sound = (state.next_sound + 1) % MAX_SOUNDS;
}

void play_sound_at(const char* index, float x, float y) {
    const struct Sound* snd = get_sound(index);
    if (snd == NULL) {
        INFO("Unknown sound \"%s\"", index);
        return;
    }

    struct SoundObject* sound = &(state.sounds[state.next_sound]);
    sound->index = snd;
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

void stop_all_sounds() {
    FMOD_ChannelGroup_Stop(state_group);
    FMOD_ChannelGroup_Stop(music_group);
}

void play_track(enum TrackSlots slot, const char* index, bool loop) {
    const struct Track* mus = get_track(index);
    if (mus == NULL) {
        INFO("Unknown track \"%s\"", index);
        return;
    }

    struct TrackObject* track = &(state.tracks[slot]);
    track->index = mus;
    track->offset = 0;
    track->loop = loop;

    if (music_channels[slot] != NULL)
        FMOD_Channel_Stop(music_channels[slot]);
    FMOD_System_PlaySound(speaker, mus->stream, music_group, true, &(music_channels[slot]));
    FMOD_Channel_SetMode(music_channels[slot], (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_ACCURATETIME);
    FMOD_Channel_SetVolume(music_channels[slot], (float)(state.top_track <= slot));
    FMOD_Channel_SetPaused(music_channels[slot], false);

    if (state.top_track < slot) {
        for (size_t i = 0; i < slot; i++)
            if (music_channels[i] != NULL)
                FMOD_Channel_SetVolume(music_channels[i], 0);
        state.top_track = slot;
    }
}

void stop_track(enum TrackSlots slot) {
    struct TrackObject* track = &(state.tracks[slot]);
    if (track->index == NULL)
        return;
    track->index = NULL;

    if (music_channels[slot] != NULL) {
        FMOD_Channel_Stop(music_channels[slot]);
        music_channels[slot] = NULL;
    }

    if (state.top_track == slot && slot > 0) {
        enum TrackSlots i = slot - 1;
        for (; i >= 0; i--)
            if (music_channels[i] != NULL) {
                FMOD_Channel_SetVolume(music_channels[i], 1);
                break;
            }
        state.top_track = i;
    }
}
