#pragma once

#include "K_misc.h"

void chat_init(), chat_update(), chat_teardown();

Bool typing_in_chat();
void chat_message(const char*, const Uint8[4]);
void clear_chat();

void draw_chat();
