#pragma once

#include "K_game.h"
#include "K_misc.h"

Bool is_editing_level(), is_noclipping();
void set_editing_level(Bool), set_noclipping(Bool);

void load_editor();
Bool editor_baton_pass(GamePlayer*, GameActor*);
void draw_editor();
