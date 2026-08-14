#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_player.h"
#include "actors/K_points.h"
#include "actors/K_powerups.h"

/* ==============
   SUPER MUSHROOM
   ============== */

static void load_super_mushroom() {
    load_sprite("items/mushroom/super", AKL_NEVER);
    load_sound("grow", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_super_mushroom(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;
}

static void tick_super_mushroom(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_POWERUP_CALAMITY))
        return;

    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    const Fixed xvel = Fabs(actor->vel.x);
    actor->vel.y += 19005;

    displace_actor(actor, Int2Fx(10), FALSE);
    if (actor->vel.x == Fx0) {
        if (TOUCHING(actor, TOUCH_LEFT))
            actor->vel.x = xvel;
        else if (TOUCHING(actor, TOUCH_RIGHT))
            actor->vel.x = -xvel;
    }
}

static void draw_super_mushroom(const GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_POWERUP_CALAMITY)) {
        batch_reset();
        draw_actor(actor, "items/mushroom/super", FALSE);
    }
}

static void collide_super_mushroom(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_POWERUP_CALAMITY))
        return;

    GamePlayer* player = get_player(from->player);
    if (player == NULL)
        return;

    if (player->powerup == POW_NONE) {
        player->powerup = POW_SUPER_MUSHROOM;
        player->score += 1000;
        VAL(from, PLAYER_ANIMATION) = PF_GROW;
        VAL(from, PLAYER_FRAME) = Fx0;
    } else {
        give_points(actor, player, 1000);
    }

    play_state_sound("grow", PLAY_POS, A_ACTOR(from));
    FLAG_ON(actor, FLG_DESTROY);
}

const ActorTable TAB_SUPER_MUSHROOM = {
    .load = load_super_mushroom,
    .create = create_super_mushroom,
    .tick = tick_super_mushroom,
    .draw = draw_super_mushroom,
    .collide = collide_super_mushroom,
};

/* ===========
   FIRE FLOWER
   =========== */

static void load_fire_flower() {
    load_sprite_num("items/fire_flower/%u", 4, AKL_NEVER);
    load_sound("grow", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_fire_flower(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(17);
    actor->box.end.y = Fx1;
}

static void tick_fire_flower(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_POWERUP_CALAMITY))
        VAL(actor, POWERUP_FRAME) += 27;
}

static void draw_fire_flower(const GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_POWERUP_CALAMITY)) {
        batch_reset();
        draw_actor(actor, fmt("items/fire_flower/%i", (VAL(actor, POWERUP_FRAME) / 100) % 4), FALSE);
    }
}

static void collide_fire_flower(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_POWERUP_CALAMITY) || actor->sprout > 0)
        return;

    GamePlayer* player = get_player(from->player);
    if (player == NULL)
        return;

    if (player->powerup != POW_FIRE_FLOWER) {
        player->powerup = (player->powerup == POW_NONE && ANY_FLAG(actor, FLG_POWERUP_SPROUTED)) ? POW_SUPER_MUSHROOM
                                                                                                 : POW_FIRE_FLOWER;
        player->score += 1000;
        VAL(from, PLAYER_ANIMATION) = PF_GROW;
        VAL(from, PLAYER_FRAME) = Fx0;
    } else {
        give_points(NULL, player, 1000);
    }

    play_state_sound("grow", PLAY_POS, A_ACTOR(from));
    FLAG_ON(actor, FLG_DESTROY);
}

const ActorTable TAB_FIRE_FLOWER = {
    .load = load_fire_flower,
    .create = create_fire_flower,
    .tick = tick_fire_flower,
    .draw = draw_fire_flower,
    .collide = collide_fire_flower,
};

/* ============
   1UP MUSHROOM
   ============ */

static void load_1up_mushroom() {
    load_sprite("items/mushroom/1up", AKL_NEVER);
    load_actor(ACT_POINTS);
}

static void create_1up_mushroom(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-29);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;
}

static void draw_1up_mushroom(const GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_POWERUP_CALAMITY)) {
        batch_reset();
        draw_actor(actor, "items/mushroom/1up", FALSE);
    }
}

static void collide_1up_mushroom(GameActor* actor, GameActor* from) {
    if ((from->type == ACT_PLAYER || from->type == ACT_PLAYER_EFFECT) && !ANY_FLAG(actor, FLG_POWERUP_CALAMITY)) {
        give_points(actor, get_player(from->player), -1);
        FLAG_ON(actor, FLG_DESTROY);
    }
}

const ActorTable TAB_1UP_MUSHROOM = {
    .load = load_1up_mushroom,
    .create = create_1up_mushroom,
    .tick = tick_super_mushroom,
    .draw = draw_1up_mushroom,
    .collide = collide_1up_mushroom,
};
