#include "K_audio.h"
#include "K_video.h"

#include "actors/K_player.h"

static PlayerFrame get_player_frame(const GameActor* actor) {
    if (actor == NULL)
        return PF_IDLE;

    switch (VAL(actor, PLAYER_ANIMATION)) {
    default:
        return VAL(actor, PLAYER_ANIMATION);

    case PF_WALK:
        return PF_WALK1 + (Fx2Int(VAL(actor, PLAYER_FRAME)) % 3);

    case PF_FIRE:
        return PF_FIRE1 + (Fx2Int(VAL(actor, PLAYER_FRAME)) % 2);

    case PF_SWIM:
        return (VAL(actor, PLAYER_FRAME) < Int2Fx(6)) ? (PF_SWIM1 + Fx2Int(VAL(actor, PLAYER_FRAME)))
                                                      : (PF_SWIM7 + (Fx2Int(VAL(actor, PLAYER_FRAME)) % 2));

    case PF_GROW: {
        const GamePlayer* player = get_player(actor->player);
        return PF_GROW1
               + (Fx2Int(VAL(actor, PLAYER_FRAME))
                   % ((player == NULL || player->powerup == POW_NONE || player->powerup == POW_SUPER_MUSHROOM) ? 3
                                                                                                               : 4));
    }
    }

    return PF_IDLE;
}

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
    load_actor(ACT_WATER_SPLASH);
    load_actor(ACT_BUBBLE);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-9);
    actor->box.start.y = Int2Fx(-25);
    actor->box.end.x = Int2Fx(10);
    actor->box.end.y = Fx1;

    VAL(actor, PLAYER_GROUND) = 2;
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

    const GameCharacter* character = get_character(gamecontext()->players[player->id].character);
    if (character == NULL) {
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
        VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED) - 8192, Fx0);

    // 186
    if (!ANY_INPUT(player, GI_LEFT) && VAL(actor, X_SPEED) < Fx0)
        VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED) + 8192, Fx0);

    // 187
    if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_TOUCH) <= 0 && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
        && VAL(actor, X_SPEED) < Fx0)
    {
        VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED) + Fmul(24576, character->steer), Fx0);
    }

    // 188
    if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_TOUCH) >= 0 && !ANY_FLAG(actor, FLG_PLAYER_DUCK)
        && VAL(actor, X_SPEED) > Fx0)
    {
        VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED) - Fmul(24576, character->steer), Fx0);
    }

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

    // 189
    if (ANY_INPUT(player, GI_RIGHT) && VAL(actor, X_SPEED) > Fx0)
        FLAG_OFF(actor, FLG_X_FLIP);

    // 190
    if (ANY_INPUT(player, GI_LEFT) && VAL(actor, X_SPEED) < Fx0)
        FLAG_ON(actor, FLG_X_FLIP);

    // 218 (modified)
    const GameState* game_state = gamestate();
    const GameActor* water = get_actor(game_state->water);
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

    // 221 (modified)
    if (ANY_PRESSED(player, GI_JUMP) && water != NULL
        && ((actor->pos.y + actor->box.end.y) <= water->pos.y
            || (actor->pos.y + actor->box.start.y) >= (water->pos.y + Int2Fx(16)))
        && actor->pos.y >= water->pos.y && VAL(actor, Y_TOUCH) >= 0 && !ANY_INPUT(player, GI_DOWN))
    {
        VAL(actor, PLAYER_GROUND) = 0;
        VAL(actor, Y_SPEED) = Fmul(Int2Fx(-3), character->jump);
        if (VAL(actor, PLAYER_ANIMATION) == PF_SWIM)
            VAL(actor, PLAYER_FRAME) = Fx0;
        play_state_sound("swim", PLAY_POS, A_ACTOR(actor));
    }

    // 222 (modified)
    if (ANY_PRESSED(player, GI_JUMP) && water != NULL && (actor->pos.y + actor->box.end.y) > water->pos.y
        && (actor->pos.y + actor->box.start.y) < (water->pos.y + Int2Fx(16)) && actor->pos.y >= water->pos.y
        && VAL(actor, Y_TOUCH) >= 0 && !ANY_INPUT(player, GI_DOWN))
    {
        VAL(actor, PLAYER_GROUND) = 0;
        VAL(actor, Y_SPEED) = Fmul(Int2Fx(-9), character->jump);
        if (VAL(actor, PLAYER_ANIMATION) == PF_SWIM)
            VAL(actor, PLAYER_FRAME) = Fx0;
        play_state_sound("swim", PLAY_POS, A_ACTOR(actor));
    }

    // 223, 224, 474 (modified)
    if (water == NULL || (actor->pos.y + actor->box.end.y) <= water->pos.y
        || (actor->pos.y + actor->box.start.y) >= (water->pos.y + Int2Fx(16)))
    {
        FLAG_OFF(actor, FLG_PLAYER_TOUCHED_WATER);
    } else if ((actor->pos.y + actor->box.end.y) > water->pos.y
               && (actor->pos.y + actor->box.start.y) < (water->pos.y + Int2Fx(16)))
    {
        if (!ANY_FLAG(actor, FLG_PLAYER_TOUCHED_WATER)) {
            if (VAL(actor, X_SPEED) > 24576)
                VAL(actor, X_SPEED) = Fmax(VAL(actor, X_SPEED) - FxHalf, Fx0);
            else if (VAL(actor, X_SPEED) < -24576)
                VAL(actor, X_SPEED) = Fmin(VAL(actor, X_SPEED) + FxHalf, Fx0);

            if (actor->pos.y < (water->pos.y + Int2Fx(11)))
                create_actor(ACT_WATER_SPLASH, actor->pos);
        }

        FLAG_ON(actor, FLG_PLAYER_TOUCHED_WATER);
    }

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
        VAL(actor, Y_SPEED) = Fmul(Int2Fx(-13), character->jump);
        play_state_sound("jump", PLAY_POS, A_ACTOR(actor));
    }

    // 241
    if (ANY_INPUT(player, GI_JUMP) && (water == NULL || actor->pos.y < water->pos.y) && !ANY_INPUT(player, GI_DOWN)
        && VAL(actor, PLAYER_GROUND) > 0 && ANY_FLAG(actor, FLG_PLAYER_JUMP))
    {
        VAL(actor, PLAYER_GROUND) = 0;
        VAL(actor, Y_SPEED) = Fmul(Int2Fx(-13), character->jump);
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

    // Przejscie Etapu i Rury: TODO

    // 561, 562, 563 (modified)
    if (water != NULL && actor->pos.y > water->pos.y && (game_state->time % 5) == 0 && rng(10) == 5) {
        create_actor(ACT_BUBBLE, Vadd(actor->pos, (player->powerup == POW_NONE) ? (FVec2){Fx0, Int2Fx(-18)}
                                                                                : (FVec2){Int2Fx(2), Int2Fx(-39)}));
    }

    actor->box.start.y = (player->powerup == POW_NONE || ANY_FLAG(actor, FLG_PLAYER_DUCK)) ? Int2Fx(-25) : Int2Fx(-51);

    displace_actor(actor, Int2Fx(10), TRUE);
    if (VAL(actor, Y_TOUCH) > 0)
        VAL(actor, PLAYER_GROUND) = 2;
    else
        VAL_TICK(actor, PLAYER_GROUND);

    if (VAL(actor, PLAYER_ANIMATION) == PF_GROW
        && (((player->powerup == POW_NONE || player->powerup == POW_SUPER_MUSHROOM)
                && VAL(actor, PLAYER_FRAME) < Int2Fx(30))
            || (player->powerup != POW_NONE && player->powerup != POW_SUPER_MUSHROOM
                && VAL(actor, PLAYER_FRAME) < Int2Fx(40))))
    {
        VAL(actor, PLAYER_FRAME) += 59638;
    } else if (VAL(actor, PLAYER_GROUND) <= 0) {
        if (water != NULL && actor->pos.y > water->pos.y) {
            if (VAL(actor, PLAYER_ANIMATION) != PF_SWIM) {
                VAL(actor, PLAYER_ANIMATION) = PF_SWIM;
                VAL(actor, PLAYER_FRAME) = Fx0;
            }
            VAL(actor, PLAYER_FRAME) += Fclamp(Fdiv(Fabs(VAL(actor, X_SPEED)), 819200), 9175, 13763);
        } else {
            VAL(actor, PLAYER_ANIMATION) = (VAL(actor, Y_SPEED) < Fx0) ? PF_JUMP : PF_FALL;
            VAL(actor, PLAYER_FRAME) = Fx0;
        }

        if (player->powerup == POW_GREEN_LUI) {
            GameActor* effect = create_actor(ACT_PLAYER_EFFECT, actor->pos);
            if (effect != NULL) {
                VAL(effect, PLAYER_EFFECT_CHARACTER) = gamecontext()->players[player->id].character;
                VAL(effect, PLAYER_EFFECT_POWERUP) = player->powerup;
                VAL(effect, PLAYER_EFFECT_FRAME) = get_player_frame(actor);
                FLAG_ON(effect, actor->flags & FLG_X_FLIP);
                align_interp(effect, actor);
            }
        }
    } else if (VAL(actor, PLAYER_ANIMATION) == PF_FIRE && VAL(actor, PLAYER_FRAME) < Int2Fx(2)) {
        VAL(actor, PLAYER_FRAME) += (player->powerup == POW_FIRE_FLOWER) ? Fx1 : FxHalf;
    } else if (ANY_FLAG(actor, FLG_PLAYER_DUCK)) {
        VAL(actor, PLAYER_ANIMATION) = PF_DUCK;
        VAL(actor, PLAYER_FRAME) = Fx0;
    } else if (Fabs(VAL(actor, X_SPEED)) >= 8192) {
        VAL(actor, PLAYER_ANIMATION) = PF_WALK;
        VAL(actor, PLAYER_FRAME) += Fclamp(Fdiv(Fabs(VAL(actor, X_SPEED)), 819200), 7864, 31457);
    } else {
        VAL(actor, PLAYER_ANIMATION) = PF_IDLE;
        VAL(actor, PLAYER_FRAME) = Fx0;
    }

    collide_actor(actor);

    player->pos = actor->pos;
}

static void draw(const GameActor* actor) {
    const GamePlayer* player = get_player(actor->player);
    if (player == NULL)
        return;

    batch_reset();
    draw_actor(actor,
        get_character_sprite(gamecontext()->players[player->id].character, player->powerup, get_player_frame(actor)));
}

const ActorTable TAB_PLAYER = {
    .load = load,
    .create = create,
    .cleanup = cleanup,
    .tick = tick,
    .draw = draw,
};

/* =============
   PLAYER EFFECT
   ============= */

static void create_effect(GameActor* actor) {
    actor->box.start.x = Int2Fx(-9);
    actor->box.start.y = Int2Fx(-25);
    actor->box.end.x = Int2Fx(10);
    actor->box.end.y = Fx1;

    actor->depth = 1;

    VAL(actor, PLAYER_EFFECT_ALPHA) = Fx1;
}

static void tick_effect(GameActor* actor) {
    actor->box.start.y = (VAL(actor, PLAYER_EFFECT_POWERUP) == POW_NONE || VAL(actor, PLAYER_EFFECT_FRAME) == PF_DUCK)
                             ? Int2Fx(-25)
                             : Int2Fx(-51);
    ++actor->depth;

    VAL(actor, PLAYER_EFFECT_ALPHA) -= 2560;
    if (VAL(actor, PLAYER_EFFECT_ALPHA) <= Fx0)
        FLAG_ON(actor, FLG_DESTROY);
}

static void draw_effect(const GameActor* actor) {
    batch_reset();
    batch_color(B_ALPHA(Fx2Float(VAL(actor, PLAYER_EFFECT_ALPHA)) * 255.f));
    draw_actor(actor, get_character_sprite(VAL(actor, PLAYER_EFFECT_CHARACTER), VAL(actor, PLAYER_EFFECT_POWERUP),
                          VAL(actor, PLAYER_EFFECT_FRAME)));
}

const ActorTable TAB_PLAYER_EFFECT = {
    .create = create_effect,
    .tick = tick_effect,
    .draw = draw_effect,
};
