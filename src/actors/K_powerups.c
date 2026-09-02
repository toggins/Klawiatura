#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_player.h"
#include "actors/K_points.h"
#include "actors/K_powerups.h"

static void draw_powerup(const GameActor* actor, const char* name) {
    if (ANY_FLAG(actor, FLG_POWERUP_CALAMITY))
        return;

    batch_reset();

    if (get_player(actor->player) == NULL || actor->player == localplayer()) {
        draw_actor(actor, name, FALSE);
    } else {
        batch_color(B_U4_ALPHA(192));
        draw_actor(actor, name, FALSE);

        batch_blend(BM_ADD);
        const float v = 128.f + (SDL_sinf(((float)(gamestate()->time % 30) / 30.f) * 2.f * SDL_PI_F) * 128.f);
        batch_color(B_U4_VALUE(v));
        draw_actor(actor, name, FALSE);
        batch_blend(BM_NORMAL);
    }
}

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

    grow_player(from, actor, POW_SUPER_MUSHROOM);
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
    draw_powerup(actor, fmt("items/fire_flower/%i", (VAL(actor, POWERUP_FRAME) / 100) % 4));
}

static void collide_fire_flower(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_POWERUP_CALAMITY) || actor->sprout > 0)
        return;

    grow_player(from, actor, POW_FIRE_FLOWER);
    FLAG_ON(actor, FLG_DESTROY);
}

const ActorTable TAB_FIRE_FLOWER = {
    .load = load_fire_flower,
    .create = create_fire_flower,
    .tick = tick_fire_flower,
    .draw = draw_fire_flower,
    .collide = collide_fire_flower,
};

/* =======
   STARMAN
   ======= */

static void load_starman() {
    load_sprite_num("items/starman/%u", 4, AKL_NEVER);
    load_sound("grow", AKL_NEVER);
    load_track("smw/starman", AKL_NEVER);
    load_actor(ACT_EXPLODE);
}

static void create_starman(GameActor* actor) {
    actor->box.start.x = Int2Fx(-16);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(17);
    actor->box.end.y = Fx1;
}

static void tick_starman(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_POWERUP_CALAMITY))
        return;

    VAL_TICK(actor, POWERUP_OVERLAP);
    actor->vel.y += 13107;

    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (actor->sprout > 0)
        --actor->sprout;

    if (ANY_FLAG(actor, FLG_POWERUP_SPROUTED)) {
        VAL(actor, POWERUP_FRAME) += 14;
        if (VAL(actor, POWERUP_FRAME) < 1200) {
            return;
        } else {
            // GROSS HACK: In MF, Starman sinks about 17 px down before bouncing for the first time.
            //             This sorta replicates that by making the initial bounce ~17 px lower.
            displace_actor(actor, Int2Fx(10), FALSE);
            if (TOUCHING(actor, TOUCH_BOTTOM))
                actor->vel.y = -280494;
            TOUCH_OFF(actor, TOUCH_SIDES);
            move_actor(actor, actor->last_pos);

            actor->vel.x = 163840;
            VAL(actor, POWERUP_FRAME) = 0;
            FLAG_OFF(actor, FLG_POWERUP_SPROUTED);
        }
    } else {
        VAL(actor, POWERUP_FRAME) += 49;
    }

    if (TOUCHING(actor, TOUCH_BOTTOM))
        actor->vel.y = Int2Fx(-5);

    const Fixed speed = Fabs(actor->vel.x);
    displace_actor(actor, Int2Fx(10), FALSE);
    if (actor->vel.x == Fx0) {
        if (TOUCHING(actor, TOUCH_RIGHT))
            actor->vel.x = -speed;
        if (TOUCHING(actor, TOUCH_LEFT))
            actor->vel.x = speed;
    }
}

static void draw_starman(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("items/starman/%i", (VAL(actor, POWERUP_FRAME) / 100) % 4), FALSE);
}

static void collide_starman(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_POWERUP_CALAMITY))
        return;

    if (VAL(actor, POWERUP_OVERLAP) > 0) {
        VAL(actor, POWERUP_OVERLAP) = 2;
        return;
    } else {
        VAL(actor, POWERUP_OVERLAP) = 2;
    }

    if (ANY_FLAG(actor, FLG_POWERUP_SPROUTED))
        return;

    VAL(from, PLAYER_STARMAN) = 500;
    update_player_track(get_player(from->player));

    play_state_sound("grow", PLAY_POS, A_ACTOR(from));
    FLAG_ON(actor, FLG_DESTROY);
}

const ActorTable TAB_STARMAN = {
    .load = load_starman,
    .create = create_starman,
    .tick = tick_starman,
    .draw = draw_starman,
    .collide = collide_starman,
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

static void tick_1up_mushroom(GameActor* actor) {
    VAL_TICK(actor, POWERUP_OVERLAP);
    tick_super_mushroom(actor);
}

static void draw_1up_mushroom(const GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_POWERUP_CALAMITY)) {
        batch_reset();
        draw_actor(actor, "items/mushroom/1up", FALSE);
    }
}

static void collide_1up_mushroom(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_POWERUP_CALAMITY))
        return;

    if (VAL(actor, POWERUP_OVERLAP) > 0) {
        VAL(actor, POWERUP_OVERLAP) = 2;
        return;
    } else {
        VAL(actor, POWERUP_OVERLAP) = 2;
    }

    if (actor->sprout > 0)
        return;

    give_points(actor, get_player(from->player), -1);
    FLAG_ON(actor, FLG_DESTROY);
}

const ActorTable TAB_1UP_MUSHROOM = {
    .load = load_1up_mushroom,
    .create = create_1up_mushroom,
    .tick = tick_1up_mushroom,
    .draw = draw_1up_mushroom,
    .collide = collide_1up_mushroom,
};

/* =========
   GREEN LUI
   ========= */

static void load_green_lui() {
    load_sprite_num("items/green_lui/%u", 12, AKL_NEVER);
    load_sprite_num("items/green_lui/bounce/%u", 6, AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_sprite("grow", AKL_NEVER);
}

static void create_green_lui(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-30);
    actor->box.end.x = Int2Fx(15);
    actor->box.end.y = Fx1;

    actor->vel.y = Int2Fx(-7);
}

static void tick_green_lui(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_POWERUP_CALAMITY))
        return;

    if (ANY_FLAG(actor, FLG_POWERUP_BOUNCE)) {
        VAL(actor, POWERUP_FRAME) += 44;
        if (VAL(actor, POWERUP_FRAME) >= 600) {
            VAL(actor, POWERUP_FRAME) = 0;
            FLAG_OFF(actor, FLG_POWERUP_BOUNCE);
        }
    } else {
        ++VAL(actor, POWERUP_FRAME);
    }

    displace_actor(actor, Fx0, FALSE);

    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (actor->sprout <= 0) {
        actor->vel.y += 13107;
        if (TOUCHING(actor, TOUCH_BOTTOM)) {
            actor->vel.y = Int2Fx(-7);
            VAL(actor, POWERUP_FRAME) = 0;
            FLAG_ON(actor, FLG_POWERUP_BOUNCE);

            play_state_sound("kick", PLAY_POS, A_ACTOR(actor));
        }
    }
}

static void draw_green_lui(const GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_POWERUP_CALAMITY)) {
        batch_reset();
        draw_actor(actor,
            ANY_FLAG(actor, FLG_POWERUP_BOUNCE)
                ? fmt("items/green_lui/bounce/%i", (VAL(actor, POWERUP_FRAME) / 100) % 6)
                : fmt("items/green_lui/%i", VAL(actor, POWERUP_FRAME) % 12),
            FALSE);
    }
}

static void collide_green_lui(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_POWERUP_CALAMITY) || actor->sprout > 0)
        return;

    grow_player(from, actor, POW_GREEN_LUI);
    FLAG_ON(actor, FLG_DESTROY);
}

const ActorTable TAB_GREEN_LUI = {
    .load = load_green_lui,
    .create = create_green_lui,
    .tick = tick_green_lui,
    .draw = draw_green_lui,
    .collide = collide_green_lui,
};

/* ========
   BEETROOT
   ======== */

static void load_beetroot() {
    load_sprite_num("items/beetroot/%u", 4, AKL_NEVER);
    load_sound("grow", AKL_NEVER);
}

static void create_beetroot(GameActor* actor) {
    actor->box.start.x = Int2Fx(-13);
    actor->box.start.y = Int2Fx(-32);
    actor->box.end.x = Int2Fx(14);
    actor->box.end.y = Fx1;
}

static void tick_beetroot(GameActor* actor) {
    VAL(actor, POWERUP_FRAME) += 2;
}

static void draw_beetroot(const GameActor* actor) {
    draw_powerup(actor, fmt("items/beetroot/%i", (VAL(actor, POWERUP_FRAME) / 25) % 4));
}

static void collide_beetroot(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_POWERUP_CALAMITY) || actor->sprout > 0)
        return;

    grow_player(from, actor, POW_BEETROOT);
    FLAG_ON(actor, FLG_DESTROY);
}

const ActorTable TAB_BEETROOT = {
    .load = load_beetroot,
    .create = create_beetroot,
    .tick = tick_beetroot,
    .draw = draw_beetroot,
    .collide = collide_beetroot,
};
