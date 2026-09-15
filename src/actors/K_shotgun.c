#include "K_audio.h"
#include "K_video.h"

#include "actors/K_effects.h"
#include "actors/K_player.h"
#include "actors/K_points.h"

enum {
    VAL_SHOTGUN_STATE,
};

#define FLG_SHOTGUN_DROPPED CUSTOM_FLAG(0)

static void load() {
    load_sprite("items/shotgun", AKL_NEVER);
    load_sound("weapon", AKL_NEVER);
    load_sound("shotgun", AKL_NEVER);
    load_actor(ACT_POINTS);
    load_actor(ACT_EXPLODE2);
    load_actor(ACT_BULLET_PROJECTILE);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-28);
    actor->box.start.y = Int2Fx(-9);
    actor->box.end.x = Int2Fx(28);
    actor->box.end.y = Int2Fx(6);
}

static void cleanup(GameActor* actor) {
    const GamePlayer* player = get_player(actor->player);
    if (player == NULL)
        return;

    GameActor* pawn = get_actor(player->actor);
    if (pawn != NULL && pawn->type == ACT_PLAYER)
        FLAG_OFF(pawn, FLG_PLAYER_WEAPON);
}

static void tick(GameActor* actor) {
    const GamePlayer* player = get_player(actor->player);
    if (player == NULL) {
        displace_actor(actor, Fx0, FALSE);
        actor->vel.y += 26214;
    } else {
        const GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER) {
            actor->player = NULL_PLAYER;
            FLAG_ON(actor, FLG_SHOTGUN_DROPPED);

            actor->vel.x = ANY_FLAG(actor, FLG_X_FLIP) ? -1 : 1;
            actor->vel.y = Fx0;
            displace_actor(actor, Fx0, FALSE);
        }
    }

    if ((ANY_FLAG(actor, FLG_SHOTGUN_DROPPED) && !in_any_view(actor->pos, Int2Fx(-128), VEF_ALL))
        || (player == NULL && actor->pos.y > (levelinfo()->size.y + Int2Fx(32))))
    {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    if (VAL(actor, SHOTGUN_STATE) > 0) {
        if (++VAL(actor, SHOTGUN_STATE) >= 30)
            VAL(actor, SHOTGUN_STATE) = 0;
    }
}

static void shoot_bullet(GameActor* actor, FVec2 bpos, Fixed yvel) {
    GameActor* bullet = create_actor(ACT_BULLET_PROJECTILE, bpos);
    if (bullet == NULL)
        return;

    bullet->player = actor->player;
    bullet->vel.x = ANY_FLAG(actor, FLG_X_FLIP) ? (Int2Fx(-28) - Int2Fx(rng(5))) : (Int2Fx(28) + Int2Fx(rng(5)));
    bullet->vel.y = yvel;
}

static void post_tick(GameActor* actor) {
    const GamePlayer* player = get_player(actor->player);
    if (player == NULL)
        return;

    const GameActor* pawn = get_actor(player->actor);
    if (pawn != NULL && pawn->type == ACT_PLAYER) {
        move_actor(actor,
            Vadd(pawn->pos,
                (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? Int2Fx(-14) : Int2Fx(14),
                    (player->powerup == POW_NONE || ANY_FLAG(pawn, FLG_PLAYER_DUCK)) ? Int2Fx(-10) : Int2Fx(-22)}));
        actor->depth = pawn->depth - 1;
        actor->flags = (actor->flags & ~FLG_X_FLIP) | (pawn->flags & FLG_X_FLIP);
    }

    if (ANY_PRESSED(player, GI_FIRE) && VAL(actor, SHOTGUN_STATE) <= 0) {
        ++VAL(actor, SHOTGUN_STATE);

        GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL && pawn->type == ACT_PLAYER) {
            pawn->vel.x -= ANY_FLAG(actor, FLG_X_FLIP) ? Int2Fx(-5) : Int2Fx(5);
            if (pawn->vel.y < Fx0)
                pawn->vel.y = Fdiv(Int2Fx(-13), get_player_jump(player));
        }

        const FVec2 bpos
            = Vadd(actor->pos, (FVec2){ANY_FLAG(actor, FLG_X_FLIP) ? Int2Fx(-27) : Int2Fx(27), Int2Fx(-6)});

        shoot_bullet(actor, bpos, Int2Fx(-4));
        shoot_bullet(actor, bpos, -98304);
        shoot_bullet(actor, bpos, 98304);
        shoot_bullet(actor, bpos, Int2Fx(4));

        GameActor* explode = create_actor(ACT_EXPLODE2, bpos);
        if (explode != NULL) {
            explode->depth = actor->depth - 1;
            VAL(explode, EFFECT_SPEED) = 125;
        }

        play_state_sound("shotgun", PLAY_POS, A_ACTOR(actor));
    }
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, "items/shotgun", FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || get_player(actor->player) != NULL || ANY_FLAG(from, FLG_PLAYER_WEAPON))
        return;

    GameActor* shotgun = (gamecontext()->num_players > 1 && !ANY_FLAG(actor, FLG_SHOTGUN_DROPPED))
                             ? create_actor(ACT_SHOTGUN, actor->pos)
                             : actor;
    if (shotgun == NULL)
        return;

    shotgun->player = from->player;
    FLAG_OFF(shotgun, FLG_SHOTGUN_DROPPED);
    FLAG_ON(from, FLG_PLAYER_WEAPON);
    give_points(shotgun, get_player(from->player), 2000);

    play_state_sound("weapon", PLAY_POS, A_ACTOR(shotgun));
}

const ActorTable TAB_SHOTGUN = {
    .load = load,
    .create = create,
    .cleanup = cleanup,
    .tick = tick,
    .post_tick = post_tick,
    .draw = draw,
    .collide = collide,
};
