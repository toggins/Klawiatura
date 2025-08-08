#pragma once

#include <SDL3/SDL_stdinc.h>
#include <fmod.h>

#define MAX_SOUNDS 16

typedef uint8_t SoundID;

enum SoundIndices {
    SND_NULL,

    SND_1UP,
    SND_BANG1,
    SND_BANG2,
    SND_BANG3,
    SND_BOWSER_FIRE,
    SND_BOWSER_HURT,
    SND_BOWSER_DEAD,
    SND_BOWSER_FALL,
    SND_BOWSER_LAVA,
    SND_BREAK,
    SND_BUMP,
    SND_CENTIPEDE,
    SND_COIN,
    SND_FIRE,
    SND_FLAMETHROWER,
    SND_GROW,
    SND_HAMMER,
    SND_HURRY,
    SND_HURT,
    SND_JUMP,
    SND_KICK,
    SND_LAKITU1,
    SND_LAKITU2,
    SND_LAKITU3,
    SND_MARIO_CHECKPOINT1,
    SND_MARIO_CHECKPOINT2,
    SND_MARIO_CHECKPOINT3,
    SND_MARIO_CHECKPOINT4,
    SND_SINK,
    SND_SPRING,
    SND_SPROUT,
    SND_STOMP,
    SND_SWIM,
    SND_THWOMP,
    SND_TICK,
    SND_WARP,

    SND_CONNECT,
    SND_DISCONNECT,
    SND_STARMAN,
    SND_LOSE,
    SND_DEAD,

    SND_KEVIN_ACTIVATE,
    SND_KEVIN_SPAWN,
    SND_KEVIN_KILL,

    SND_SIZE,
};

struct Sound {
    const char* name;
    FMOD_SOUND* sound;
    uint32_t length;
};

struct SoundState {
    struct SoundObject {
        enum SoundIndices index;
        uint32_t offset;
    } sounds[MAX_SOUNDS];
    uint8_t next_sound;
};

void audio_init();
void audio_update();
void audio_teardown();

void save_audio_state();
void load_audio_state();

void load_sound(enum SoundIndices);
void play_sound(enum SoundIndices);
