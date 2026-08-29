#include <SDL3/SDL_platform_defines.h>

#include <SDL3_mixer/SDL_mixer.h>

#include "K_audio.h"
#include "K_cmd.h"
#include "K_file.h"
#include "K_log.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

typedef struct {
    TinyHash key;
    MIX_Track* channel;
} GenericSoundChannel;

typedef struct {
    Uint32 offset;
    float volume[3], time[2];

    TinyHash key;
    MIX_Track* channel;
} GenericTrackChannel;

static MIX_Mixer* speaker = NULL;
static MIX_Group *sound_group = NULL, *system_group = NULL, *music_group = NULL;

static SDL_PropertiesID midi_properties = 0, loop_properties = 0;

static float master_volume = 0.5f, sound_volume = 1.f, music_volume = 1.f;
static float mixer_volume = 1.0f;

static TinyMap sounds = {0}, tracks = {0};

static GenericSoundChannel generic_sounds[MAX_GENERIC_SOUNDS] = {0};
static size_t next_generic_sound = 0;

static GenericTrackChannel generic_track = {
    .volume = {0.f, 0.f, 1.f},
    .time = {0.f},
};

static AudioState *desired_audio_state = NULL, *actual_audio_state = NULL;
static MIX_Track *state_sound_channels[MAX_STATE_SOUNDS] = {NULL}, *state_track_channels[MAX_STATE_TRACKS] = {NULL};

static void mix_sound(void* userdata, MIX_Group* group, const SDL_AudioSpec* spec, float* pcm, int samples) {
    (void)userdata;
    (void)group;
    (void)spec;

    const float vol = sound_volume * mixer_volume;
    for (int i = 0; i < samples; i++)
        pcm[i] *= vol;
}

static void mix_system(void* userdata, MIX_Group* group, const SDL_AudioSpec* spec, float* pcm, int samples) {
    (void)userdata;
    (void)group;
    (void)spec;

    const float vol = sound_volume * master_volume;
    for (int i = 0; i < samples; i++)
        pcm[i] *= vol;
}

static void mix_music(void* userdata, MIX_Group* group, const SDL_AudioSpec* spec, float* pcm, int samples) {
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

    midi_properties = SDL_CreateProperties();
    EXPECT(midi_properties, "Failed to create midi properties: %s", SDL_GetError());
    SDL_SetFloatProperty(midi_properties, "synth.gain", 0.5f);
    SDL_SetNumberProperty(midi_properties, "synth.reverb.active", 0);
    SDL_SetNumberProperty(midi_properties, "synth.chorus.active", 0);

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

    generic_track.channel = MIX_CreateTrack(speaker);
    if (generic_track.channel == NULL)
        WTF("Failed to create generic track channels: %s", SDL_GetError());
    else
        MIX_SetTrackGroup(generic_track.channel, music_group);
}

void audio_update() {
    mixer_volume = (CLIENT.audio_in_background || window_focused()) ? master_volume : 0.f;

    if (MIX_TrackPlaying(generic_track.channel) && generic_track.time[0] < generic_track.time[1]) {
        generic_track.time[0] += deltaticks();
        if (generic_track.time[0] > generic_track.time[1])
            generic_track.time[0] = generic_track.time[1];

        generic_track.volume[0]
            = glm_lerp(generic_track.volume[1], generic_track.volume[2], generic_track.time[0] / generic_track.time[1]);
        MIX_SetTrackGain(generic_track.channel, generic_track.volume[0]);
    }
}

void audio_teardown() {
    audio_wipeout();

    FreeTinyMap(&sounds);
    FreeTinyMap(&tracks);

    SDL_DestroyProperties(midi_properties);
    SDL_DestroyProperties(loop_properties);

    for (size_t i = 0; i < MAX_GENERIC_SOUNDS; i++)
        MIX_DestroyTrack(generic_sounds[i].channel);
    MIX_DestroyTrack(generic_track.channel);

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

    const Track* track = get_track_key(generic_track.key);
    if (track == NULL || track->base.keep <= AKL_NEVER)
        stop_generic_track();
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

    SDL_IOStream* io = stream_data_file(fmt("tracks/%s.*", name), ".json");
    ASSUME(io, "Failed to load track \"%s\": %s", name, SDL_GetError());

    SDL_PropertiesID props = SDL_CreateProperties();
    EXPECT(props, "Failed to allocate track \"%s\" properties", name);
    SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, io);
    SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, TRUE);
    SDL_SetStringProperty(
        props, "SDL_mixer.decoder.fluidsynth.soundfont_path", fmt("%ssoundfont.sf2", get_base_path()));
    SDL_SetNumberProperty(props, "SDL_mixer.decoder.fluidsynth.props", midi_properties);

    track.internal = MIX_LoadAudioWithProperties(props);
    if (track.internal == NULL) {
        WTF("Failed to load track \"%s\":", SDL_GetError());
        SDL_DestroyProperties(props);
        return;
    }
    SDL_DestroyProperties(props);

    track.base.name = SDL_strdup(name);
    EXPECT(track.base.name, "Failed to allocate name for track \"%s\"", name);
    track.base.keep = keep;
    track.length = track.loop[1] = MIX_AudioFramesToMS(track.internal, MIX_GetAudioDuration(track.internal));

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

void play_generic_track(const char* name, PlayFlags flags, Uint32 offset) {
    const TinyHash key = StHashStr(name);
    const Track* track = get_track_key(key);
    WHATEVER(track, "Unknown track \"%s\"", name);

    stop_generic_track();

    MIX_SetTrackAudio(generic_track.channel, track->internal);
    MIX_SetTrackGain(generic_track.channel, 0.f);
    if (flags & PLAY_LOOPING) {
        SDL_SetNumberProperty(loop_properties, MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER, track->loop[0]);
        SDL_SetNumberProperty(loop_properties, MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER, track->loop[1] - 1);
        MIX_PlayTrack(generic_track.channel, loop_properties);
    } else {
        MIX_PlayTrack(generic_track.channel, 0);
    }
    MIX_SetTrackPlaybackPosition(generic_track.channel, MIX_TrackMSToFrames(generic_track.channel, (Sint64)offset));

    generic_track.key = key;
    generic_track.offset = offset;
}

void fade_generic_track(float volume, float time) {
    if (time <= 0.f) {
        generic_track.volume[0] = generic_track.volume[1] = generic_track.volume[2] = volume;
        generic_track.time[0] = generic_track.time[1] = time;
        return;
    }

    generic_track.volume[1] = generic_track.volume[0];
    generic_track.volume[2] = volume;
    generic_track.time[0] = 0.f;
    generic_track.time[1] = time;
}

void stop_generic_track() {
    generic_track.key = 0;
    MIX_StopTrack(generic_track.channel, 0);
    generic_track.volume[0] = generic_track.volume[1] = 0.f;
    generic_track.volume[2] = 1.f;
    generic_track.time[0] = 0.f;
    generic_track.time[1] = 1.f;
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

    for (size_t i = 0; i < MAX_STATE_TRACKS; i++) {
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
    const float left = 1.f - SDL_max(pan, 0.f), right = 1.f + SDL_min(pan, 0.f);
    MIX_SetTrackStereo(
        state_sound_channels[idx], &(MIX_StereoGains){SDL_clamp(left, 0.f, 1.f), SDL_clamp(right, 0.f, 1.f)});
}

static void move_state_sound(size_t idx) {
    const float* pos = desired_audio_state->sounds[idx].pos;

    const VideoCamera* camera = &videostate()->camera;
    const float cx = Fx2Float(camera->pos.x), cy = Fx2Float(camera->pos.y);

    const float dx = pos[0] - cx, dy = pos[1] - cy;
    const float pan = dx / (float)SCREEN_WIDTH;

    float att = SDL_sqrtf((dx * dx) + (dy * dy)) / (float)SCREEN_WIDTH;
    att = 1.f - SDL_min(att, 1.f);

    const float left = (1.f - SDL_max(pan, 0.f)) * att, right = (1.f + SDL_min(pan, 0.f)) * att;
    MIX_SetTrackStereo(
        state_sound_channels[idx], &(MIX_StereoGains){SDL_clamp(left, 0.f, 1.f), SDL_clamp(right, 0.f, 1.f)});
}

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
    if (!rollback) {
        for (size_t i = 0; i < MAX_STATE_SOUNDS; i++) {
            const SoundChannel* dschan = &desired_audio_state->sounds[i];
            SoundChannel* aschan = &actual_audio_state->sounds[i];

            if (aschan->sound_key == dschan->sound_key && aschan->flags == dschan->flags) {
                update_state_sound(i);
            } else {
                MIX_StopTrack(state_sound_channels[i], 0);

                const Sound* sound = get_sound_key(dschan->sound_key);
                if (sound != NULL) {
                    MIX_SetTrackAudio(state_sound_channels[i], sound->internal);
                    MIX_SetTrackPlaybackPosition(
                        state_sound_channels[i], MIX_TrackMSToFrames(state_sound_channels[i], (Sint64)dschan->offset));
                    update_state_sound(i);
                    MIX_PlayTrack(state_sound_channels[i], 0);
                }
            }

            *aschan = *dschan;
        }

        for (size_t i = 0; i < MAX_STATE_TRACKS; i++) {
            const TrackChannel* dtchan = &desired_audio_state->tracks[i];
            TrackChannel* atchan = &actual_audio_state->tracks[i];
            MIX_Track* tdata = state_track_channels[i];

            if (atchan->track_key != dtchan->track_key || atchan->flags != dtchan->flags) {
                MIX_StopTrack(tdata, 0);

                const Track* track = get_track_key(dtchan->track_key);
                if (track != NULL) {
                    MIX_SetTrackAudio(tdata, track->internal);
                    MIX_SetTrackPlaybackPosition(tdata, MIX_TrackMSToFrames(tdata, (Sint64)dtchan->offset));
                    if (dtchan->flags & PLAY_LOOPING) {
                        SDL_SetNumberProperty(
                            loop_properties, MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER, track->loop[0]);
                        SDL_SetNumberProperty(
                            loop_properties, MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER, track->loop[1] - 1);
                        MIX_PlayTrack(tdata, loop_properties);
                    } else {
                        MIX_PlayTrack(tdata, 0);
                    }
                }
            }

            if (MIX_TrackPlaying(tdata)) {
                const float volume = (i == viewplayer()) ? dtchan->volume[0] : 0.f;
                if (MIX_GetTrackGain(tdata) != volume)
                    MIX_SetTrackGain(tdata, volume);
            }

            *atchan = *dtchan;
        }
    }

    for (size_t i = 0; i < MAX_STATE_SOUNDS; i++) {
        SoundChannel* dschan = &desired_audio_state->sounds[i];

        const Sound* sound = get_sound_key(dschan->sound_key);
        if (sound == NULL)
            continue;

        dschan->offset += 1000 / TICKRATE;
        if (dschan->offset > (sound->length + (1000 / TICKRATE)))
            dschan->sound_key = 0;
    }

    for (size_t i = 0; i < MAX_STATE_TRACKS; i++) {
        TrackChannel* dtchan = &desired_audio_state->tracks[i];

        const Track* track = get_track_key(dtchan->track_key);
        if (track == NULL)
            continue;

        if (dtchan->time[0] < dtchan->time[1]) {
            dtchan->time[0] += 1.f;
            if (dtchan->time[0] > dtchan->time[1])
                dtchan->time[0] = dtchan->time[1];

            dtchan->volume[0] = glm_lerp(dtchan->volume[1], dtchan->volume[2], dtchan->time[0] / dtchan->time[1]);
        }

        dtchan->offset += 1000 / TICKRATE;
        if (dtchan->flags & PLAY_LOOPING)
            while (dtchan->offset >= track->loop[1])
                dtchan->offset = track->loop[0] + (dtchan->offset - track->loop[1]);
        else if (dtchan->offset >= track->length)
            dtchan->track_key = 0;
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

    for (size_t i = 0; i < MAX_STATE_TRACKS; i++) {
        MIX_DestroyTrack(state_track_channels[i]);
        state_track_channels[i] = NULL;
    }
}

void pause_audio_state(Bool pause) {
    if (pause) {
        for (size_t i = 0; i < MAX_STATE_SOUNDS; i++)
            MIX_PauseTrack(state_sound_channels[i]);
        for (size_t i = 0; i < MAX_STATE_TRACKS; i++)
            MIX_PauseTrack(state_track_channels[i]);
    } else {
        for (size_t i = 0; i < MAX_STATE_SOUNDS; i++)
            MIX_ResumeTrack(state_sound_channels[i]);
        for (size_t i = 0; i < MAX_STATE_TRACKS; i++)
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

void play_state_track(PlayerID pid, const char* name, PlayFlags flags) {
    if (pid < 0)
        return;

    const TinyHash key = StHashStr(name);

    const Track* track = get_track_key(key);
    WHATEVER(track, "Unknown track \"%s\"", name);

    PlayerID i = pid, n = (PlayerID)(pid + 1);
    if (pid >= ALL_TRACKS) {
        i = 0;
        n = MAX_STATE_TRACKS;
    }

    for (; i < n; i++) {
        TrackChannel* dtchan = &desired_audio_state->tracks[i];
        if (key != dtchan->track_key || flags != dtchan->flags) {
            dtchan->track_key = key;
            dtchan->offset = 0;
            dtchan->flags = flags;

            dtchan->volume[0] = dtchan->volume[1] = 0.f;
            dtchan->volume[2] = 1.f;
            dtchan->time[0] = 0.f;
            dtchan->time[1] = 1.f;
        } else {
            dtchan->volume[0] = dtchan->volume[1] = dtchan->volume[2] = 1.f;
            dtchan->time[0] = dtchan->time[1] = 0.f;
        }
    }
}

void fade_state_track(PlayerID pid, float volume, float time) {
    if (pid < 0)
        return;

    PlayerID i = pid, n = (PlayerID)(pid + 1);
    if (pid >= ALL_TRACKS) {
        i = 0;
        n = MAX_STATE_TRACKS;
    }

    for (; i < n; i++) {
        TrackChannel* dtchan = &desired_audio_state->tracks[i];

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
}

void stop_state_track(PlayerID pid) {
    if (pid < 0)
        return;

    PlayerID i = pid, n = (PlayerID)(pid + 1);
    if (pid >= ALL_TRACKS) {
        i = 0;
        n = MAX_STATE_TRACKS;
    }

    for (; i < n; i++)
        desired_audio_state->tracks[i].track_key = 0;
}
