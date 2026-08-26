#include "K_audio.h"

#include "actors/K_enemies.h"
#include "actors/K_koopa.h"
#include "actors/K_player.h"
#include "actors/K_points.h"
#include "actors/K_projectiles.h"

void move_enemy(GameActor* actor, FVec2 speed, Bool edge) {
    if (actor->pos.y > (levelinfo()->size.y + Int2Fx(32))) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (!ANY_FLAG(actor, FLG_ENEMY_ACTIVE) && box_in_any_view(Radd(actor->box, actor->pos))) {
        actor->vel.x = ANY_FLAG(actor, FLG_X_FLIP) ? -speed.x : speed.x;
        FLAG_ON(actor, FLG_ENEMY_ACTIVE);
    }

    if (edge) {
        const Fixed x1 = ANY_FLAG(actor, FLG_X_FLIP) ? (actor->pos.x + actor->box.start.x - Fx1)
                                                     : (actor->pos.x + actor->box.end.x);
        const Fixed y1 = actor->pos.y + actor->box.start.y;
        const Fixed x2 = x1 + Fx1;
        const Fixed y2 = actor->pos.y + actor->box.end.y + Fx1;
        if (!touching_solid((FRect){x1, y1, x2, y2}, SOL_SOLID | SOL_SLOPE)) {
            if (actor->vel.x < Fx0) {
                actor->vel.x = speed.x;
                FLAG_OFF(actor, FLG_X_FLIP);
            } else if (actor->vel.x > Fx0) {
                actor->vel.x = -speed.x;
                FLAG_ON(actor, FLG_X_FLIP);
            }
        }
    }

    actor->vel.y += speed.y;
    displace_actor(actor, Int2Fx(10), FALSE);

    collide_actor(actor);
    VAL_TICK(actor, ENEMY_TURN);

    if (TOUCHING(actor, TOUCH_LEFT | TOUCH_RIGHT)) {
        actor->vel.x = TOUCHING(actor, TOUCH_RIGHT) ? -speed.x : speed.x;
        if (actor->vel.x < Fx0)
            FLAG_ON(actor, FLG_X_FLIP);
        else
            FLAG_OFF(actor, FLG_X_FLIP);
    }
}

void turn_enemy(GameActor* actor) {
    if (VAL(actor, ENEMY_TURN) > 0) {
        ++VAL(actor, ENEMY_TURN);
        return;
    }

    actor->vel.x = -actor->vel.x;
    if (actor->vel.x < Fx0)
        FLAG_ON(actor, FLG_X_FLIP);
    else if (actor->vel.x > Fx0)
        FLAG_OFF(actor, FLG_X_FLIP);

    VAL(actor, ENEMY_TURN) = 2;
}

GameActor* kill_enemy(GameActor* actor, GameActor* from, Bool kick) {
    if (actor == NULL)
        return NULL;

    switch (actor->type) {
    default:
        break;

    case ACT_PIRANHA_PLANT:
    case ACT_ROTODISC: {
        if (kick)
            play_state_sound("kick", PLAY_POS, A_ACTOR(actor));

        FLAG_ON(actor, FLG_DESTROY);
        return NULL;
    }
    }

    GameActor* dead = create_actor(ACT_DEAD, actor->pos);
    if (dead != NULL) {
        VAL(dead, DEAD_TYPE) = actor->type;
        dead->flags = actor->flags & ~(FLG_DESTROY | FLG_X_FLIP | FLG_FREEZE);

        if (kick) {
            const Fixed r = Int2Fx(rng(5));
            const Fixed dir = 77208 + Fmul(r, 12868);
            dead->vel.x = Fmul(Fcos(dir), Int2Fx(3));
            dead->vel.y = Fmul(Fsin(dir), Int2Fx(-3));
            play_state_sound("kick", PLAY_POS, A_ACTOR(actor));
        }

        align_interp(dead, actor);
    }

    FLAG_ON(actor, FLG_DESTROY);
    mark_ambush_winner(from);
    return dead;
}

Bool check_stomp(GameActor* actor, GameActor* from, Fixed offset, Sint32 points) {
    if (actor == NULL || from == NULL || from->type != ACT_PLAYER || VAL(from, PLAYER_STARMAN) > 0)
        return FALSE;

    if (from->pos.y < (actor->pos.y + offset) && (from->vel.y >= Fx0 || ANY_FLAG(from, FLG_PLAYER_STOMP))) {
        GamePlayer* player = get_player(from->player);
        const GameCharacter* character
            = (player == NULL) ? NULL : get_character(gamecontext()->players[player->id].character);
        from->vel.y = Fmul((player != NULL && ANY_INPUT(player, GI_JUMP)) ? Int2Fx(-13) : Int2Fx(-8),
            (character == NULL) ? Fx1 : character->jump);
        FLAG_ON(from, FLG_PLAYER_STOMP);

        give_points(actor, player, points);
        play_state_sound("stomp", PLAY_POS, A_ACTOR(actor));
        return TRUE;
    }

    return FALSE;
}

Bool maybe_hit_player(GameActor* actor, GameActor* from) {
    if (actor == NULL || from == NULL || from->type != ACT_PLAYER)
        return FALSE;

    if (VAL(from, PLAYER_STARMAN) > 0) {
        player_starman(from, actor);
        return FALSE;
    }

    return hit_player(from);
}

void hit_bump(GameActor* actor, GameActor* from, Sint32 points) {
    if (actor == NULL || from == NULL)
        return;

    GamePlayer* player = get_player(from->player);
    if (player == NULL)
        return;

    give_points(actor, player, points);
    kill_enemy(actor, from, TRUE);
}

Bool hit_shell(GameActor* actor, GameActor* from) {
    if (actor == NULL || from == NULL || from->vel.x == Fx0)
        return FALSE;

    if (!in_any_view(actor->pos, Int2Fx(-32), FALSE))
        return TRUE;

    if (actor->type == ACT_KOOPA_SHELL && actor->vel.x != Fx0) {
        VAL(actor, SHELL_COMBO) = VAL(from, SHELL_COMBO) = 0;
        give_points(from, get_player(from->player), 100);
        kill_enemy(from, from, TRUE);
    }

    Sint32 points = 0;
    switch (VAL(from, SHELL_COMBO)++) {
    case 0:
        points = 100;
        break;
    case 1:
        points = 200;
        break;
    case 2:
        points = 500;
        break;
    case 3:
        points = 1000;
        break;
    case 4:
        points = 2000;
        break;
    case 5:
        points = 5000;
        break;

    default: {
        points = -1;
        VAL(from, SHELL_COMBO) = 0;
        break;
    }
    }
    give_points(actor, get_player(from->player), points);

    kill_enemy(actor, from, TRUE);
    return TRUE;
}

void hit_fireball(GameActor* actor, GameActor* from, Sint32 points) {
    if (actor == NULL || from == NULL)
        return;

    GamePlayer* player = get_player(from->player);
    if (player == NULL)
        return;

    give_points(actor, player, points);
    kill_enemy(actor, from, TRUE);
    FLAG_ON(from, FLG_PROJECTILE_HIT | FLG_PROJECTILE_HIT_ENEMY);
}

void hit_beetroot(GameActor* actor, GameActor* from, Sint32 points) {
    hit_fireball(actor, from, points);
}

void mark_ambush_winner(GameActor* actor) {
    // TODO
}

void increase_ambush() {
    // TODO
}

void decrease_ambush() {
    // TODO
}

/* ==========
   DEAD ENEMY
   ========== */

static void tick_dead(GameActor* actor) {
    move_actor(actor, Vadd(actor->pos, actor->vel));
    actor->vel.y += 13107;

    if (below_nearest_bounds(actor->pos, Int2Fx(32)))
        FLAG_ON(actor, FLG_DESTROY);
}

const ActorTable TAB_DEAD = {
    .tick = tick_dead,
    .draw = draw_dead_actor,
};
