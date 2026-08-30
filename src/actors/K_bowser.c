#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_bowser.h"
#include "actors/K_player.h"
#include "actors/K_projectiles.h"

/* ======
   BOWSER
   ====== */

static void load() {
    load_sprite_num("enemies/bowser/%u", 2, AKL_NEVER);
    load_sprite_num("enemies/bowser/jump/%u", 3, AKL_NEVER);
    load_sprite_num("enemies/bowser/fire/%u", 6, AKL_NEVER);
    load_sprite_num("enemies/bowser/fire/end/%u", 2, AKL_NEVER);
    load_sprite("ui/bowser", AKL_NEVER);
    load_sprite("ui/bowser/bar", AKL_NEVER);
    load_sound("bowser/fire", AKL_NEVER);
    load_sound("bowser/hurt", AKL_NEVER);
    load_sound("bowser/dead", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_BOWSER_FIRE_PROJECTILE);
    load_actor(ACT_BOWSER_DEAD);
}

static void load_special(const GameActor* actor) {
    // TODO
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-30);
    actor->box.start.y = Int2Fx(-69);
    actor->box.end.x = Int2Fx(30);
    actor->box.end.y = Fx1;

    actor->depth = 2;

    VAL(actor, BOWSER_HEALTH) = 5;
    VAL(actor, BOWSER_SPEED) = Int2Fx(2);
    VAL(actor, BOWSER_JUMP_CHANCE) = 5;
    VAL(actor, BOWSER_JUMP_SPEED) = Int2Fx(-5);
    VAL(actor, BOWSER_ATTACK_CHANCE) = 3;
    VAL(actor, BOWSER_ATTACK_SPEED) = Fx1;
    VAL(actor, BOWSER_PROJECTILE_SPEED) = Fx1;
    VAL(actor, BOWSER_HURT_DURATION) = 100;

    VAL(actor, BOWSER_Y) = actor->pos.y;
}

static void pre_tick(GameActor* actor) {
    switch (VAL(actor, BOWSER_ANIMATION)) {
    default:
        break;

    case BA_FIRE: {
        VAL(actor, BOWSER_FRAME) += Fmul(9175, VAL(actor, BOWSER_ATTACK_SPEED));
        if (VAL(actor, BOWSER_FRAME) < Int2Fx(6))
            return;

        VAL(actor, BOWSER_ANIMATION) = BA_FIRE_END;
        VAL(actor, BOWSER_FRAME) = Fx0;

        GameActor* fire = create_actor(ACT_BOWSER_FIRE_PROJECTILE,
            Vadd(actor->pos, (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? Int2Fx(-17) : Int2Fx(17), Int2Fx(-38)}));
        if (fire != NULL) {
            fire->vel.x
                = Fmul(ANY_FLAG(actor, FLG_X_FLIP) ? Int2Fx(-4) : Int2Fx(4), VAL(actor, BOWSER_PROJECTILE_SPEED));

            if (ANY_FLAG(actor, FLG_BOWSER_CHARGE)) {
                VAL(fire, PROJECTILE_Y) = VAL(actor, BOWSER_Y) - Int2Fx(27);
                const Sint32 r = rng(3);
                if (r > 0)
                    VAL(fire, PROJECTILE_Y) -= Int2Fx(4) + (r * Int2Fx(32));

                FLAG_ON(fire, FLG_PROJECTILE_ALT);
            } else {
                VAL(fire, PROJECTILE_Y) = VAL(actor, BOWSER_Y) - Int2Fx(31) - (rng(3) * Int2Fx(32));
            }

            FLAG_ON(fire, actor->flags & FLG_X_FLIP);
        }

        play_state_sound("bowser/fire", PLAY_POS, A_ACTOR(actor));
        return;
    }

    case BA_FIRE_END: {
        VAL(actor, BOWSER_FRAME) += 13107;
        if (VAL(actor, BOWSER_FRAME) >= Int2Fx(2)) {
            VAL(actor, BOWSER_ANIMATION) = BA_IDLE;
            VAL(actor, BOWSER_FRAME) = Fx0;
            break;
        }

        return;
    }
    }

    if (TOUCHING(actor, TOUCH_BOTTOM)) {
        if (VAL(actor, BOWSER_ANIMATION) == BA_JUMP) {
            VAL(actor, BOWSER_ANIMATION) = BA_IDLE;
            VAL(actor, BOWSER_FRAME) = Fx0;
            return;
        }
    } else {
        switch (VAL(actor, BOWSER_ANIMATION)) {
        default:
            break;

        case BA_IDLE: {
            VAL(actor, BOWSER_ANIMATION) = BA_JUMP;
            VAL(actor, BOWSER_FRAME) = Fx0;
            return;
        }

        case BA_JUMP: {
            if (VAL(actor, BOWSER_FRAME) < Int2Fx(2))
                VAL(actor, BOWSER_FRAME) += FxHalf;
            return;
        }
        }
    }

    VAL(actor, BOWSER_ANIMATION) = BA_IDLE;
    VAL(actor, BOWSER_FRAME) += 19661;
}

static void tick(GameActor* actor) {
    // EVENTS FROM "Level 1 - 4"

    // 800
    const GameState* game_state = gamestate();
    if ((game_state->time % 50) == 0 && ANY_FLAG(actor, FLG_BOWSER_ACTIVE))
        VAL(actor, BOWSER_MOVE) = rng(64);

    // 801 (modified)
    if (!ANY_FLAG(actor, FLG_BOWSER_ACTIVE) && in_any_view(actor->pos, Int2Fx(-32), FALSE)) {
        FLAG_ON(actor, FLG_BOWSER_ACTIVE);

        // !!! CLIENT-SIDE !!!
        videostate()->bowser.active = TRUE;
        // !!! CLIENT-SIDE !!!
    }

    // 802
    if (ANY_FLAG(actor, FLG_BOWSER_ACTIVE) && !ANY_FLAG(actor, FLG_BOWSER_RIGHT) && (game_state->time % 50) == 0)
        VAL(actor, BOWSER_MOVE) = rng(128);

    // 803, 804 (modified)
    if (VAL(actor, BOWSER_MOVE) > 0) {
        actor->vel.x = ANY_FLAG(actor, FLG_BOWSER_RIGHT) ? VAL(actor, BOWSER_SPEED) : -VAL(actor, BOWSER_SPEED);
        --VAL(actor, BOWSER_MOVE);
    } else {
        actor->vel.x = Fx0;
    }

    // 805, 806 (modified)
    const LevelInfo* level_info = levelinfo();
    if ((actor->pos.x + actor->box.start.x) < level_info->bowser_bounds.x || TOUCHING(actor, TOUCH_LEFT))
        FLAG_ON(actor, FLG_BOWSER_RIGHT);
    if ((actor->pos.x + actor->box.end.x) > level_info->bowser_bounds.y || TOUCHING(actor, TOUCH_RIGHT))
        FLAG_OFF(actor, FLG_BOWSER_RIGHT);

    // 807 (modified)
    if (VAL(actor, BOWSER_JUMP_CHANCE) > 0 && (game_state->time % VAL(actor, BOWSER_JUMP_CHANCE)) == 0
        && ANY_FLAG(actor, FLG_BOWSER_ACTIVE))
    {
        VAL(actor, BOWSER_JUMP) = rng(20);
    }

    // 811 (modified)
    // Moved above 808 for full jump speed on the first frame.
    actor->vel.y += 8738;

    // 808 (modified)
    if (VAL(actor, BOWSER_JUMP) == 10 && TOUCHING(actor, TOUCH_BOTTOM)) {
        actor->vel.y = VAL(actor, BOWSER_JUMP_SPEED);
        TOUCH_OFF(actor, TOUCH_BOTTOM);
    }

    // 813, 814
    const FVec2 ppos = nearest_player_pos(actor->pos);
    if (actor->pos.x > ppos.x)
        FLAG_ON(actor, FLG_X_FLIP);
    if (actor->pos.x < ppos.x)
        FLAG_OFF(actor, FLG_X_FLIP);

    // 817
    if (ANY_FLAG(actor, FLG_BOWSER_ACTIVE))
        VAL(actor, BOWSER_ATTACK) += rng(VAL(actor, BOWSER_ATTACK_CHANCE));

    // 818
    if (VAL(actor, BOWSER_ATTACK) > 150) {
        VAL(actor, BOWSER_ATTACK) = 0;
        VAL(actor, BOWSER_ANIMATION) = BA_FIRE;
        VAL(actor, BOWSER_FRAME) = Fx0;
    }

    // 835
    if (VAL(actor, BOWSER_HITS) < -4) {
        --VAL(actor, BOWSER_HEALTH);
        VAL(actor, BOWSER_HURT) = VAL(actor, BOWSER_HURT_DURATION);
        VAL(actor, BOWSER_HITS) = 0;

        play_state_sound("bowser/hurt", PLAY_POS, A_ACTOR(actor));
    }

    // 836, 837, 838
    if (VAL(actor, BOWSER_HURT) > 0) {
        --VAL(actor, BOWSER_HURT);

        if (VAL(actor, BOWSER_FADE) < 100 && !ANY_FLAG(actor, FLG_BOWSER_FADE_IN))
            VAL(actor, BOWSER_FADE) += 5;
        if (VAL(actor, BOWSER_FADE) >= 100)
            FLAG_ON(actor, FLG_BOWSER_FADE_IN);
    }

    // 839 (modified)
    if (VAL(actor, BOWSER_FADE) > 0 && (ANY_FLAG(actor, FLG_BOWSER_FADE_IN) || VAL(actor, BOWSER_HURT) <= 0))
        VAL(actor, BOWSER_FADE) -= 5;

    // 840
    if (VAL(actor, BOWSER_HURT) > 0 && VAL(actor, BOWSER_FADE) <= 0)
        FLAG_OFF(actor, FLG_BOWSER_FADE_IN);

    // 848
    if (VAL(actor, BOWSER_HEALTH) <= 0) {
        if (get_num_actors(ACT_BOWSER) <= 1)
            set_sequence(GS_BOWSER_END, get_player(actor->player), 0);

        GameActor* dead = create_actor(ACT_BOWSER_DEAD, actor->pos);
        if (dead != NULL) {
            FLAG_ON(dead, actor->flags & FLG_X_FLIP);
            align_interp(dead, actor);
        }
        FLAG_ON(actor, FLG_DESTROY);

        play_state_sound("bowser/dead", PLAY_POS, A_ACTOR(actor));
        return;
    }

    displace_actor(actor, Int2Fx(10), FALSE);
}

static void draw(const GameActor* actor) {
    batch_reset();
    batch_color(B_U4_ALPHA((1.f - ((float)VAL(actor, BOWSER_FADE) / 128.f)) * 255.f));

    const char* sprite = NULL;
    switch (VAL(actor, BOWSER_ANIMATION)) {
    default:
        sprite = fmt("enemies/bowser/%i", Fx2Int(VAL(actor, BOWSER_FRAME)) % 2);
        break;
    case BA_FIRE:
        sprite = fmt("enemies/bowser/fire/%i", Fx2Int(VAL(actor, BOWSER_FRAME)) % 6);
        break;
    case BA_JUMP:
        sprite = fmt("enemies/bowser/jump/%i", Fx2Int(VAL(actor, BOWSER_FRAME)) % 3);
        break;
    case BA_FIRE_END:
        sprite = fmt("enemies/bowser/fire/end/%i", Fx2Int(VAL(actor, BOWSER_FRAME)) % 2);
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

static void draw_hud(const GameActor* actor) {
    videostate()->bowser.health += VAL(actor, BOWSER_HEALTH);
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (VAL(from, PLAYER_STARMAN) > 0)
            break;

        if (from->pos.y < (actor->pos.y - Int2Fx(40))) {
            if (VAL(actor, BOWSER_HURT) > 0 || (from->vel.y < Fx0 && !ANY_FLAG(from, FLG_PLAYER_STOMP)))
                break;

            actor->player = from->player;
            --VAL(actor, BOWSER_HEALTH);
            VAL(actor, BOWSER_HURT) = VAL(actor, BOWSER_HURT_DURATION);
            VAL(actor, BOWSER_HITS) = 0;

            GamePlayer* player = get_player(from->player);
            if (player != NULL)
                player->score += 100;
            from->vel.y = Fmul(Int2Fx(-8), get_player_jump(player));
            VAL(from, PLAYER_SLIP) = ANY_FLAG(from, FLG_X_FLIP) ? -8 : 8;

            play_state_sound("bowser/hurt", PLAY_POS, A_ACTOR(actor));
        } else {
            hit_player(from);
        }

        break;
    }

    case ACT_FIREBALL_PROJECTILE: {
        if (get_player(from->player) == NULL || VAL(actor, BOWSER_HURT) > 0)
            break;

        actor->player = from->player;
        --VAL(actor, BOWSER_HITS);

        FLAG_ON(from, FLG_PROJECTILE_HIT | FLG_PROJECTILE_HIT_ENEMY);

        play_state_sound("kick", PLAY_POS, A_ACTOR(from));
        break;
    }

    case ACT_BEETROOT_PROJECTILE: {
        if (get_player(from->player) == NULL || VAL(actor, BOWSER_HURT) > 0)
            break;

        if (VAL(from, PROJECTILE_COOLDOWN) > 0) {
            VAL(from, PROJECTILE_COOLDOWN) = 2;
            break;
        }

        actor->player = from->player;
        --VAL(actor, BOWSER_HITS);

        VAL(from, PROJECTILE_COOLDOWN) = 2;
        FLAG_ON(from, FLG_PROJECTILE_HIT | FLG_PROJECTILE_HIT_BOSS);

        play_state_sound("kick", PLAY_POS, A_ACTOR(from));
        break;
    }
    }
}

const ActorTable TAB_BOWSER = {
    .load = load,
    .load_special = load_special,
    .create = create,
    .pre_tick = pre_tick,
    .tick = tick,
    .draw = draw,
    .draw_hud = draw_hud,
    .collide = collide,
};

/* ============
   BOWSER, DEAD
   ============ */

static void load_dead() {
    load_sprite_num("enemies/bowser/dead/%u", 2, AKL_NEVER);
    load_sound("bowser/fall", AKL_NEVER);
    load_sound("bowser/lava", AKL_NEVER);
    load_actor(ACT_LAVA_BUBBLE);
}

static void create_dead(GameActor* actor) {
    actor->box.start.x = Int2Fx(-32);
    actor->box.start.y = Int2Fx(-71);
    actor->box.end.x = Int2Fx(31);
    actor->box.end.y = Fx1;
}

static void tick_dead(GameActor* actor) {
    move_actor(actor, Vadd(actor->pos, actor->vel));

    if (++VAL(actor, BOWSER_DEAD) > 100) {
        if (VAL(actor, BOWSER_DEAD) == 101)
            play_state_sound("bowser/fall", PLAY_POS, A_ACTOR(actor));

        actor->vel.y += 8738;
    }

    if (VAL(actor, BOWSER_DEAD_LAVA) > 0) {
        if ((actor->pos.y + actor->box.end.y) > VAL(actor, BOWSER_DEAD_Y)
            && (actor->pos.y + actor->box.start.y) < (VAL(actor, BOWSER_DEAD_Y) + Int2Fx(51)))
        {
            if (actor->vel.y > Fx1) {
                actor->vel.y -= Fx1;
                if (actor->vel.y < Fx1)
                    actor->vel.y = Fx1;
            }

            if (VAL(actor, BOWSER_DEAD_LAVA) < 50) {
                FVec2 bpos = actor->pos;
                bpos.x += Int2Fx(rng(24));
                bpos.x -= Int2Fx(rng(24));
                GameActor* bubble = create_actor(ACT_LAVA_BUBBLE, bpos);
                if (bubble != NULL) {
                    bubble->vel.x += Int2Fx(rng(3));
                    bubble->vel.x -= Int2Fx(rng(3));
                    bubble->vel.y = Int2Fx(-2) - Int2Fx(rng(4));
                }
            }

            ++VAL(actor, BOWSER_DEAD_LAVA);
        } else {
            VAL(actor, BOWSER_DEAD_LAVA) = 0;
        }
    }

    if (below_nearest_bounds(actor->pos, Int2Fx(100))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    collide_actor(actor);
}

static void draw_dead(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/bowser/dead/%i", (VAL(actor, BOWSER_DEAD) / 2) % 2), FALSE);
}

const ActorTable TAB_BOWSER_DEAD = {
    .load = load_dead,
    .create = create_dead,
    .tick = tick_dead,
    .draw = draw_dead,
};
