#include <SDL3_mixer/SDL_mixer.h>

#include "K_audio.h"
#include "K_cmd.h"
#include "K_file.h"
#include "K_log.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

typedef Uint8 GenericTrackFlags;
enum {
    GTF_MELT = 1 << 0,
    GTF_INTERRUPT = 1 << 1,
};

typedef struct {
    TinyHash key;
    MIX_Track* channel;
} GenericSoundChannel;

typedef struct {
    GenericTrackFlags flags;

    Uint32 offset;
    float volume[3], time[2];
    float melt_volume, interrupt_volume;

    TinyHash key;
    MIX_Track* channel;
} GenericTrackChannel;

static MIX_Mixer* speaker = NULL;
static MIX_Group *sound_group = NULL, *system_group = NULL, *music_group = NULL;
static SDL_PropertiesID loop_properties = 0;

static float master_volume = 0.5f, sound_volume = 1.f, music_volume = 1.f;
static float mixer_volume = 1.0f;

static TinyMap sounds = {0}, tracks = {0};

static GenericSoundChannel generic_sounds[MAX_GENERIC_SOUNDS] = {0};
static size_t next_generic_sound = 0;

static GenericTrackChannel generic_tracks[MAX_GENERIC_TRACKS] = {
    {
     .volume = {1.f},
     .time = {0.f},
     .melt_volume = 1.f,
     .interrupt_volume = 1.f,
     }
};

static AudioState *desired_audio_state = NULL, *actual_audio_state = NULL;
static void *state_sound_channels[MAX_STATE_SOUNDS] = {NULL}, *state_track_channels[MAX_STATE_TRACKS] = {NULL};

void mix_sound(void* userdata, MIX_Group* group, const SDL_AudioSpec* spec, float* pcm, int samples) {
    (void)userdata;
    (void)group;
    (void)spec;

    const float vol = sound_volume * mixer_volume;
    for (int i = 0; i < samples; i++)
        pcm[i] *= vol;
}

void mix_system(void* userdata, MIX_Group* group, const SDL_AudioSpec* spec, float* pcm, int samples) {
    (void)userdata;
    (void)group;
    (void)spec;

    const float vol = sound_volume * master_volume;
    for (int i = 0; i < samples; i++)
        pcm[i] *= vol;
}

void mix_music(void* userdata, MIX_Group* group, const SDL_AudioSpec* spec, float* pcm, int samples) {
    (void)userdata;
    (void)group;
    (void)spec;

    const float vol = music_volume * mixer_volume;
    for (int i = 0; i < samples; i++)
        pcm[i] *= vol;
}

void audio_init() {
    EXPECT(MIX_Init(), "Failed to initialize audio system: %s", SDL_GetError());

    speaker = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (speaker == NULL)
        WTF("Failed to initialize audio device: %s", SDL_GetError());

    sound_group = MIX_CreateGroup(speaker);
    if (sound_group == NULL)
        WTF("Failed to create sound group: %s", SDL_GetError());
    else
        MIX_SetGroupPostMixCallback(sound_group, mix_sound, NULL);

    system_group = MIX_CreateGroup(speaker);
    if (system_group == NULL)
        WTF("Failed to create system sound group: %s", SDL_GetError());
    else
        MIX_SetGroupPostMixCallback(system_group, mix_system, NULL);

    music_group = MIX_CreateGroup(speaker);
    if (music_group == NULL)
        WTF("Failed to create music group: %s", SDL_GetError());
    else
        MIX_SetGroupPostMixCallback(music_group, mix_music, NULL);

    loop_properties = SDL_CreateProperties();
    EXPECT(loop_properties, "Failed to create loop properties: %s", SDL_GetError());
    SDL_SetNumberProperty(loop_properties, MIX_PROP_PLAY_LOOPS_NUMBER, -1);

    for (size_t i = 0; i < MAX_GENERIC_SOUNDS; i++) {
        generic_sounds[i].channel = MIX_CreateTrack(speaker);
        if (generic_sounds[i].channel == NULL) {
            WTF("Failed to create generic sound channels: %s", SDL_GetError());
            break;
        }

        MIX_SetTrackGroup(generic_sounds[i].channel, sound_group);
    }

    for (GenericTrackSlot i = 0; i < (GenericTrackSlot)MAX_GENERIC_TRACKS; i++) {
        generic_tracks[i].channel = MIX_CreateTrack(speaker);
        if (generic_tracks[i].channel == NULL) {
            WTF("Failed to create generic track channels: %s", SDL_GetError());
            break;
        }

        MIX_SetTrackGroup(generic_tracks[i].channel, music_group);
    }
}

static void stop_generic_track_pro(GenericTrackSlot slot) {
    GenericTrackChannel* gtchan = &generic_tracks[slot];
    gtchan->key = 0;
    MIX_StopTrack(gtchan->channel, 0);
    gtchan->volume[0] = gtchan->volume[1] = 0.f;
    gtchan->volume[2] = 1.f;
    gtchan->time[0] = 0.f;
    gtchan->time[1] = 1.f;
    gtchan->flags = 0;
    gtchan->melt_volume = gtchan->interrupt_volume = 1.f;
}

void audio_update() {
    mixer_volume = (CLIENT.audio_in_background || window_focused()) ? master_volume : 0.f;

    GenericTrackSlot top_track = MAX_GENERIC_TRACKS;
    while (top_track--) {
        GenericTrackChannel* gtchan = &generic_tracks[top_track];
        if (!MIX_TrackPlaying(gtchan->channel) || (gtchan->flags & GTF_MELT))
            continue;

        gtchan->flags &= ~GTF_INTERRUPT;
        for (GenericTrackSlot i = 0; i < top_track; i++)
            generic_tracks[i].flags |= GTF_INTERRUPT;

        break;
    }

    for (GenericTrackSlot i = 0; i < (GenericTrackSlot)MAX_GENERIC_TRACKS; i++) {
        GenericTrackChannel* gtchan = &generic_tracks[i];
        if (!MIX_TrackPlaying(gtchan->channel))
            continue;

        if ((gtchan->flags & GTF_MELT) && gtchan->melt_volume <= 0.f) {
            stop_generic_track_pro(i);
            continue;
        }

        Bool update_volume = FALSE;
        if (gtchan->time[0] < gtchan->time[1]) {
            gtchan->time[0] += deltaticks();
            if (gtchan->time[0] > gtchan->time[1])
                gtchan->time[0] = gtchan->time[1];
            gtchan->volume[0] = glm_lerp(gtchan->volume[1], gtchan->volume[2], gtchan->time[0] / gtchan->time[1]);
            update_volume = TRUE;
        }

#define GTVOLUME(flg, vol)                                                                                             \
    if (gtchan->flags & (flg)) {                                                                                       \
        gtchan->vol -= 0.0390625f * deltaticks();                                                                      \
        if (gtchan->vol < 0.f)                                                                                         \
            gtchan->vol = 0.f;                                                                                         \
        update_volume = TRUE;                                                                                          \
    } else if (gtchan->vol < 1.f) {                                                                                    \
        gtchan->vol += 0.0390625f * deltaticks();                                                                      \
        if (gtchan->vol > 1.f)                                                                                         \
            gtchan->vol = 1.f;                                                                                         \
        update_volume = TRUE;                                                                                          \
    }

        GTVOLUME(GTF_INTERRUPT, interrupt_volume)
        GTVOLUME(GTF_MELT, melt_volume)
#undef GTVOLUME

        if (update_volume)
            MIX_SetTrackGain(gtchan->channel, gtchan->volume[0] * gtchan->melt_volume * gtchan->interrupt_volume);
    }
}

void audio_teardown() {
    audio_wipeout();

    FreeTinyMap(&sounds);
    FreeTinyMap(&tracks);

    SDL_DestroyProperties(loop_properties);
    for (size_t i = 0; i < MAX_GENERIC_SOUNDS; i++)
        MIX_DestroyTrack(generic_sounds[i].channel);
    for (size_t i = 0; i < MAX_GENERIC_TRACKS; i++)
        MIX_DestroyTrack(generic_tracks[i].channel);
    MIX_DestroyGroup(sound_group);
    MIX_DestroyGroup(system_group);
    MIX_DestroyGroup(music_group);
    MIX_DestroyMixer(speaker);
    MIX_Quit();
}

void audio_wipeout() {
    for (size_t i = 0; i < MAX_GENERIC_SOUNDS; i++) {
        const Sound* sound = get_sound_key(generic_sounds[i].key);
        if (sound == NULL || sound->base.keep <= AKL_NEVER)
            MIX_StopTrack(generic_sounds[i].channel, 0);
    }
    next_generic_sound = 0;

    for (size_t i = 0; i < MAX_GENERIC_TRACKS; i++) {
        const Track* track = get_track_key(generic_tracks[i].key);
        if (track == NULL || track->base.keep <= AKL_NEVER)
            stop_generic_track(i);
    }
}

float get_volume() {
    return master_volume;
}

void set_volume(float volume) {
    master_volume = SDL_clamp(volume, 0.f, 1.f);
}

float get_sound_volume() {
    return sound_volume;
}

void set_sound_volume(float volume) {
    sound_volume = SDL_clamp(volume, 0.f, 1.f);
}

float get_music_volume() {
    return music_volume;
}

void set_music_volume(float volume) {
    music_volume = SDL_clamp(volume, 0.f, 1.f);
}

// ======
// ASSETS
// ======

static void nuke_sound(void* ptr) {
    Sound* sound = ptr;
    MIX_DestroyAudio(sound->internal);
    SDL_free((void*)sound->base.name);
}

ASSET_SRC(sounds, Sound, sound);

void load_sound(const char* name, AssetKeepLevel keep) {
    CHECK_ASSET(sounds);

    Sound sound = {0};

    sound.internal = MIX_LoadAudio_IO(speaker, stream_data_file(fmt("sounds/%s.*", name), NULL), TRUE, TRUE);
    ASSUME(sound.internal, "Failed to load sound \"%s\": %s", name, SDL_GetError());

    sound.base.name = SDL_strdup(name);
    EXPECT(sound.base.name, "Failed to allocate name for sound \"%s\"", name);
    sound.base.keep = keep;
    sound.length = MIX_AudioFramesToMS(sound.internal, MIX_GetAudioDuration(sound.internal));

    TinyDictPut(&sounds, name, &sound, sizeof(sound))->cleanup = nuke_sound;
}

static void nuke_track(void* ptr) {
    Track* track = ptr;
    MIX_DestroyAudio(track->internal);
    SDL_free((void*)track->base.name);
}

ASSET_SRC(tracks, Track, track);

void load_track(const char* name, AssetKeepLevel keep) {
    CHECK_ASSET(tracks);

    Track track = {0};

    track.internal = MIX_LoadAudio_IO(speaker, stream_data_file(fmt("tracks/%s.*", name), ".json"), FALSE, TRUE);
    ASSUME(track.internal, "Failed to load track \"%s\": %s", name, SDL_GetError());

    track.base.name = SDL_strdup(name);
    EXPECT(track.base.name, "Failed to allocate name for track \"%s\"", name);
    track.base.keep = keep;
    track.length = MIX_AudioFramesToMS(track.internal, MIX_GetAudioDuration(track.internal));

    yyjson_doc* json = load_data_json(fmt("tracks/%s.json", name));
    if (json != NULL) {
        yyjson_val* root = yyjson_doc_get_root(json);
        if (yyjson_is_obj(root)) {
            track.loop[0] = yyjson_get_uint(yyjson_obj_get(root, "loop_start")),
            track.loop[1] = yyjson_get_uint(yyjson_obj_get(root, "loop_end"));
            if (track.loop[0] >= track.loop[1])
                track.loop[1] = track.length;
        }

        yyjson_doc_free(json);
    }

trk_no_json:
    TinyDictPut(&tracks, name, &track, sizeof(track))->cleanup = nuke_track;
}

// ==============
// GENERIC SOUNDS
// ==============

void play_generic_sound(const char* name, PlayFlags flags) {
    const TinyHash key = StHashStr(name);
    const Sound* sound = get_sound_key(key);
    WHATEVER(sound, "Unknown sound \"%s\"", name);

    GenericSoundChannel* channel = &generic_sounds[next_generic_sound];
    next_generic_sound = (next_generic_sound + 1) % MAX_GENERIC_SOUNDS;

    channel->key = key;
    MIX_SetTrackGroup(channel->channel, (flags & PLAY_SYSTEM) ? system_group : sound_group);
    MIX_SetTrackAudio(channel->channel, sound->internal);
    MIX_PlayTrack(channel->channel, 0);
}

void play_generic_track(GenericTrackSlot slot, const char* name, PlayFlags flags, Uint32 offset) {
    if (slot < 0 || slot >= MAX_GENERIC_TRACKS)
        return;

    const TinyHash key = StHashStr(name);
    const Track* track = get_track_key(key);
    WHATEVER(track, "Unknown track \"%s\"", name);

    GenericTrackChannel* gtchan = &generic_tracks[slot];
    if (gtchan->key == key && gtchan->offset == offset) {
        if (gtchan->flags & GTF_MELT)
            gtchan->flags &= ~GTF_MELT;
        return;
    }

    stop_generic_track_pro(slot);

    for (GenericTrackSlot i = MAX_GENERIC_TRACKS - 1; i > slot; i--) {
        if (generic_tracks[i].channel == NULL || (generic_tracks[i].flags & GTF_MELT))
            continue;

        gtchan->flags |= GTF_INTERRUPT;
        break;
    }

    MIX_SetTrackAudio(gtchan->channel, track->internal);
    MIX_SetTrackGain(gtchan->channel, 0.f);
    if (flags & PLAY_LOOPING) {
        SDL_SetNumberProperty(loop_properties, MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER, track->loop[0]);
        SDL_SetNumberProperty(loop_properties, MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER, track->loop[1] - 1);
        MIX_PlayTrack(gtchan->channel, loop_properties);
    } else {
        MIX_PlayTrack(gtchan->channel, 0);
    }
    MIX_SetTrackPlaybackPosition(gtchan->channel, MIX_TrackMSToFrames(gtchan->channel, (Sint64)offset));

    gtchan->key = key;
    gtchan->offset = offset;
}

void stop_generic_track(GenericTrackSlot slot) {
    if (slot < 0 || slot >= MAX_GENERIC_TRACKS) {
        for (GenericTrackSlot i = 0; i < (GenericTrackSlot)MAX_GENERIC_TRACKS; i++)
            stop_generic_track_pro(i);
        return;
    }

    stop_generic_track_pro(slot);
}

static void fade_generic_track_pro(GenericTrackSlot slot, float volume, float time) {
    GenericTrackChannel* gtchan = &generic_tracks[slot];

    if (time <= 0.f) {
        gtchan->volume[0] = gtchan->volume[1] = gtchan->volume[2] = volume;
        gtchan->time[0] = gtchan->time[1] = time;
        MIX_SetTrackGain(gtchan->channel, volume * gtchan->interrupt_volume * gtchan->melt_volume);
        return;
    }

    gtchan->volume[1] = gtchan->volume[0];
    gtchan->volume[2] = volume;
    gtchan->time[0] = 0.f;
    gtchan->time[1] = time;
}

void fade_generic_track(GenericTrackSlot slot, float volume, float time) {
    if (slot < 0 || slot >= MAX_GENERIC_TRACKS) {
        for (GenericTrackSlot i = 0; i < (GenericTrackSlot)MAX_GENERIC_TRACKS; i++)
            fade_generic_track_pro(i, volume, time);
        return;
    }

    fade_generic_track_pro(slot, volume, time);
}

void melt_generic_track(GenericTrackSlot slot) {
    if (slot < 0 || slot >= MAX_GENERIC_TRACKS) {
        for (GenericTrackSlot i = 0; i < (GenericTrackSlot)MAX_GENERIC_TRACKS; i++)
            generic_tracks[i].flags |= GTF_MELT;
        return;
    }

    generic_tracks[slot].flags |= GTF_MELT;
}

// ============
// STATE SOUNDS
// ============

void start_audio_state() {
    desired_audio_state = SDL_calloc(1, sizeof(*desired_audio_state));
    EXPECT(desired_audio_state, "Failed to allocate desired audio state");

    actual_audio_state = SDL_calloc(1, sizeof(*actual_audio_state));
    EXPECT(actual_audio_state, "Failed to allocate actual audio state");

    for (size_t i = 0; i < MAX_STATE_SOUNDS; i++) {
        state_sound_channels[i] = MIX_CreateTrack(speaker);
        if (state_sound_channels[i] == NULL) {
            WTF("Failed to allocate state sound channels: %s", SDL_GetError());
            break;
        }

        MIX_SetTrackGroup(state_sound_channels[i], sound_group);
    }

    for (StateTrackSlot i = 0; i < (StateTrackSlot)MAX_STATE_TRACKS; i++) {
        state_track_channels[i] = MIX_CreateTrack(speaker);
        if (state_track_channels[i] == NULL) {
            WTF("Failed to allocate state track channels: %s", SDL_GetError());
            break;
        }

        MIX_SetTrackGroup(state_track_channels[i], music_group);
    }
}

static void pan_state_sound(size_t idx) {
    const float pan = desired_audio_state->sounds[idx].pos[0];
    const float left = 1.f - SDL_max(pan, 0.f);
    const float right = 1.f + SDL_min(pan, 0.f);

    MIX_SetTrackStereo(
        state_sound_channels[idx], &(MIX_StereoGains){SDL_clamp(left, 0.f, 1.f), SDL_clamp(right, 0.f, 1.f)});
}

static void move_state_sound(size_t idx) {}

static void update_state_sound(size_t idx) {
    if (desired_audio_state->sounds[idx].flags & PLAY_POS)
        move_state_sound(idx);
    else if (desired_audio_state->sounds[idx].flags & PLAY_PAN)
        pan_state_sound(idx);
    else
        MIX_SetTrackStereo(state_sound_channels[idx], NULL);
}

void tick_audio_state(Bool rollback) {
    // Don't update actual audio state during rollback frames.
    if (rollback)
        goto tas_tick_desired;

    for (size_t i = 0; i < MAX_STATE_SOUNDS; i++) {
        const SoundChannel* dschan = &desired_audio_state->sounds[i];
        SoundChannel* aschan = &actual_audio_state->sounds[i];

        if (aschan->sound_key == dschan->sound_key) {
            update_state_sound(i);
            goto ass_continue;
        }

        MIX_StopTrack(state_sound_channels[i], 0);

        const Sound* sound = get_sound_key(dschan->sound_key);
        if (sound == NULL)
            goto ass_continue;

        MIX_SetTrackAudio(state_sound_channels[i], sound->internal);
        MIX_SetTrackPlaybackPosition(
            state_sound_channels[i], MIX_TrackMSToFrames(state_sound_channels[i], (Sint64)dschan->offset));
        update_state_sound(i);
        MIX_PlayTrack(state_sound_channels[i], 0);

    ass_continue:
        *aschan = *dschan;
    }

    for (StateTrackSlot i = 0; i < (StateTrackSlot)MAX_STATE_TRACKS; i++) {
        const TrackChannel* dtchan = &desired_audio_state->tracks[i];
        TrackChannel* atchan = &actual_audio_state->tracks[i];

        if (atchan->track_key == dtchan->track_key) {
            if (get_track_key(atchan->track_key) != NULL) {
                if (atchan->hash != dtchan->hash) {
                    MIX_SetTrackPlaybackPosition(
                        state_track_channels[i], MIX_TrackMSToFrames(state_track_channels[i], (Sint64)dtchan->offset));
                }

                if (atchan->volume[0] != dtchan->volume[0])
                    MIX_SetTrackGain(state_track_channels[i], dtchan->volume[0]);
            }

            goto ats_continue;
        }

        MIX_StopTrack(state_track_channels[i], 0);

        const Track* track = get_track_key(dtchan->track_key);
        if (track == NULL)
            goto ats_continue;

        MIX_SetTrackAudio(state_track_channels[i], track->internal);
        MIX_SetTrackPlaybackPosition(
            state_track_channels[i], MIX_TrackMSToFrames(state_track_channels[i], (Sint64)dtchan->offset));
        MIX_SetTrackGain(state_track_channels[i], dtchan->volume[0]);
        if (dtchan->flags & PLAY_LOOPING) {
            SDL_SetNumberProperty(loop_properties, MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER, track->loop[0]);
            SDL_SetNumberProperty(loop_properties, MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER, track->loop[1] - 1);
            MIX_PlayTrack(state_track_channels[i], loop_properties);
        } else {
            MIX_PlayTrack(state_track_channels[i], 0);
        }

    ats_continue:
        *atchan = *dtchan;
    }

tas_tick_desired:
    for (size_t i = 0; i < MAX_STATE_SOUNDS; i++) {
        SoundChannel* dschan = &desired_audio_state->sounds[i];

        const Sound* sound = get_sound_key(dschan->sound_key);
        if (sound == NULL)
            continue;

        dschan->offset += 1000 / TICKRATE;
        if (dschan->offset >= sound->length)
            dschan->sound_key = 0;
    }

    for (StateTrackSlot i = 0; i < (StateTrackSlot)MAX_STATE_TRACKS; i++) {
        TrackChannel* dtchan = &desired_audio_state->tracks[i];

        const Track* track = get_track_key(dtchan->track_key);
        if (track == NULL)
            continue;

        dtchan->offset += 1000 / TICKRATE;
        if (dtchan->flags & PLAY_LOOPING) {
            while (dtchan->offset >= track->loop[1])
                dtchan->offset = track->loop[0] + (dtchan->offset - track->loop[1]);
        } else if (dtchan->offset >= track->length) {
            dtchan->track_key = 0;
            continue;
        }

        if (dtchan->time[0] < dtchan->time[1]) {
            dtchan->time[0] += 1.f;
            if (dtchan->time[0] > dtchan->time[1])
                dtchan->time[0] = dtchan->time[1];

            dtchan->volume[0] = glm_lerp(dtchan->volume[1], dtchan->volume[2], dtchan->time[0] / dtchan->time[1]);
        }
    }
}

void save_audio_state(AudioState* as) {
    *as = *desired_audio_state;
}

void load_audio_state(const AudioState* as) {
    *desired_audio_state = *as;
}

void nuke_audio_state() {
    SDL_free(desired_audio_state);
    desired_audio_state = NULL;

    SDL_free(actual_audio_state);
    actual_audio_state = NULL;

    for (size_t i = 0; i < MAX_STATE_SOUNDS; i++) {
        MIX_DestroyTrack(state_sound_channels[i]);
        state_sound_channels[i] = NULL;
    }

    for (StateTrackSlot i = 0; i < (StateTrackSlot)MAX_STATE_TRACKS; i++) {
        MIX_DestroyTrack(state_track_channels[i]);
        state_track_channels[i] = NULL;
    }
}

void pause_audio_state(Bool pause) {
    if (pause) {
        for (size_t i = 0; i < MAX_STATE_SOUNDS; i++)
            MIX_PauseTrack(state_sound_channels[i]);
        for (StateTrackSlot i = 0; i < (StateTrackSlot)MAX_STATE_TRACKS; i++)
            MIX_PauseTrack(state_track_channels[i]);
    } else {
        for (size_t i = 0; i < MAX_STATE_SOUNDS; i++)
            MIX_ResumeTrack(state_sound_channels[i]);
        for (StateTrackSlot i = 0; i < (StateTrackSlot)MAX_STATE_TRACKS; i++)
            MIX_ResumeTrack(state_track_channels[i]);
    }
}

void play_state_sound(const char* name, PlayFlags flags, const float pos[2]) {
    const TinyHash key = StHashStr(name);

    const Sound* sound = get_sound_key(key);
    WHATEVER(sound, "Unknown sound \"%s\"", name);

    SoundChannel* dschan = &desired_audio_state->sounds[desired_audio_state->next_sound];
    dschan->flags = flags;
    dschan->offset = 0;
    if (pos == NULL) {
        dschan->pos[0] = dschan->pos[1] = 0.f;
    } else {
        dschan->pos[0] = pos[0];
        dschan->pos[1] = pos[1];
    }
    dschan->sound_key = key;

    desired_audio_state->next_sound = (desired_audio_state->next_sound + 1) % MAX_STATE_SOUNDS;
}

void play_state_track(StateTrackSlot slot, const char* name, PlayFlags flags) {
    if (slot < 0 || slot >= MAX_STATE_TRACKS)
        return;

    const TinyHash key = StHashStr(name);

    const Track* track = get_track_key(key);
    WHATEVER(track, "Unknown track \"%s\"", name);

    TrackChannel* dtchan = &desired_audio_state->tracks[slot];
    ++dtchan->hash;
    dtchan->flags = flags;
    dtchan->volume[0] = dtchan->volume[1] = dtchan->volume[2] = 1.f;
    dtchan->time[0] = dtchan->time[1] = 0.f;
    dtchan->offset = 0;
    dtchan->track_key = key;
}

void stop_state_track(StateTrackSlot slot) {
    if (slot < 0 || slot >= MAX_STATE_TRACKS) {
        for (StateTrackSlot i = 0; i < (StateTrackSlot)MAX_STATE_TRACKS; i++)
            desired_audio_state->tracks[i].track_key = 0;
        return;
    }

    desired_audio_state->tracks[slot].track_key = 0;
}

static void fade_state_track_pro(StateTrackSlot slot, float volume, float time) {
    TrackChannel* dtchan = &desired_audio_state->tracks[slot];

    if (time <= 0.f) {
        dtchan->volume[0] = dtchan->volume[1] = dtchan->volume[2] = volume;
        dtchan->time[0] = dtchan->time[1] = time;
        return;
    }

    dtchan->volume[1] = dtchan->volume[0];
    dtchan->volume[2] = volume;
    dtchan->time[0] = 0.f;
    dtchan->time[1] = time;
}

void fade_state_track(StateTrackSlot slot, float volume, float time) {
    if (slot < 0 || slot >= MAX_STATE_TRACKS) {
        for (StateTrackSlot i = 0; i < (StateTrackSlot)MAX_STATE_TRACKS; i++)
            fade_state_track_pro(i, volume, time);
        return;
    }

    fade_state_track_pro(slot, volume, time);
}
