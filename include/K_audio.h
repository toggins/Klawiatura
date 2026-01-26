#pragma once

#include <SDL3/SDL_stdinc.h>

#include <fmod.h>

#include "K_assets.h"

#define MAX_SOUNDS 16

#define A_NULL ((float[2]){0})
#define A_PAN(pan) ((float[2]){pan, 0})
#define A_XY(x, y) ((float[2]){x, y})
#define A_ACTOR(actor) ((float[2]){Fx2Float(actor->pos.x), Fx2Float(actor->pos.y)})

typedef struct {
	AssetBase base;

	FMOD_SOUND* sound;
	Uint32 length;
} Sound;

typedef struct {
	AssetBase base;

	FMOD_SOUND* stream;
	Uint32 length, loop[2];
} Track;

typedef Uint8 PlayFlags;
enum {
	PLAY_LOOPING = 1 << 0,
	PLAY_POS = 1 << 1,
	PLAY_PAN = 1 << 2,
};

typedef Uint8 TrackSlots;
enum {
	TS_MAIN,
	TS_EVENT,
	TS_POWER,
	TS_SCORE,
	TS_FANFARE,
	TS_SIZE,
};

typedef struct {
	StTinyKey track_key;
	Uint32 offset;
	PlayFlags flags;
} TrackObject;

typedef struct {
	StTinyKey sound_key;
	Uint32 offset;
	PlayFlags flags;
	float pos[2];
} SoundObject;

typedef struct {
	Uint8 next_sound;
	TrackSlots top_track;

	SoundObject sounds[MAX_SOUNDS];
	TrackObject tracks[TS_SIZE];
} AudioState;

extern AudioState audio_state;

void audio_init(), audio_update(), audio_teardown();

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
void play_generic_sound(const char*);

void play_generic_track(const char*, PlayFlags);
void stop_generic_track();

// State Sounds
void start_audio_state();
void tick_audio_state();
void save_audio_state(AudioState*), load_audio_state(const AudioState*);
void nuke_audio_state();
void pause_audio_state(Bool);

void play_state_sound(const char*, PlayFlags, Uint32, const float[2]);

void play_state_track(TrackSlots, const char*, PlayFlags, Uint32);
void stop_state_track(TrackSlots);

void move_ears(const float[2]);
