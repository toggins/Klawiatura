#pragma once

#include <SDL3/SDL_stdinc.h>

#include <fmod.h>

#define MAX_SOUNDS 32

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

typedef struct {
	void* temp;
} AudioState;

extern AudioState audio_state;

typedef uint8_t TrackSlots;
enum {
	TRK_MAIN,
	TRK_EVENT,
	TRK_POWER,
	TRK_SCORE,
	TRK_FANFARE,
	TRK_SIZE,
};

typedef uint8_t PlayFlags;
enum {
	PLAY_LOOPING = 1 << 0,
};

void audio_init();
void audio_update();
void audio_teardown();

void set_volume(float);
void set_sound_volume(float);
void set_music_volume(float);

// Assets

void load_sound(const char*);
const Sound* get_sound(const char*);

void load_track(const char*);
const Track* get_track(const char*);

// Generic

void play_generic_sound(const char*);
void play_generic_track(const char*, PlayFlags);
void stop_generic_sounds();
void stop_generic_tracks();

// State

void play_state_sound(const char*);
void play_state_sound_at(const char*, float, float);
void play_state_track(TrackSlots, const char*, PlayFlags);
void stop_state_sounds();
void stop_state_tracks();
