#pragma once

#include <SDL3/SDL_stdinc.h>

#include <fmod.h>

#define MAX_SOUNDS 16

typedef struct {
	char* name;
	FMOD_SOUND* sound;
	uint32_t length;
} Sound;

typedef struct {
	char* name;
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
	TrackObject tracks[TS_SIZE];
	TrackSlots top_track;

	SoundObject sounds[MAX_SOUNDS];
	uint8_t next_sound;
} AudioState;

extern AudioState audio_state;

void audio_init();
void audio_update();
void audio_teardown();

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
void save_audio_state(AudioState*);
void load_audio_state(const AudioState*);
void nuke_audio_state();

// Assets
void load_sound(const char*);
const Sound* get_sound(const char*);

void load_track(const char*);
const Track* get_track(const char*);

// Generic Sounds
void play_generic_sound(const char*);
void play_generic_track(const char*, PlayFlags);

// State Sounds
void play_state_sound(const char*);
void play_state_sound_at(const char*, const float[2]);

void play_state_track(TrackSlots, const char*, PlayFlags);
void stop_state_track(TrackSlots);
