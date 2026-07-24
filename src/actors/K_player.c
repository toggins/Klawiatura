#include "K_audio.h"
#include "K_video.h"

#include "actors/K_player.h"

/* ============
   PLAYER SPAWN
   ============ */

static void load_spawn() {
    load_actor(ACT_PLAYER);
}

static void create_spawn(GameActor* actor) {
    GameState* game_state = gamestate();

    GameActor* spawn = get_actor(game_state->spawn);
    if (spawn != NULL)
        FLAG_ON(spawn, FLG_DESTROY);

    game_state->spawn = actor->id;
}

static void cleanup_spawn(GameActor* actor) {
    GameState* game_state = gamestate();
    if (game_state->spawn == actor->id)
        game_state->spawn = NULL_ACTOR;
}

const ActorTable TAB_PLAYER_SPAWN = {
    .load = load_spawn,
    .create = create_spawn,
    .cleanup = cleanup_spawn,
};

/* ======
   PLAYER
   ====== */

static void load() {
    const GameContext* game_context = gamecontext();
    for (PlayerID i = 0; i < game_context->num_players; i++) {
        for (PlayerPowerup j = 0; j < (PlayerPowerup)POW_SIZE; j++)
            for (PlayerFrame k = 0; k < (PlayerFrame)PF_SIZE; k++)
                load_sprite(get_character_sprite(game_context->players[i].character, j, k), AKL_NEVER);
    }

    load_sound("jump", AKL_NEVER);
    load_sound("swim", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-9);
    actor->box.start.y = Int2Fx(-25);
    actor->box.end.x = Int2Fx(10);
    actor->box.end.y = Fx1;
}

static void cleanup(GameActor* actor) {
    GamePlayer* player = get_player(actor->player);
    if (player != NULL && player->actor == actor->id)
        player->actor = NULL_ACTOR;
}

static void tick(GameActor* actor) {
    GamePlayer* player = get_player(actor->player);
    if (player == NULL) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    // EVENTS FROM "Level 1-1"

    // 179
    if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_SPEED) == Fx0 && VAL(actor, X_TOUCH) == 0
        && !ANY_FLAG(actor, FLG_PLAYER_DUCK))
    {
        FLAG_OFF(actor, FLG_X_FLIP);
    }

    // 180
    if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_SPEED) == Fx0 && VAL(actor, X_TOUCH) == 0
        && !ANY_FLAG(actor, FLG_PLAYER_DUCK))
    {
        FLAG_ON(actor, FLG_X_FLIP);
    }

    // 181
    if (ANY_INPUT(player, GI_RIGHT) && !ANY_INPUT(player, GI_RUN) && VAL(actor, X_TOUCH) <= 0
        && !ANY_FLAG(actor, FLG_PLAYER_DUCK) && VAL(actor, X_SPEED) >= Fx0 && VAL(actor, X_SPEED) < 286720)
    {
        VAL(actor, X_SPEED) += 8192;
    }

    // 182
    if (ANY_INPUT(player, GI_LEFT) && !ANY_INPUT(player, GI_RUN) && VAL(actor, X_TOUCH) >= 0
        && !ANY_FLAG(actor, FLG_PLAYER_DUCK) && VAL(actor, X_SPEED) <= Fx0 && VAL(actor, X_SPEED) > -286720)
    {
        VAL(actor, X_SPEED) -= 8192;
    }

    // 183 (modified)
    if (ANY_INPUT(player, GI_RIGHT) && ANY_INPUT(player, GI_RUN) && VAL(actor, X_TOUCH) <= 0
        && !ANY_FLAG(actor, FLG_PLAYER_DUCK) && VAL(actor, X_SPEED) >= Fx0 && VAL(actor, X_SPEED) < 491520)
    {
        VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED) + 8192, 491520);
    }

    // 184 (modified)
    if (ANY_INPUT(player, GI_LEFT) && ANY_INPUT(player, GI_RUN) && VAL(actor, X_TOUCH) >= 0
        && !ANY_FLAG(actor, FLG_PLAYER_DUCK) && VAL(actor, X_SPEED) <= Fx0 && VAL(actor, X_SPEED) > -491520)
    {
        VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED) - 8192, -491520);
    }

    // 185
    if (!ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_SPEED) > Fx0)
        VAL(actor, X_SPEED) -= 8192;

    // 186
    if (!ANY_INPUT(player, GI_LEFT) && VAL(actor, X_SPEED) < Fx0)
        VAL(actor, X_SPEED) += 8192;

    // 187
    if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_TOUCH) <= 0 && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
        && VAL(actor, X_SPEED) < Fx0)
    {
        VAL(actor, X_SPEED) += 24576;
    }

    // 188
    if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_TOUCH) >= 0 && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
        && VAL(actor, X_SPEED) > Fx0)
    {
        VAL(actor, X_SPEED) -= 24576;
    }

    // 189
    if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_SPEED) > Fx0)
        FLAG_OFF(actor, FLG_X_FLIP);

    // 190
    if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_SPEED) < Fx0)
        FLAG_ON(actor, FLG_X_FLIP);

    // 191
    if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_TOUCH) <= 0 && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
        && VAL(actor, X_SPEED) < Fx1 && VAL(actor, X_SPEED) >= Fx0)
    {
        VAL(actor, X_SPEED) += Fx1;
    }

    // 192
    if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_TOUCH) >= 0 && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
        && VAL(actor, X_SPEED) > -Fx1 && VAL(actor, X_SPEED) <= Fx0)
    {
        VAL(actor, X_SPEED) -= Fx1;
    }

    // 193
    if (!ANY_INPUT(player, GI_RUN) && VAL(actor, X_SPEED) < -286720)
        VAL(actor, X_SPEED) += 8192;

    // 194
    if (!ANY_INPUT(player, GI_RUN) && VAL(actor, X_SPEED) > 286720)
        VAL(actor, X_SPEED) -= 8192;

    // 209, 210, 211, 212
    if (ANY_INPUT(player, GI_DOWN) && !ANY_INPUT(player, GI_LEFT | GI_RIGHT) && VAL(actor, PLAYER_GROUND) > 0
        && player->powerup != POW_NONE)
    {
        FLAG_ON(actor, FLG_PLAYER_DUCK);
    }

    // 213 (modified)
    if (ANY_FLAG(actor, FLG_PLAYER_DUCK)
        && (VAL(actor, PLAYER_GROUND) <= 0 || !ANY_INPUT(player, GI_DOWN) || player->powerup == POW_NONE))
    {
        FLAG_OFF(actor, FLG_PLAYER_DUCK);
    }

    // 214 (modified)
    if (ANY_FLAG(actor, FLG_PLAYER_DUCK) && !ANY_FLAG(actor, FLG_X_FLIP) && VAL(actor, X_SPEED) > Fx0)
        VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED) - 8192, Fx0);

    // 215 (modified)
    if (ANY_FLAG(actor, FLG_PLAYER_DUCK) && ANY_FLAG(actor, FLG_X_FLIP) && VAL(actor, X_SPEED) < Fx0)
        VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED) + 8192, Fx0);

    // 218 (modified)
    const GameActor* water = get_actor(gamestate()->water);
    if (ANY_INPUT(player, GI_JUMP) && VAL(actor, Y_SPEED) < Fx0 && (water == NULL || actor->pos.y < water->pos.y)
        && !ANY_INPUT(player, GI_DOWN) && Fabs(VAL(actor, X_SPEED)) < 40960)
    {
        VAL(actor, Y_SPEED) -= 26214;
    }

    // 219 (modified)
    if (ANY_INPUT(player, GI_JUMP) && VAL(actor, Y_SPEED) < Fx0 && (water == NULL || actor->pos.y < water->pos.y)
        && !ANY_INPUT(player, GI_DOWN) && Fabs(VAL(actor, X_SPEED)) >= 40960)
    {
        VAL(actor, Y_SPEED) -= FxHalf;
    }

    // 220 (modified)
    if (ANY_INPUT(player, GI_JUMP) && VAL(actor, Y_SPEED) < Fx0 && (water == NULL || actor->pos.y < water->pos.y)
        && !ANY_INPUT(player, GI_DOWN) && player->powerup == POW_GREEN_LUI)
    {
        VAL(actor, Y_SPEED) -= 6554;
    }

    // 221 (almost)
    if (ANY_PRESSED(player, GI_JUMP) && water != NULL
        && ((actor->pos.y + actor->box.end.y) <= water->pos.y
            || (actor->pos.y + actor->box.start.y) >= (water->pos.y + Int2Fx(16)))
        && actor->pos.y >= water->pos.y && VAL(actor, Y_TOUCH) >= 0 && !ANY_INPUT(player, GI_DOWN))
    {
        VAL(actor, PLAYER_GROUND) = 0;
        VAL(actor, Y_SPEED) = Int2Fx(-3);
        play_state_sound("swim", PLAY_POS, A_ACTOR(actor));
    }

    // 222
    if (ANY_PRESSED(player, GI_JUMP) && water != NULL && (actor->pos.y + actor->box.end.y) > water->pos.y
        && (actor->pos.y + actor->box.start.y) < (water->pos.y + Int2Fx(16)) && actor->pos.y >= water->pos.y
        && VAL(actor, Y_TOUCH) >= 0 && !ANY_INPUT(player, GI_DOWN))
    {
        VAL(actor, PLAYER_GROUND) = 0;
        VAL(actor, Y_SPEED) = Int2Fx(-9);
        play_state_sound("swim", PLAY_POS, A_ACTOR(actor));
    }

    // 223, 224: TODO

    // 225 (modified)
    if (water != NULL && actor->pos.y >= water->pos.y)
        VAL(actor, Y_SPEED) += 6554;

    // 229
    if (water == NULL || actor->pos.y < water->pos.y)
        VAL(actor, Y_SPEED) += Fx1;

    // 230
    if (VAL(actor, Y_SPEED) > Int2Fx(10))
        VAL(actor, Y_SPEED) = Int2Fx(10);

    // 240
    if (ANY_PRESSED(player, GI_JUMP) && (water == NULL || actor->pos.y < water->pos.y) && !ANY_INPUT(player, GI_DOWN)
        && VAL(actor, PLAYER_GROUND) > 0 && !ANY_FLAG(actor, FLG_PLAYER_JUMP))
    {
        VAL(actor, PLAYER_GROUND) = 0;
        VAL(actor, Y_SPEED) = Int2Fx(-13);
        play_state_sound("jump", PLAY_POS, A_ACTOR(actor));
    }

    // 241
    if (ANY_INPUT(player, GI_JUMP) && (water == NULL || actor->pos.y < water->pos.y) && !ANY_INPUT(player, GI_DOWN)
        && VAL(actor, PLAYER_GROUND) > 0 && ANY_FLAG(actor, FLG_PLAYER_JUMP))
    {
        VAL(actor, PLAYER_GROUND) = 0;
        VAL(actor, Y_SPEED) = Int2Fx(-13);
        FLAG_OFF(actor, FLG_PLAYER_JUMP);
        play_state_sound("jump", PLAY_POS, A_ACTOR(actor));
    }

    // 242
    if (!ANY_INPUT(player, GI_JUMP) && (water == NULL || actor->pos.y < water->pos.y) && !ANY_INPUT(player, GI_DOWN)
        && VAL(actor, PLAYER_GROUND) > 0 && ANY_FLAG(actor, FLG_PLAYER_JUMP))
    {
        FLAG_OFF(actor, FLG_PLAYER_JUMP);
    }

    // 243
    if (ANY_PRESSED(player, GI_JUMP) && VAL(actor, PLAYER_GROUND) <= 0 && VAL(actor, Y_SPEED) > Fx0)
        FLAG_ON(actor, FLG_PLAYER_JUMP);

    // 467, 468, 469: TODO

    // 471
    if (water != NULL && actor->pos.y > water->pos.y && VAL(actor, Y_SPEED) > Int2Fx(3))
        VAL(actor, Y_SPEED) -= Fx1;

    // 472
    if (VAL(actor, X_SPEED) > 245760 && water != NULL && actor->pos.y > water->pos.y)
        VAL(actor, X_SPEED) -= 24576;

    // 473
    if (VAL(actor, X_SPEED) < -245760 && water != NULL && actor->pos.y > water->pos.y)
        VAL(actor, X_SPEED) += 24576;

    // 474: TODO

    // Przejscie Etapu i Rury: TODO

    displace_actor(actor, Int2Fx(10), TRUE);
    if (VAL(actor, Y_TOUCH) > 0)
        VAL(actor, PLAYER_GROUND) = 2;
    else
        VAL_TICK(actor, PLAYER_GROUND);

    collide_actor(actor);

    player->pos = actor->pos;
}

static void draw(const GameActor* actor) {
    const GamePlayer* player = get_player(actor->player);
    if (player == NULL)
        return;

    batch_reset();
    const FVec2 ipos = get_interp(actor);
    const Sint32 ax = Fx2Int(ipos.x), ay = Fx2Int(ipos.y);
    const Bool small = player->powerup == POW_NONE || ANY_FLAG(actor, FLG_PLAYER_DUCK);
    batch_pos(B_XYZ(ax - 9.f, ay - (small ? 25.f : 51.f), Fx2Float(actor->depth)));
    batch_color(B_BLACK);
    batch_rectangle(NULL, B_WH(19.f, small ? 26.f : 52.f));
}

const ActorTable TAB_PLAYER = {
    .load = load,
    .create = create,
    .cleanup = cleanup,
    .tick = tick,
    .draw = draw,
};
