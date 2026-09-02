#pragma once

#include "K_assets.h"
#include "K_game.h"
#include "K_misc.h"

#define MAX_GENERIC_SOUNDS 8
#define MAX_STATE_SOUNDS 16
#define MAX_STATE_TRACKS MAX_PLAYERS
#define ALL_TRACKS MAX_STATE_TRACKS

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
#define PLAY_LOOPING (PlayFlags)(1U << 0)
#define PLAY_PAN (PlayFlags)(1U << 1)
#define PLAY_POS (PlayFlags)(1U << 2)
#define PLAY_SYSTEM (PlayFlags)(1U << 3)

typedef struct {
    PlayFlags flags;
    Uint32 offset;
    float pos[2];

    TinyHash sound_key;
} SoundChannel;

typedef struct {
    PlayFlags flags;
    Uint32 offset;
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
ASSET_HEAD(tracks, Track, track);

// Generic Sounds
void play_generic_sound(const char*, PlayFlags);

void play_generic_track(const char*, PlayFlags, Uint32);
void fade_generic_track(float, float);
void stop_generic_track();

// State Sounds
void start_audio_state(), tick_audio_state(Bool), nuke_audio_state();
void save_audio_state(AudioState*), load_audio_state(const AudioState*);
void pause_audio_state(Bool);

void play_state_sound(const char*, PlayFlags, const float[2]);

void play_state_track(PlayerID, const char*, PlayFlags, Uint32);
void fade_state_track(PlayerID, float, float);
void stop_state_track(PlayerID);
