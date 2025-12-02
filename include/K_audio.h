#pragma once

#include <SDL3/SDL_stdinc.h>

#include <fmod.h>

#define MAX_SOUNDS 16

typedef struct {
	char* name;
	bool transient;

	FMOD_SOUND* sound;
	uint32_t length;
} Sound;

typedef struct {
	char* name;
	bool transient;

	FMOD_SOUND* stream;
	uint32_t length, loop[2];
} Track;

typedef uint8_t TrackSlots;
enum {
	TS_MAIN,
	TS_EVENT,
	TS_POWER,
	TS_SCORE,
	TS_FANFARE,
	TS_SIZE,
};

typedef uint8_t PlayFlags;
enum {
	PLAY_LOOPING = 1 << 0,
	PLAY_PAN = 1 << 1,
};

typedef struct {
	const Track* track;
	uint32_t offset;
	PlayFlags flags;
} TrackObject;

typedef struct {
	const Sound* sound;
	uint32_t offset;
	PlayFlags flags;
	float pos[2];
} SoundObject;

typedef struct {
	uint8_t next_sound;
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
void move_ears(const float[2]);

// State
void start_audio_state();
void tick_audio_state();
void save_audio_state(AudioState*), load_audio_state(const AudioState*);
void nuke_audio_state();
void pause_audio_state(bool);

// Assets
void clear_sounds();
void load_sound(const char*, bool);
const Sound* get_sound(const char*);

void clear_tracks();
void load_track(const char*, bool);
const Track* get_track(const char*);

// Generic Sounds
void play_generic_sound(const char*);

void play_generic_track(const char*, PlayFlags);
void stop_generic_track();

// State Sounds
void play_state_sound(const char*);
void play_state_sound_at(const char*, const float[2]);

void play_state_track(TrackSlots, const char*, PlayFlags);
void stop_state_track(TrackSlots);
