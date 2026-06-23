#pragma once

#include "K_game.h"

typedef Uint8 ReplayState;
enum {
    RPS_NONE,
    RPS_RECORDING,
    RPS_PLAYING,
};

const char* load_replay(const char*);

void start_replay(), end_replay();

const GameInput* read_replay();
void write_replay(int, const GameInput*, Uint32);

ReplayState get_replay_state();
Uint32 get_replay_checksum();
