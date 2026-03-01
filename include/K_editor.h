#pragma once

#include "K_game.h"
#include "K_misc.h"

Bool is_editing_level();
void set_editing_level(Bool);

void load_editor();
void editor_baton_pass(GamePlayer*, GameActor*);
void draw_editor();
