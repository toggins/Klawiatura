#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_player.h"

enum {
    VAL_SPRING_FRAME
};

#define FLG_SPRING_BOUNCE CUSTOM_FLAG(0)

static void load() {
    load_sprite("markers/spring", AKL_NEVER);
    load_sprite_num("markers/spring/%u", 5, AKL_NEVER);
    load_sound("spring", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-47);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = Fx1;
}

static void tick(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_SPRING_BOUNCE)) {
        VAL(actor, SPRING_FRAME) += 25;
        if (VAL(actor, SPRING_FRAME) >= 500) {
            VAL(actor, SPRING_FRAME) = 0;
            FLAG_OFF(actor, FLG_SPRING_BOUNCE);
        }
    }

    actor->vel.y = Fx1;
    displace_actor(actor, Fx0, FALSE);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        ANY_FLAG(actor, FLG_SPRING_BOUNCE) ? fmt("markers/spring/%u", (VAL(actor, SPRING_FRAME) / 100) % 5)
                                           : "markers/spring",
        FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER
        || (from->vel.y <= Fx0 && (!ANY_FLAG(from, FLG_PLAYER_STOMP) || TOUCHING(from, TOUCH_BOTTOM))))
    {
        return;
    }

    from->vel.y
        = Fmul((VAL(from, PLAYER_SPRING) > 0) ? Int2Fx(-19) : Int2Fx(-10), get_player_jump(get_player(from->player)));
    VAL(actor, SPRING_FRAME) = 0;
    FLAG_ON(actor, FLG_SPRING_BOUNCE);

    play_state_sound("spring", PLAY_POS, A_ACTOR(actor));
}

const ActorTable TAB_SPRING = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};
