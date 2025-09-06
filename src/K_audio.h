#pragma once

#include <SDL3/SDL_stdinc.h>
#include <fmod.h>

#include "K_memory.h" // IWYU pragma: keep

#define MAX_SOUNDS 16

enum TrackSlots {
    TS_LEVEL,
    TS_SWITCH,
    TS_POWER,
    TS_SCORE,
    TS_FANFARE,
    TS_SIZE,
};

struct Sound {
    char name[sizeof(StTinyKey)];
    FMOD_SOUND* sound;
    uint32_t length;
};

struct Track {
    char name[sizeof(StTinyKey)];
    FMOD_SOUND* stream;
    uint32_t length, loop[2];
};

struct SoundState {
    struct TrackObject {
        const struct Track* index;
        uint32_t offset;
        bool loop;
    } tracks[TS_SIZE];
    enum TrackSlots top_track;

    struct SoundObject {
        const struct Sound* index;
        uint32_t offset;

        bool pan;
        float pos[2];
    } sounds[MAX_SOUNDS];
    uint8_t next_sound;
};

void audio_init();
void audio_update();
void audio_teardown();

void start_audio_state();
void save_audio_state(struct SoundState*);
void load_audio_state(const struct SoundState*);
void tick_audio_state();

void load_sound(const char*);
const struct Sound* get_sound(const char*);
void load_track(const char*);
const struct Track* get_track(const char*);

void move_ears(float, float);

void play_ui_sound(const char*);
void play_sound(const char*);
void play_sound_at(const char*, float, float);
void stop_all_sounds();

void play_ui_track(const char*, bool);
void play_track(enum TrackSlots, const char*, bool);
void stop_track(enum TrackSlots);
void stop_all_tracks();
