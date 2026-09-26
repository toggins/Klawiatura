#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_player.h"

enum {
    VAL_HAZARD_STATE,
    VAL_HAZARD_Y,
    VAL_HAZARD_RESPAWN,
    VAL_HAZARD_OVERLAP,
    VAL_HAZARD_FRAME,
};

#define FLG_HAZARD_ACTIVE CUSTOM_FLAG(0)

/* ====
   SINK
   ==== */

static void load_sink() {
    load_actor(ACT_SINK_BUBBLE);
}

static void create_sink(GameActor* actor) {
    actor->vel.x = Int2Fx(2);
    actor->vel.y = Fx1;
}

static void tick_sink(GameActor* actor) {
    const GameState* game_state = gamestate();
    if (in_any_view(actor->pos, Int2Fx(-128), VEF_ALL) && (game_state->time % 15) == 0) {
        FVec2 bpos = actor->pos;
        bpos.y -= Int2Fx(200) + Int2Fx(rng(32));
        bpos.x += Int2Fx(rng(64));
        bpos.x -= Int2Fx(rng(64));
        GameActor* bubble = create_actor(ACT_SINK_BUBBLE, bpos);
        if (bubble != NULL)
            bubble->vel.y = Fdouble(actor->vel.y);
    }

    const GameActor* water = get_actor(game_state->water);
    if (water == NULL)
        return;

    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER || pawn->pos.y <= water->pos.y
            || actor->pos.x <= (pawn->pos.x - Int2Fx(128)) || actor->pos.x >= (pawn->pos.x + Int2Fx(128)))
        {
            continue;
        }

        if (actor->pos.x < (pawn->pos.x - actor->vel.x)) {
            const FVec2 ppos = Vadd(pawn->pos, (FVec2){-actor->vel.x, Fx0});
            if (!touching_solid(Radd(pawn->box, ppos), SOL_SOLID))
                move_actor(pawn, ppos);
        }
        if (actor->pos.x > (pawn->pos.x + actor->vel.x)) {
            const FVec2 ppos = Vadd(pawn->pos, (FVec2){actor->vel.x, Fx0});
            if (!touching_solid(Radd(pawn->box, ppos), SOL_SOLID))
                move_actor(pawn, ppos);
        }

        const FVec2 ppos = Vadd(pawn->pos, (FVec2){Fx0, actor->vel.y});
        if (!touching_solid(Radd(pawn->box, ppos), SOL_SOLID))
            move_actor(pawn, ppos);
    }
}

const ActorTable TAB_SINK = {
    .load = load_sink,
    .create = create_sink,
    .tick = tick_sink,
};

/* ==========
   FAKE BRICK
   ========== */

static void load_fake_brick() {
    load_sprite("enemies/fake_brick", AKL_NEVER);
    load_sound("stun", AKL_NEVER);
}

static void create_fake_brick(GameActor* actor) {
    actor->box.start.x = Int2Fx(-31);
    actor->box.start.y = Int2Fx(-14);
    actor->box.end.x = Int2Fx(33);
    actor->box.end.y = Int2Fx(18);

    VAL(actor, HAZARD_Y) = actor->pos.y;
}

static void tick_fake_brick(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_HAZARD_ACTIVE)) {
        const FVec2 ppos = nearest_player_pos(actor->pos);
        if (actor->pos.x < (ppos.x + Int2Fx(80)) && actor->pos.x > (ppos.x - Int2Fx(80))) {
            FLAG_ON(actor, FLG_HAZARD_ACTIVE);

            play_state_sound("stun", PLAY_POS, A_ACTOR(actor));
        } else {
            return;
        }
    }

    ++VAL(actor, HAZARD_FRAME);

    move_actor(actor, Vadd(actor->pos, actor->vel));
    actor->vel.y += 13107;

    if (below_nearest_view(actor->pos, Int2Fx(32))) {
        if (gamecontext()->num_players <= 1) {
            FLAG_ON(actor, FLG_DESTROY);
        } else if (++VAL(actor, HAZARD_RESPAWN) >= 150) {
            move_actor(actor, (FVec2){actor->pos.x, VAL(actor, HAZARD_Y)});
            actor->vel.y = Fx0;
            VAL(actor, HAZARD_FRAME) = VAL(actor, HAZARD_RESPAWN) = 0;
            FLAG_OFF(actor, FLG_HAZARD_ACTIVE);
        }
    }
}

static void draw_fake_brick(const GameActor* actor) {
    batch_reset();
    if (VAL(actor, HAZARD_RESPAWN) >= 100 && (gamestate()->time % 2) == 0) {
        batch_pos(B_F3(Fx2Int(get_interp(actor).x), Fx2Int(VAL(actor, HAZARD_Y)), Fx2Float(actor->depth)));
        batch_sprite("enemies/fake_brick");
    } else {
        batch_angle((float)((int)(VAL(actor, HAZARD_FRAME) / 2)) * 0.125f * SDL_PI_F);
        draw_actor(actor, "enemies/fake_brick", FALSE);
    }
}

static void collide_fake_brick(GameActor* actor, GameActor* from) {
    (void)actor;

    hit_player(from);
}

const ActorTable TAB_FAKE_BRICK = {
    .load = load_fake_brick,
    .create = create_fake_brick,
    .tick = tick_fake_brick,
    .draw = draw_fake_brick,
    .collide = collide_fake_brick,
};

/* =====
   CORAL
   ===== */

static void load_coral() {
    load_sprite_num("enemies/coral/%u", 22, AKL_NEVER);
}

static void create_coral(GameActor* actor) {
    actor->box.end.x = Int2Fx(28);
    actor->box.end.y = Int2Fx(30);

    actor->depth = 1310719;
}

static void tick_coral(GameActor* actor) {
    ++VAL(actor, HAZARD_FRAME);
    VAL_TICK(actor, HAZARD_OVERLAP);
}

static void draw_coral(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("enemies/coral/%i", (VAL(actor, HAZARD_FRAME) / 2) % 22), FALSE);
}

static void collide_coral(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER)
        return;

    if (VAL(actor, HAZARD_OVERLAP) > 0) {
        VAL(actor, HAZARD_OVERLAP) = 2;
        return;
    }

    VAL(actor, HAZARD_OVERLAP) = 2;
    hit_player(from);
}

const ActorTable TAB_ELECTRIC_CORAL = {
    .load = load_coral,
    .create = create_coral,
    .tick = tick_coral,
    .draw = draw_coral,
    .collide = collide_coral,
};
