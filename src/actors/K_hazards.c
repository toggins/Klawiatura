#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_hazards.h"
#include "actors/K_player.h"

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
        if (any_in_range(actor->pos.x, Int2Fx(80))) {
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

/* =======
   SPAWNER
   ======= */

static void load_spawner_special(const GameActor* actor) {
    load_actor(VAL(actor, HAZARD_STATE));
}

static void create_spawner(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(40);

    VAL(actor, HAZARD_STATE) = ACT_SPINY;
    VAL(actor, HAZARD_STATE2) = VAL(actor, HAZARD_OVERLAP) = 2;
    FLAG_ON(actor, FLG_HAZARD_ACTIVE);
    FLAG_OFF(actor, FLG_VISIBLE);
}

static void tick_spawner(GameActor* actor) {
    VAL_TICK(actor, HAZARD_OVERLAP);

    const FVec2 center = Vadd(actor->pos, (FVec2){Fmul(30882, actor->box.end.x), Fmul(62259, actor->box.end.y)});
    if (!in_any_view(center, Int2Fx(-300), VEF_ALL) && VAL(actor, HAZARD_OVERLAP) <= 0 && (gamestate()->time % 50) == 0)
        create_actor(VAL(actor, HAZARD_STATE), center);
}

static void collide_spawner(GameActor* actor, GameActor* from) {
    if (from->type == VAL(actor, HAZARD_STATE))
        VAL(actor, HAZARD_OVERLAP) = VAL(actor, HAZARD_STATE2);
}

const ActorTable TAB_SPAWNER = {
    .load_special = load_spawner_special,
    .create = create_spawner,
    .tick = tick_spawner,
    .collide = collide_spawner,
};

/* ====
   LOOP
   ==== */

static void create_loop(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    VAL(actor, HAZARD_STATE) = Int2Fx(640);
    FLAG_OFF(actor, FLG_VISIBLE);
}

static void collide_loop(GameActor* actor, GameActor* from) {
    if (from->type == ACT_PLAYER)
        move_actor(from, Vsub(from->pos, (FVec2){VAL(actor, HAZARD_STATE), Fx0}));
}

const ActorTable TAB_LOOP = {
    .create = create_loop,
    .collide = collide_loop,
};

/* =============
   SPIKE CEILING
   ============= */

static void load_spike_ceiling() {
    load_sprite("markers/spike_ceiling", AKL_NEVER);
    load_sound("bang/2", AKL_NEVER);
}

static void create_spike_ceiling(GameActor* actor) {
    actor->depth = Int2Fx(-10);

    VAL(actor, HAZARD_Y) = actor->pos.y;
}

static void tick_spike_ceiling(GameActor* actor) {
    const PlayerID n = gamecontext()->num_players;

    if (!ANY_FLAG(actor, FLG_HAZARD_ACTIVE)) {
        // 4
        Bool past_start = FALSE, at_end = FALSE;

        for (PlayerID i = 0; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player != NULL && player->pos.x > Int2Fx(1000)) {
                past_start = TRUE;
                break;
            }
        }

        const LevelInfo* level_info = levelinfo();
        for (PlayerID i = 0; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player != NULL && player->pos.x >= (level_info->size.x - Int2Fx(1800))) {
                at_end = TRUE;
                break;
            }
        }

        if (past_start && !at_end)
            ++VAL(actor, HAZARD_STATE);

        // 5
        if (VAL(actor, HAZARD_STATE) > 200 && VAL(actor, HAZARD_STATE) < 300) {
            Fixed y = VAL(actor, HAZARD_Y) + Int2Fx(rng(2));
            y -= Int2Fx(rng(2));
            move_actor(actor, (FVec2){actor->pos.x, y});
        }

        // 6
        if (VAL(actor, HAZARD_STATE) > 300 && VAL(actor, HAZARD_STATE) < 400) {
            Fixed y = VAL(actor, HAZARD_Y) + Int2Fx(rng(5));
            y -= Int2Fx(rng(5));
            move_actor(actor, (FVec2){actor->pos.x, y});
        }

        // 7
        const Fixed bottom = level_info->size.y - Int2Fx(80);
        if (VAL(actor, HAZARD_STATE) > 400 && actor->pos.y < bottom) {
            move_actor(actor, Vadd(actor->pos, actor->vel));
            actor->vel.y += Fx1;
        }

        // 8
        if (actor->pos.y > bottom) {
            move_actor(actor, (FVec2){actor->pos.x, bottom});
            FLAG_ON(actor, FLG_HAZARD_ACTIVE);
            VAL(actor, HAZARD_STATE) = 0;
            actor->vel.y = Fx0;
            quake_actor(NULL, (FVec2){Int2Fx(10), 26214});

            play_state_sound("bang/2", 0, NULL);
        }
    }

    if (ANY_FLAG(actor, FLG_HAZARD_ACTIVE)) {
        // 9
        ++VAL(actor, HAZARD_STATE2);

        // 10
        if (VAL(actor, HAZARD_STATE2) > 100 && actor->pos.y > Int2Fx(64))
            move_actor(actor, Vadd(actor->pos, (FVec2){Fx0, Int2Fx(-2)}));

        // 11
        if (actor->pos.y <= Int2Fx(64)) {
            VAL(actor, HAZARD_STATE2) = 0;
            FLAG_OFF(actor, FLG_HAZARD_ACTIVE);
        }
    }

    for (PlayerID i = 0; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL && pawn->type == ACT_PLAYER && (pawn->pos.y + pawn->box.start.y) < (actor->pos.y + Fx1)
            && (pawn->pos.y + pawn->box.end.y) > (actor->pos.y - Int2Fx(478)))
        {
            kill_player(pawn);
        }
    }
}

static void draw_spike_ceiling(const GameActor* actor) {
    batch_reset();
    batch_pos(B_F3(
        Fx2Int(videostate()->camera.pos.x - F_HALF_SCREEN_WIDTH), Fx2Int(get_interp(actor).y), Fx2Float(actor->depth)));
    batch_sprite("markers/spike_ceiling");
}

const ActorTable TAB_SPIKE_CEILING = {
    .load = load_spike_ceiling,
    .create = create_spike_ceiling,
    .tick = tick_spike_ceiling,
    .draw = draw_spike_ceiling,
};

/* ==============
   ELECTRIC CORAL
   ============== */

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
