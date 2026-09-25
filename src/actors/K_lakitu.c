#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_lakitu.h"
#include "actors/K_player.h"
#include "actors/K_spiny.h"

static void load() {
    load_sprite("enemies/lakitu", AKL_NEVER);
    load_sprite_num("enemies/lakitu/%u", 16, AKL_NEVER);
    load_sprite_num("enemies/lakitu/cloud/%u", 12, AKL_NEVER);
    load_sprite_num("enemies/lakitu/in/%u", 11, AKL_NEVER);
    load_sprite_num("enemies/lakitu/out/%u", 11, AKL_NEVER);
    load_sprite("enemies/lakitu/dead", AKL_NEVER);
    load_sound_num("vo/lakitu%u", 3, AKL_NEVER);
    load_sound("stomp", AKL_NEVER);
    load_sound("kick", AKL_NEVER);
    load_actor(ACT_SPINY_EGG);
    load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-47);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;

    VAL(actor, LAKITU_SPEED) = Int2Fx(9);
    VAL(actor, LAKITU_THROW_SPEED) = 1;
}

static void tick(GameActor* actor) {
    if (VAL(actor, LAKITU_SPAWN_START) != VAL(actor, LAKITU_SPAWN_END)) {
        for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player == NULL)
                continue;

            const GameActor* pawn = get_actor(player->actor);
            if (pawn != NULL && pawn->type == ACT_PLAYER
                && (pawn->pos.x + pawn->box.end.x) > VAL(actor, LAKITU_SPAWN_START)
                && (pawn->pos.x + pawn->box.start.x) < VAL(actor, LAKITU_SPAWN_END))
            {
                FLAG_ON(actor, FLG_LAKITU_SPAWNED);
                break;
            }
        }

        if (!ANY_FLAG(actor, FLG_LAKITU_SPAWNED))
            return;
    }

    switch (VAL(actor, LAKITU_ANIMATION)) {
    default:
        break;

    case LA_BLINK: {
        if (++VAL(actor, LAKITU_FRAME) >= 16)
            VAL(actor, LAKITU_ANIMATION) = LA_IDLE;

        break;
    }

    case LA_CLOUD: {
        if (++VAL(actor, LAKITU_FRAME) >= 12)
            VAL(actor, LAKITU_ANIMATION) = LA_IDLE;

        break;
    }

    case LA_IN: {
        if (VAL(actor, LAKITU_FRAME) < 10)
            ++VAL(actor, LAKITU_FRAME);

        break;
    }

    case LA_OUT: {
        if (++VAL(actor, LAKITU_FRAME) >= 11)
            VAL(actor, LAKITU_ANIMATION) = LA_IDLE;

        break;
    }
    }

    move_actor(actor, Vadd(actor->pos, actor->vel));

    const LevelInfo* level_info = levelinfo();
    if (actor->pos.y > (level_info->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    const GameActor* nearest = nearest_player_actor(actor->pos);
    if (nearest != NULL) {
        if (nearest->pos.x < (level_info->size.x - Int2Fx(1000))) {
            if ((gamestate()->time % 5) == 0) {
                if (actor->pos.x > (nearest->pos.x + Int2Fx(50)) && actor->vel.x > -VAL(actor, LAKITU_SPEED))
                    actor->vel.x -= Fx1;
                if (actor->pos.x < (nearest->pos.x - Int2Fx(50)) && actor->vel.x < VAL(actor, LAKITU_SPEED))
                    actor->vel.x += Fx1;
            }

            if (!ANY_FLAG(actor, FLG_LAKITU_FAST) && actor->pos.x < (nearest->pos.x + Int2Fx(100))
                && actor->pos.x > (nearest->pos.x - Int2Fx(100)))
            {
                if (actor->pos.x > nearest->pos.x && actor->vel.x < Int2Fx(-2))
                    actor->vel.x += Fx1;
                if (actor->pos.x < nearest->pos.x && actor->vel.x > Int2Fx(4))
                    actor->vel.x -= Fx1;
            }
        } else {
            if (actor->vel.x > Int2Fx(-2))
                actor->vel.x -= Fx1;
        }
    }

    if ((gamestate()->time % 5) == 0 && VAL(actor, LAKITU_ANIMATION) == LA_IDLE) {
        switch (rng(20)) {
        default:
            break;

        case 10: {
            VAL(actor, LAKITU_ANIMATION) = LA_BLINK;
            VAL(actor, LAKITU_FRAME) = 0;

            break;
        }

        case 15: {
            VAL(actor, LAKITU_ANIMATION) = LA_CLOUD;
            VAL(actor, LAKITU_FRAME) = 0;

            break;
        }
        }
    }

    if (((nearest == NULL) ? Fx0 : nearest->pos.x) < (level_info->size.x - Int2Fx(1000))) {
        if (in_any_view(actor->pos, Fx0, VEF_ALL)) {
            if (VAL(actor, LAKITU_THROW_SPEED) > 0)
                VAL(actor, LAKITU_THROW) += VAL(actor, LAKITU_THROW_SPEED);

            if (VAL(actor, LAKITU_AGGRO) > 0
                && ((VAL(actor, LAKITU_AGGRO_START) == VAL(actor, LAKITU_AGGRO_END))
                    || ((actor->pos.x + actor->box.end.x) > VAL(actor, LAKITU_AGGRO_START)
                        && (actor->pos.x + actor->box.start.x) < VAL(actor, LAKITU_AGGRO_END))))
            {
                VAL(actor, LAKITU_THROW) += VAL(actor, LAKITU_AGGRO);
            }
        }

        if (VAL(actor, LAKITU_THROW_SPEED2) > 0 && in_any_view(actor->pos, Int2Fx(-200), VEF_ALL))
            VAL(actor, LAKITU_THROW) += VAL(actor, LAKITU_THROW_SPEED2);
    }

    if (VAL(actor, LAKITU_THROW) > (200 + VAL(actor, LAKITU_THROW_DELAY))) {
        VAL(actor, LAKITU_THROW) = 0;
        VAL(actor, LAKITU_THROW_DELAY) = rng(100);

        GameActor* egg = create_actor(
            ACT_SPINY_EGG, Vadd(actor->pos, (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? -Fx1 : Fx1, Int2Fx(-38)}));
        if (egg != NULL) {
            egg->vel.y = Int2Fx(-3);
            if (ANY_FLAG(actor, FLG_LAKITU_SPAWNED))
                FLAG_ON(egg, FLG_SPINY_TEMP);
        }

        const Sint32 r = rng(3);
        // !!! CLIENT-SIDE !!!
        play_state_sound(fmt("vo/lakitu%i", r), PLAY_POS, A_ACTOR(actor));
        // !!! CLIENT-SIDE !!!
    }

    const ActorValue throw_start = VAL(actor, LAKITU_THROW_DELAY) + 160, throw_end = throw_start + 30;
    if (VAL(actor, LAKITU_THROW) > throw_start && VAL(actor, LAKITU_THROW) <= throw_end
        && VAL(actor, LAKITU_ANIMATION) != LA_IN)
    {
        VAL(actor, LAKITU_ANIMATION) = LA_IN;
        VAL(actor, LAKITU_FRAME) = 0;
    }
    if (VAL(actor, LAKITU_THROW) >= throw_end && VAL(actor, LAKITU_ANIMATION) != LA_OUT) {
        VAL(actor, LAKITU_ANIMATION) = LA_OUT;
        VAL(actor, LAKITU_FRAME) = 0;
    }
}

static void draw(const GameActor* actor) {
    if (VAL(actor, LAKITU_SPAWN_START) != VAL(actor, LAKITU_SPAWN_END) && !ANY_FLAG(actor, FLG_LAKITU_SPAWNED))
        return;

    batch_reset();

    const char* sprite = "enemies/lakitu";
    switch (VAL(actor, LAKITU_ANIMATION)) {
    default:
        break;
    case LA_BLINK:
        sprite = fmt("enemies/lakitu/%i", VAL(actor, LAKITU_FRAME) % 16);
        break;
    case LA_CLOUD:
        sprite = fmt("enemies/lakitu/cloud/%i", VAL(actor, LAKITU_FRAME) % 12);
        break;
    case LA_IN:
        sprite = fmt("enemies/lakitu/in/%i", VAL(actor, LAKITU_FRAME) % 11);
        break;
    case LA_OUT:
        sprite = fmt("enemies/lakitu/out/%i", VAL(actor, LAKITU_FRAME) % 11);
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

static void draw_dead(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "enemies/lakitu/dead", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (VAL(actor, LAKITU_SPAWN_START) != VAL(actor, LAKITU_SPAWN_END) && !ANY_FLAG(actor, FLG_LAKITU_SPAWNED))
        return;

    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        if (VAL(from, PLAYER_STARMAN) > 0)
            player_starman(from, actor);
        else if (check_stomp(actor, from, Int2Fx(-16), 100, FALSE))
            kill_enemy(actor, from, FALSE);

        break;
    }

    case ACT_KOOPA_SHELL:
    case ACT_CODER_CLONE_RUN:
    case ACT_BUZZY_SHELL: {
        if (!hit_shell(actor, from))
            turn_enemy(actor);

        break;
    }

    case ACT_FIREBALL_PROJECTILE: {
        hit_fireball(actor, from, 100);
        break;
    }

    case ACT_BEETROOT_PROJECTILE: {
        hit_beetroot(actor, from, 100);
        break;
    }

    case ACT_HAMMER_PROJECTILE: {
        hit_hammer(actor, from, 100);
        break;
    }

    case ACT_BULLET_PROJECTILE: {
        hit_bullet(actor, from, 100);
        break;
    }
    }
}

const ActorTable TAB_LAKITU = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .draw_dead = draw_dead,
    .collide = collide,
};
