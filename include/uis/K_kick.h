#pragma once

#include "K_interface.h"
#include "K_net.h"

typedef struct {
    Bool promote;
    NetID peers[MAX_PEERS];
    Option options[MAX_OPTIONS];
    size_t option;
} UIKickData;
