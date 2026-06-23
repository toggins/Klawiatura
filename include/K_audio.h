#pragma once

#include "K_assets.h"

#define MAX_GENERIC_SOUNDS 8
#define MAX_STATE_SOUNDS 16

#define A_PAN(pan) ((float[2]){pan, 0.f})
#define A_XY(x, y) ((float[2]){x, y})
#define A_FVEC2(fvec) A_XY(Fx2Float(fvec.x), Fx2Float(fvec.y))
#define A_ACTOR(actor) A_FVEC2(actor->pos)

typedef struct {
    AssetBase base;

    Uint32 length;

    void* internal;
} Sound;

typedef struct {
    AssetBase base;

    Uint32 length, loop[2];

    void* internal;
} Track;

typedef Uint8 PlayFlags;
enum {
    PLAY_LOOPING = 1 << 0,
    PLAY_PAN = 1 << 1,
    PLAY_POS = 1 << 2,
    PLAY_SYSTEM = 1 << 3,
};

typedef Uint8 GenericTrackSlot;
enum {
    GTS_MAIN,
    GTS_EVENT,
    GTS_FANFARE,
    MAX_GENERIC_TRACKS,
};

typedef Uint8 StateTrackSlot;
enum {
    STS_MAIN,
    MAX_STATE_TRACKS,
};

typedef struct {
    PlayFlags flags;
    float offset;
    float pos[2];

    TinyHash sound_key;
} SoundChannel;

typedef struct {
    Uint8 hash;
    PlayFlags flags;
    float offset;
    float volume[3], time[2];

    TinyHash track_key;
} TrackChannel;

typedef struct {
    Uint8 next_sound;
    SoundChannel sounds[MAX_STATE_SOUNDS];
    TrackChannel tracks[MAX_STATE_TRACKS];
} AudioState;

void audio_init(), audio_update(), audio_teardown();
void audio_wipeout();

float get_volume();
void set_volume(float);
float get_sound_volume();
void set_sound_volume(float);
float get_music_volume();
void set_music_volume(float);

// Assets
ASSET_HEAD(sounds, Sound, sound);
void load_sound_num(const char*, Uint32, Bool);

ASSET_HEAD(tracks, Track, track);

// Generic Sounds
void play_generic_sound(const char*, PlayFlags);

void play_generic_track(GenericTrackSlot, const char*, PlayFlags);
void stop_generic_track(GenericTrackSlot);
void fade_generic_track(GenericTrackSlot, float, float);
void melt_generic_track(GenericTrackSlot);

// State Sounds
void start_audio_state();
void tick_audio_state(Bool);
void save_audio_state(AudioState*), load_audio_state(const AudioState*);
void nuke_audio_state();
void pause_audio_state(Bool);

void play_state_sound(const char*, PlayFlags, const float[2]);

void play_state_track(StateTrackSlot, const char*, PlayFlags);
void stop_state_track(StateTrackSlot);
void fade_state_track(StateTrackSlot, float, float);
