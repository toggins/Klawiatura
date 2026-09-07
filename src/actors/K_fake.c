#include "K_audio.h"
#include "K_video.h"

#include "actors/K_player.h"

enum {
    VAL_BRICK_Y,
    VAL_BRICK_FRAME,
    VAL_BRICK_RESPAWN,
};

#define FLG_BRICK_FALL CUSTOM_FLAG(0)

static void load() {
    load_sprite("enemies/fake_brick", AKL_NEVER);
    load_sound("stun", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-31);
    actor->box.start.y = Int2Fx(-14);
    actor->box.end.x = Int2Fx(33);
    actor->box.end.y = Int2Fx(18);

    VAL(actor, BRICK_Y) = actor->pos.y;
}

static void tick(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_BRICK_FALL)) {
        const FVec2 ppos = nearest_player_pos(actor->pos);
        if (actor->pos.x < (ppos.x + Int2Fx(80)) && actor->pos.x > (ppos.x - Int2Fx(80))) {
            FLAG_ON(actor, FLG_BRICK_FALL);

            play_state_sound("stun", PLAY_POS, A_ACTOR(actor));
        } else {
            return;
        }
    }

    ++VAL(actor, BRICK_FRAME);

    move_actor(actor, Vadd(actor->pos, actor->vel));
    actor->vel.y += 13107;

    if (below_nearest_view(actor->pos, Int2Fx(32))) {
        if (gamecontext()->num_players <= 1) {
            FLAG_ON(actor, FLG_DESTROY);
        } else if (++VAL(actor, BRICK_RESPAWN) >= 150) {
            move_actor(actor, (FVec2){actor->pos.x, VAL(actor, BRICK_Y)});
            actor->vel.y = Fx0;
            VAL(actor, BRICK_FRAME) = VAL(actor, BRICK_RESPAWN) = 0;
            FLAG_OFF(actor, FLG_BRICK_FALL);
        }
    }
}

static void draw(const GameActor* actor) {
    batch_reset();
    if (VAL(actor, BRICK_RESPAWN) >= 100 && (gamestate()->time % 2) == 0) {
        batch_pos(B_F3(Fx2Int(get_interp(actor).x), Fx2Int(VAL(actor, BRICK_Y)), Fx2Float(actor->depth)));
        batch_sprite("enemies/fake_brick");
    } else {
        batch_angle((float)((int)(VAL(actor, BRICK_FRAME) / 2)) * 0.125f * SDL_PI_F);
        draw_actor(actor, "enemies/fake_brick", FALSE);
    }
}

static void collide(GameActor* actor, GameActor* from) {
    (void)actor;

    hit_player(from);
}

const ActorTable TAB_FAKE_BRICK = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};
