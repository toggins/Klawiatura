#include "K_audio.h"
#include "K_net.h"
#include "K_video.h"

#include "actors/K_player.h"
#include "actors/K_warp.h"

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

void kill_player(GameActor* actor) {
    if (actor == NULL)
        return;

    GameActor* dead = create_actor(ACT_PLAYER_DEAD, actor->pos);
    if (dead == NULL)
        return;
    dead->player = actor->player;
    align_interp(dead, actor);

    GamePlayer* player = get_player(actor->player);
    if (player != NULL) {
        if (gamecontext()->num_players > 1)
            --player->lives;
        player->powerup = POW_NONE;
    }

    FLAG_ON(actor, FLG_DESTROY);
}

/* ============
   PLAYER SPAWN
   ============ */

static void load_spawn() {
    load_actor(ACT_PLAYER);
}

static void load_spawn_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_PLAYER_WARP_OUT))
        load_sound("warp", AKL_NEVER);
}

static void create_spawn(GameActor* actor) {
    GameState* game_state = gamestate();
    GameActor* spawn = get_actor(game_state->spawn);
    if (spawn != NULL)
        FLAG_ON(spawn, FLG_DESTROY);
    game_state->spawn = actor->id;

    VAL(actor, PLAYER_WARP_OUT_ANGLE) = 1;
}

static void cleanup_spawn(GameActor* actor) {
    GameState* game_state = gamestate();
    if (game_state->spawn == actor->id)
        game_state->spawn = NULL_ACTOR;
}

const ActorTable TAB_PLAYER_SPAWN = {
    .load = load_spawn,
    .load_special = load_spawn_special,
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
    load_actor(ACT_PLAYER_DEAD);
    load_actor(ACT_WATER_SPLASH);
    load_actor(ACT_BUBBLE);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-9);
    actor->box.start.y = Int2Fx(-25);
    actor->box.end.x = Int2Fx(10);
    actor->box.end.y = Fx1;

    VAL(actor, PLAYER_WARP) = NULL_ACTOR;
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

    const GameState* game_state = gamestate();
    const GameActor* water = get_actor(game_state->water);

    const GameActor* warp = get_actor(VAL(actor, PLAYER_WARP));
    if (warp != NULL || ANY_FLAG(actor, FLG_PLAYER_WARP_OUT)) {
        actor->depth = Int2Fx(22);
        actor->vel.x = actor->vel.y = Fx0;
        actor->touch = TOUCH_BOTTOM;
        FLAG_OFF(actor, FLG_PLAYER_DUCK);

        if (ANY_FLAG(actor, FLG_PLAYER_WARP_OUT)) {
            switch (VAL(actor, PLAYER_WARP_OUT_ANGLE)) {
            default: {
                move_actor(actor, Vadd(actor->pos, (FVec2){Fx1, Fx0}));
                FLAG_OFF(actor, FLG_X_FLIP);
                break;
            }

            case 1: {
                move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, -Fx1}));
                FLAG_ON(actor, FLG_PLAYER_DUCK);
                break;
            }

            case 2: {
                move_actor(actor, Vadd(actor->pos, (FVec2){-Fx1, Fx0}));
                FLAG_ON(actor, FLG_X_FLIP);
                break;
            }

            case 3: {
                move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Fx1}));
                TOUCH_OFF(actor, TOUCH_ALL);
                break;
            }
            }

            if (!touching_solid(Radd(actor->box, actor->pos), SOL_SOLID)) {
                actor->depth = Fx0;
                FLAG_OFF(actor, FLG_PLAYER_WARP_OUT);
            }
        } else if (warp != NULL) {
            if (VAL(actor, PLAYER_WARP_STATE) < 60) {
                switch (VAL(warp, WARP_ANGLE)) {
                default: {
                    move_actor(actor, Vadd(actor->pos, (FVec2){Fx1, Fx0}));
                    FLAG_OFF(actor, FLG_X_FLIP);
                    break;
                }

                case 1: {
                    move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, -Fx1}));
                    TOUCH_OFF(actor, TOUCH_ALL);
                    break;
                }

                case 2: {
                    move_actor(actor, Vadd(actor->pos, (FVec2){-Fx1, Fx0}));
                    FLAG_ON(actor, FLG_X_FLIP);
                    break;
                }

                case 3: {
                    move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Fx1}));
                    FLAG_ON(actor, FLG_PLAYER_DUCK);
                    break;
                }
                }
            }

            if (++VAL(actor, PLAYER_WARP_STATE) >= 60) {
                switch (VAL(warp, WARP_OUT_ANGLE)) {
                default:
                    move_actor(actor, (FVec2){VAL(warp, WARP_X) - Int2Fx(30), VAL(warp, WARP_Y)});
                    break;
                case 1:
                    move_actor(actor, (FVec2){VAL(warp, WARP_X), VAL(warp, WARP_Y) + Int2Fx(49)});
                    break;
                case 2:
                    move_actor(actor, (FVec2){VAL(warp, WARP_X) + Int2Fx(30), VAL(warp, WARP_Y)});
                    break;
                case 3:
                    move_actor(actor, (FVec2){VAL(warp, WARP_X), VAL(warp, WARP_Y) - Int2Fx(49)});
                    break;
                }
                actor->last_pos = actor->pos;
                VAL(actor, PLAYER_WARP_OUT_ANGLE) = VAL(warp, WARP_OUT_ANGLE);
                VAL(actor, PLAYER_WARP) = NULL_ACTOR;
                FLAG_ON(actor, FLG_PLAYER_WARP_OUT);

                skip_interp(actor);
                play_state_sound("warp", PLAY_POS, A_ACTOR(actor));
            }
        }

        actor->box.start.y
            = (player->powerup == POW_NONE || ANY_FLAG(actor, FLG_PLAYER_DUCK)) ? Int2Fx(-25) : Int2Fx(-51);

        goto t_skip_physics;
    }

    if (game_state->sequence.type == GS_WIN) {
        actor->vel.x = 155648;
        FLAG_OFF(actor, FLG_X_FLIP | FLG_PLAYER_JUMP | FLG_PLAYER_DUCK);
    }

    // EVENTS FROM "Level 1-1"

    // 179, 180, 181, 182
    if (!ANY_FLAG(actor, FLG_PLAYER_DUCK)) {
        if (actor->vel.x == Fx0 && !TOUCHING(actor, TOUCH_LEFT | TOUCH_RIGHT)) {
            if (ANY_INPUT(player, GI_RIGHT))
                FLAG_OFF(actor, FLG_X_FLIP);
            else if (ANY_INPUT(player, GI_LEFT))
                FLAG_ON(actor, FLG_X_FLIP);
        }

        // 181, 182, 183 (modified), 184 (modified)
        if (!ANY_INPUT(player, GI_RUN)) {
            if (ANY_INPUT(player, GI_RIGHT) && !TOUCHING(actor, TOUCH_RIGHT) && actor->vel.x >= Fx0
                && actor->vel.x < 278528)
            {
                actor->vel.x += 8192;
            }
            if (ANY_INPUT(player, GI_LEFT) && !TOUCHING(actor, TOUCH_LEFT) && actor->vel.x <= Fx0
                && actor->vel.x > -278528)
            {
                actor->vel.x -= 8192;
            }
        } else {
            if (ANY_INPUT(player, GI_RIGHT) && !TOUCHING(actor, TOUCH_RIGHT) && actor->vel.x >= Fx0
                && actor->vel.x < Int2Fx(7))
            {
                actor->vel.x = Fmin(actor->vel.x + 8192, Int2Fx(7));
            }
            if (ANY_INPUT(player, GI_LEFT) && !TOUCHING(actor, TOUCH_LEFT) && actor->vel.x <= Fx0
                && actor->vel.x > Int2Fx(-7))
            {
                actor->vel.x = Fmax(actor->vel.x - 8192, Int2Fx(-7));
            }
        }
    }

    // 185, 186
    if (!ANY_INPUT(player, GI_RIGHT) && actor->vel.x > Fx0)
        actor->vel.x = Fmax(actor->vel.x - 8192, Fx0);
    if (!ANY_INPUT(player, GI_LEFT) && actor->vel.x < Fx0)
        actor->vel.x = Fmin(actor->vel.x + 8192, Fx0);

    // 187, 188, 191, 192
    if (!ANY_FLAG(actor, FLG_PLAYER_DUCK)) {
        if (ANY_INPUT(player, GI_RIGHT) && !TOUCHING(actor, TOUCH_RIGHT) && actor->vel.x < Fx0)
            actor->vel.x = Fmin(actor->vel.x + Fmul(24576, character->steer), Fx0);
        if (ANY_INPUT(player, GI_LEFT) && !TOUCHING(actor, TOUCH_LEFT) && actor->vel.x > Fx0)
            actor->vel.x = Fmax(actor->vel.x - Fmul(24576, character->steer), Fx0);

        if (ANY_INPUT(player, GI_RIGHT) && !TOUCHING(actor, TOUCH_RIGHT) && actor->vel.x < Fx1 && actor->vel.x >= Fx0)
            actor->vel.x += Fx1;
        if (ANY_INPUT(player, GI_LEFT) && !TOUCHING(actor, TOUCH_LEFT) && actor->vel.x > -Fx1 && actor->vel.x <= Fx0)
            actor->vel.x -= Fx1;
    }

    // 193, 194
    if (!ANY_INPUT(player, GI_RUN)) {
        if (actor->vel.x < -278528)
            actor->vel.x += 8192;
        if (actor->vel.x > 278528)
            actor->vel.x -= 8192;
    }

    // 209, 210, 211, 212
    if (ANY_INPUT(player, GI_DOWN) && !ANY_INPUT(player, GI_LEFT | GI_RIGHT) && TOUCHING(actor, TOUCH_BOTTOM)
        && player->powerup != POW_NONE)
    {
        FLAG_ON(actor, FLG_PLAYER_DUCK);
    }

    // 213 (modified)
    if (ANY_FLAG(actor, FLG_PLAYER_DUCK)
        && (!TOUCHING(actor, TOUCH_BOTTOM) || !ANY_INPUT(player, GI_DOWN) || player->powerup == POW_NONE))
    {
        FLAG_OFF(actor, FLG_PLAYER_DUCK);
    }

    // 214 (modified), 215 (modified)
    if (ANY_FLAG(actor, FLG_PLAYER_DUCK)) {
        if (!ANY_FLAG(actor, FLG_X_FLIP) && actor->vel.x > Fx0)
            actor->vel.x = Fmax(actor->vel.x - 8192, Fx0);
        if (ANY_FLAG(actor, FLG_X_FLIP) && actor->vel.x < Fx0)
            actor->vel.x = Fmin(actor->vel.x + 8192, Fx0);
    }

    // 189, 190
    if (ANY_INPUT(player, GI_RIGHT) && actor->vel.x > Fx0)
        FLAG_OFF(actor, FLG_X_FLIP);
    if (ANY_INPUT(player, GI_LEFT) && actor->vel.x < Fx0)
        FLAG_ON(actor, FLG_X_FLIP);

    // 242, 243
    // Moved here to replicate jump buffer while sinking underwater, like in Clickteam.
    if (!ANY_INPUT(player, GI_JUMP) && (water == NULL || actor->pos.y < water->pos.y) && !ANY_INPUT(player, GI_DOWN)
        && TOUCHING(actor, TOUCH_BOTTOM) && ANY_FLAG(actor, FLG_PLAYER_JUMP))
    {
        FLAG_OFF(actor, FLG_PLAYER_JUMP);
    }
    if (ANY_PRESSED(player, GI_JUMP) && !TOUCHING(actor, TOUCH_BOTTOM) && actor->vel.y > Fx0)
        FLAG_ON(actor, FLG_PLAYER_JUMP);

    // 221 (modified), 222 (modified)
    if (ANY_PRESSED(player, GI_JUMP) && water != NULL && actor->pos.y >= water->pos.y && !TOUCHING(actor, TOUCH_TOP)
        && !ANY_INPUT(player, GI_DOWN))
    {
        actor->vel.y = Fmul(((actor->pos.y + actor->box.end.y) <= water->pos.y
                                || (actor->pos.y + actor->box.start.y) >= (water->pos.y + Int2Fx(16)))
                                ? Int2Fx(-3)
                                : Int2Fx(-9),
            character->jump);
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
            if (actor->vel.x > 24576)
                actor->vel.x = Fmax(actor->vel.x - FxHalf, Fx0);
            else if (actor->vel.x < -24576)
                actor->vel.x = Fmin(actor->vel.x + FxHalf, Fx0);

            if (actor->pos.y < (water->pos.y + Int2Fx(11)))
                create_actor(ACT_WATER_SPLASH, actor->pos);
        }

        FLAG_ON(actor, FLG_PLAYER_TOUCHED_WATER);
    }

    // 225 (modified), 229
    if (water != NULL && actor->pos.y >= water->pos.y)
        actor->vel.y += 6554;
    else
        actor->vel.y += Fx1;

    // 230
    if (actor->vel.y > Int2Fx(10))
        actor->vel.y = Int2Fx(10);

    // 240, 241
    if ((water == NULL || actor->pos.y < water->pos.y) && !ANY_INPUT(player, GI_DOWN) && TOUCHING(actor, TOUCH_BOTTOM)
        && ((ANY_PRESSED(player, GI_JUMP) && !ANY_FLAG(actor, FLG_PLAYER_JUMP))
            || (ANY_INPUT(player, GI_JUMP) && ANY_FLAG(actor, FLG_PLAYER_JUMP))))
    {
        actor->vel.y = Fmul(Int2Fx(-13), character->jump);
        FLAG_OFF(actor, FLG_PLAYER_JUMP);
        play_state_sound("jump", PLAY_POS, A_ACTOR(actor));
    }

    // 467, 468, 469: TODO

    // 471
    if (water != NULL && actor->pos.y > water->pos.y && actor->vel.y > Int2Fx(3))
        actor->vel.y -= Fx1;

    // 472, 473
    if (water != NULL && actor->pos.y > water->pos.y) {
        if (actor->vel.x > 245760)
            actor->vel.x -= 24576;
        if (actor->vel.x < -245760)
            actor->vel.x += 24576;
    }

    // Przejscie Etapu i Rury: TODO

    // 561, 562, 563 (modified)
    if (water != NULL && actor->pos.y > water->pos.y && (game_state->time % 5) == 0 && rng(10) == 5) {
        create_actor(ACT_BUBBLE, Vadd(actor->pos, (player->powerup == POW_NONE) ? (FVec2){Fx0, Int2Fx(-18)}
                                                                                : (FVec2){Int2Fx(2), Int2Fx(-39)}));
    }

    actor->box.start.y = (player->powerup == POW_NONE || ANY_FLAG(actor, FLG_PLAYER_DUCK)) ? Int2Fx(-25) : Int2Fx(-51);

    displace_actor(actor, Int2Fx(10), TRUE);
    if (get_actor(VAL(actor, PLAYER_WARP)) == NULL && !ANY_FLAG(actor, FLG_PLAYER_WARP_OUT))
        collide_actor(actor);

    // 218 (modified), 219 (modified), 220 (modified)
    // Moved below events 240, 241 to replicate Clickteam jump height and to prevent jump buffs against a wall.
    if (ANY_INPUT(player, GI_JUMP) && actor->vel.y < Fx0 && (water == NULL || actor->pos.y < water->pos.y)
        && !ANY_INPUT(player, GI_DOWN))
    {
        if (player->powerup != POW_GREEN_LUI && Fabs(actor->vel.x) < 40960)
            actor->vel.y -= 26214;
        else
            actor->vel.y -= FxHalf;
    }

t_skip_physics:
    const Bool warping = get_actor(VAL(actor, PLAYER_WARP)) != NULL || ANY_FLAG(actor, FLG_PLAYER_WARP_OUT);
    if (VAL(actor, PLAYER_ANIMATION) == PF_GROW
        && (((player->powerup == POW_NONE || player->powerup == POW_SUPER_MUSHROOM)
                && VAL(actor, PLAYER_FRAME) < Int2Fx(30))
            || (player->powerup != POW_NONE && player->powerup != POW_SUPER_MUSHROOM
                && VAL(actor, PLAYER_FRAME) < Int2Fx(40))))
    {
        VAL(actor, PLAYER_FRAME) += 59638;
    } else if (!TOUCHING(actor, TOUCH_BOTTOM)) {
        if (water != NULL && actor->pos.y > water->pos.y && !warping) {
            if (VAL(actor, PLAYER_ANIMATION) != PF_SWIM) {
                VAL(actor, PLAYER_ANIMATION) = PF_SWIM;
                VAL(actor, PLAYER_FRAME) = Fx0;
            }
            VAL(actor, PLAYER_FRAME) += Fclamp(Fdiv(Fabs(actor->vel.x), 819200), 9175, 13763);
        } else {
            VAL(actor, PLAYER_ANIMATION) = (actor->vel.y < Fx0) ? PF_JUMP : PF_FALL;
            VAL(actor, PLAYER_FRAME) = Fx0;
        }

        if (player->powerup == POW_GREEN_LUI && (game_state->time % 3) == 0 && !warping) {
            GameActor* effect = create_actor(ACT_PLAYER_EFFECT, actor->pos);
            if (effect != NULL) {
                effect->player = player->id;
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
    } else if (Fabs(actor->vel.x) >= 8192 || get_actor(VAL(actor, PLAYER_WARP)) != NULL
               || ANY_FLAG(actor, FLG_PLAYER_WARP_OUT))
    {
        VAL(actor, PLAYER_ANIMATION) = PF_WALK;
        VAL(actor, PLAYER_FRAME) += Fclamp(Fdiv(Fabs(actor->vel.x), 819200), 7864, 31457);
    } else {
        VAL(actor, PLAYER_ANIMATION) = PF_IDLE;
        VAL(actor, PLAYER_FRAME) = Fx0;
    }

    if (warping)
        collide_actor(actor);

    player->pos = actor->pos;

    if (actor->pos.y > (player->bounds.end.y + Int2Fx(64)))
        kill_player(actor);
}

static void post_tick(GameActor* actor) {
    GamePlayer* player = get_player(actor->player);
    if (player == NULL || !gamecontext()->players[player->id].xscroll)
        return;

    // EVENTS FROM "Level 1-1"
    // Modified to not jitter while slow walking (holding left+right).

    if (!ALL_INPUT(player, GI_LEFT | GI_RIGHT)) {
        // 532, 533
        if (ANY_INPUT(player, GI_RIGHT) && actor->vel.x > Fx0 && !TOUCHING(actor, TOUCH_RIGHT)
            && player->xscroll < Int2Fx(200))
        {
            player->xscroll = Fmin(player->xscroll + Fx1, Int2Fx(200));
        }
        if (ANY_INPUT(player, GI_LEFT) && actor->vel.x < Fx0 && !TOUCHING(actor, TOUCH_LEFT)
            && player->xscroll > Int2Fx(-200))
        {
            player->xscroll = Fmax(player->xscroll - Fx1, Int2Fx(-200));
        }
    }

    // 534 (modified), 535 (modified)
    if (!ANY_INPUT(player, GI_LEFT | GI_RIGHT) || ALL_INPUT(player, GI_LEFT | GI_RIGHT)) {
        if (player->xscroll > Fx0)
            player->xscroll = Fmax(player->xscroll - Int2Fx(2), Fx0);
        if (player->xscroll < Fx0)
            player->xscroll = Fmin(player->xscroll + Int2Fx(2), Fx0);
    }

    if (!ALL_INPUT(player, GI_LEFT | GI_RIGHT)) {
        // 536, 537
        if (ANY_INPUT(player, GI_RIGHT) && player->xscroll < Fx0)
            player->xscroll += Int2Fx(4);
        if (ANY_INPUT(player, GI_LEFT) && player->xscroll > Fx0)
            player->xscroll -= Int2Fx(4);
    }
}

static void draw(const GameActor* actor) {
    const GamePlayer* player = get_player(actor->player);
    if (player == NULL)
        return;

    batch_reset();
    batch_color(B_U4_ALPHA((player->id == localplayer()) ? 255 : 192));
    draw_actor(actor,
        get_character_sprite(gamecontext()->players[player->id].character, player->powerup, get_player_frame(actor)),
        FALSE);

    if (player->id == viewplayer())
        return;

    const char* name = get_peer_name(player_to_peer(player->id));
    if (name == NULL)
        return;

    const FVec2 ipos = get_interp(actor);
    const Sint32 nx = Fx2Int(ipos.x), ny = Fx2Int(ipos.y + actor->box.start.y) - 16;
    batch_pos(B_F3(nx, ny, Fx2Float(actor->depth)));
    batch_color(B_U4_ALPHA(192));
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_filter(TRUE);
    batch_string("main", 16.f, name);
    batch_filter(FALSE);
}

const ActorTable TAB_PLAYER = {
    .load = load,
    .create = create,
    .cleanup = cleanup,
    .tick = tick,
    .post_tick = post_tick,
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
    batch_color(
        B_U4_ALPHA(Fx2Float(VAL(actor, PLAYER_EFFECT_ALPHA)) * ((actor->player == localplayer()) ? 255.f : 192.f)));
    draw_actor(actor,
        get_character_sprite(
            VAL(actor, PLAYER_EFFECT_CHARACTER), VAL(actor, PLAYER_EFFECT_POWERUP), VAL(actor, PLAYER_EFFECT_FRAME)),
        FALSE);
}

const ActorTable TAB_PLAYER_EFFECT = {
    .create = create_effect,
    .tick = tick_effect,
    .draw = draw_effect,
};

/* ===========
   DEAD PLAYER
   =========== */

static void load_dead() {
    load_track("smw/lose", AKL_NEVER);
    load_track("smb/game_over", AKL_NEVER);
}

static void tick_dead(GameActor* actor) {
    switch (++VAL(actor, PLAYER_DEAD)) {
    default:
        break;

    case 1: {
        // !!! CLIENT-SIDE !!!
        if (actor->player == viewplayer())
            play_state_track(STS_FANFARE, "smw/lose", 0);
        // !!! CLIENT-SIDE !!!

        GameState* game_state = gamestate();
        GameSequence* sequence = &game_state->sequence;
        if (sequence->type == GS_NONE && gamecontext()->num_players > 1 && !all_players_dead()
            && game_state->clock != 0)
        {
            break;
        }

        stop_state_track(STS_MAIN);
        stop_state_track(STS_POWER);

        sequence->type = GS_LOSE;
        sequence->time = 0;
        sequence->activator = actor->player;

        break;
    }

    case 25: {
        actor->vel.y = Int2Fx(-10);
        break;
    }

    case 201: {
        GamePlayer* player = get_player(actor->player);

        GameState* game_state = gamestate();
        if (game_state->sequence.type == GS_LOSE) {
            if (gamecontext()->num_players <= 1 && player != NULL)
                --player->lives;

            if (!all_players_dead())
                game_state->flags |= GF_END;

            break;
        }

        GameActor* pawn = respawn_player(player);
        if (pawn != NULL)
            FLAG_ON(actor, FLG_DESTROY);

        break;
    }

    case 210: {
        if (all_players_dead()) {
            GameSequence* sequence = &gamestate()->sequence;
            sequence->type = GS_LOSE;
            sequence->time = 1;
            sequence->activator = actor->player;
        }

        FLAG_ON(actor, FLG_DESTROY);
        break;
    }
    }

    if (VAL(actor, PLAYER_DEAD) >= 25) {
        move_actor(actor, Vadd(actor->pos, actor->vel));
        actor->vel.y += 26214;
    }
}

static void draw_dead(const GameActor* actor) {
    const GamePlayer* player = get_player(actor->player);
    if (player == NULL)
        return;

    batch_reset();
    draw_actor(actor, get_character_sprite(gamecontext()->players[player->id].character, POW_NONE, PF_DEAD), FALSE);
}

const ActorTable TAB_PLAYER_DEAD = {
    .load = load_dead,
    .tick = tick_dead,
    .draw = draw_dead,
};
