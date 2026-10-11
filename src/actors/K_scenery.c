#include "K_audio.h"
#include "K_cmd.h"
#include "K_string.h"
#include "K_tick.h"
#include "K_video.h"

#include "actors/K_effects.h"

enum {
    VAL_SCENERY_ANIMATION,
    VAL_SCENERY_FRAME,
    VAL_SCENERY_ANGLE,
    VAL_SCENERY_ALPHA,
    VAL_SCENERY_SCALE,
    VAL_SCENERY_SPEED,
    VAL_SCENERY_X,
    VAL_SCENERY_Y,
};

#define FLG_SCENERY_ACTIVE CUSTOM_FLAG(0)
#define FLG_SCENERY_ALT CUSTOM_FLAG(1)
#define FLG_SCENERY_SECRET CUSTOM_FLAG(2)

/* ====
   BUSH
   ==== */

static void load_bush() {
    load_sprite_num("scenery/bush/%u", 3, AKL_NEVER);
}

static void create_bush(GameActor* actor) {
    actor->depth = Int2Fx(31);
}

static void draw_bush(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/bush/%i", ((gamestate()->time * 7) / 50) % 3), FALSE);
}

const ActorTable TAB_BUSH = {
    .load = load_bush,
    .create = create_bush,
    .draw = draw_bush,
};

/* =====
   CLOUD
   ===== */

static void load_cloud() {
    load_sprite_num("scenery/cloud/%u", 3, AKL_NEVER);
    load_sprite("effects/glow", AKL_NEVER);
}

static void create_cloud(GameActor* actor) {
    actor->depth = Int2Fx(33);
}

static void tick_cloud(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_SCENERY_ALT))
        return;

    if (!ANY_FLAG(actor, FLG_SCENERY_ACTIVE)) {
        VAL(actor, SCENERY_X) = actor->pos.x;
        VAL(actor, SCENERY_Y) = actor->pos.y;
        VAL(actor, SCENERY_ANGLE) = rng(360) * 1144;
        FLAG_ON(actor, FLG_SCENERY_ACTIVE);

        skip_interp(actor);
    }

    move_actor(actor, (FVec2){actor->pos.x, VAL(actor, SCENERY_Y) + Fmul(Int2Fx(5), Fcos(VAL(actor, SCENERY_ANGLE)))});
    VAL(actor, SCENERY_ANGLE) = Fmod(VAL(actor, SCENERY_ANGLE) + 1144, Fx2Pi);
    move_actor(actor, (FVec2){VAL(actor, SCENERY_X) + Fmul(VAL(actor, SCENERY_SPEED), -Fsin(VAL(actor, SCENERY_ANGLE))),
                          actor->pos.y});
    VAL(actor, SCENERY_SPEED) = Fmin(VAL(actor, SCENERY_SPEED) + Int2Fx(rng(2)), Int2Fx(20));
}

static void draw_cloud(const GameActor* actor) {
    batch_reset();

    if (CLIENT.extra_effects && VAL(actor, SCENERY_ANIMATION) <= 0) {
        batch_offset(B_F3_XY(-31.f, -25.f));
        draw_actor(actor, "effects/glow", FALSE);
        batch_offset(B_F3_0);
    }

    switch (VAL(actor, SCENERY_ANIMATION)) {
    default:
        break;
    case 1:
        batch_color(B_U4(181, 173, 173, 255));
        break;
    case 2:
        batch_color(B_U4(123, 99, 99, 255));
        break;
    }

    draw_actor(actor, fmt("scenery/cloud/%i", ((gamestate()->time * 2) / 25) % 3), FALSE);
}

const ActorTable TAB_CLOUD = {
    .load = load_cloud,
    .create = create_cloud,
    .tick = tick_cloud,
    .draw = draw_cloud,
};

/* ======
   CLOUDS
   ====== */

static void load_clouds() {
    load_sprite("scenery/clouds", AKL_NEVER);
    load_sprite("effects/clouds_glow", AKL_NEVER);
}

static void draw_clouds(const GameActor* actor) {
    batch_reset();
    const FVec2 ipos = get_interp(actor);
    const Sint32 ax = Fx2Int(ipos.x) + ((Sint32)(Fx2Float(actor->vel.x) * screenticks()) % 64), ay = Fx2Int(ipos.y);
    batch_pos(B_F3(ax, ay, Fx2Float(actor->depth)));
    batch_tile(B_B2(TRUE, FALSE));
    const Sint32 w = Fx2Int(levelinfo()->size.x) + 128;

    if (CLIENT.extra_effects)
        batch_rectangle("effects/clouds_glow", B_F2(w, 74.f));

    batch_rectangle("scenery/clouds", B_F2(w, 64.f));
    batch_tile(B_B2_FALSE);
}

const ActorTable TAB_CLOUDS = {
    .load = load_clouds,
    .create = create_cloud,
    .draw = draw_clouds,
};

/* ==========
   LAMP LIGHT
   ========== */

static void load_lamp_light() {
    load_sprite_num("scenery/lamp/light/%u", 6, AKL_NEVER);
}

static void create_lamp_light(GameActor* actor) {
    actor->depth = Int2Fx(-33);

    VAL(actor, SCENERY_ALPHA) = 50;
}

static void tick_lamp_light(GameActor* actor) {
    VAL(actor, SCENERY_ALPHA) += rng(5);
    VAL(actor, SCENERY_ALPHA) -= rng(5);

    if (VAL(actor, SCENERY_ALPHA) > 100 || VAL(actor, SCENERY_ALPHA) < 20)
        VAL(actor, SCENERY_ALPHA) = 50;
}

static void draw_lamp_light(const GameActor* actor) {
    batch_reset();
    batch_color(B_U4_ALPHA((1.f - ((float)VAL(actor, SCENERY_ALPHA) / 128.f)) * 255.f));
    draw_actor(actor, fmt("scenery/lamp/light/%i", ((gamestate()->time * 2) / 5) % 6), FALSE);
}

const ActorTable TAB_LAMP_LIGHT = {
    .load = load_lamp_light,
    .create = create_lamp_light,
    .tick = tick_lamp_light,
    .draw = draw_lamp_light,
};

/* ============
   TUBE BUBBLES
   ============ */

static void load_tube_bubbles() {
    load_actor(ACT_TUBE_BUBBLE);
}

static void tick_tube_bubbles(GameActor* actor) {
    if (!in_any_view(actor->pos, Int2Fx(-32), VEF_ALL) || (gamestate()->time % 5) > 0 || rng(11) != 10)
        return;

    Sint32 r = rng(10);
    r -= rng(10);
    GameActor* bubble = create_actor(ACT_TUBE_BUBBLE, Vadd(actor->pos, (FVec2){Int2Fx(r), Int2Fx(-4)}));
    if (bubble != NULL)
        bubble->vel.y = -8192 - (rng(60) * 8192);
}

const ActorTable TAB_TUBE_BUBBLES = {
    .load = load_tube_bubbles,
    .tick = tick_tube_bubbles,
};

/* =========
   WATERFALL
   ========= */

static void load_waterfall() {
    load_sprite_num("scenery/waterfall/%u", 4, AKL_NEVER);
}

static void create_waterfall(GameActor* actor) {
    actor->depth = Int2Fx(189);
}

static void draw_waterfall(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/waterfall/%i", ((gamestate()->time * 41) / 100) % 4), FALSE);
}

const ActorTable TAB_WATERFALL = {
    .load = load_waterfall,
    .create = create_waterfall,
    .draw = draw_waterfall,
};

/* ========
   LAVAFALL
   ======== */

static void load_lavafall() {
    load_sprite_num("scenery/lavafall/%u", 10, AKL_NEVER);
}

static void create_lavafall(GameActor* actor) {
    actor->depth = Int2Fx(40);
}

static void draw_lavafall(const GameActor* actor) {
    batch_reset();

    const Sint32 ax = Fx2Int(get_interp(actor).x), ay = (Sint32)SDL_fmodf(screenticks() * 6.f, 32.f);
    const float az = Fx2Float(actor->depth);
    const char* sprite = fmt("scenery/lavafall/%i", gamestate()->time % 10);

    for (Sint32 i = -32, n = Fx2Int(levelinfo()->size.y); i < n; i += 32) {
        batch_pos(B_F3(ax, i + ay, az));
        batch_sprite(sprite);
    }
}

const ActorTable TAB_LAVAFALL = {
    .load = load_lavafall,
    .create = create_lavafall,
    .draw = draw_lavafall,
};

/* ============
   LAVA BUBBLER
   ============ */

static void load_lava_bubbler() {
    load_actor(ACT_LAVA_BUBBLE);
}

static void tick_lava_bubbler(GameActor* actor) {
    if (((gamestate()->time * 2) % 5) > 1 || !in_any_view(actor->pos, Int2Fx(-32), VEF_ALL))
        return;

    FVec2 bpos = actor->pos;
    bpos.x += Int2Fx(rng(24));
    bpos.x -= Int2Fx(rng(24));

    GameActor* bubble = create_actor(ACT_LAVA_BUBBLE, bpos);
    if (bubble == NULL)
        return;

    bubble->vel.x += Int2Fx(rng(3));
    bubble->vel.x -= Int2Fx(rng(3));
    bubble->vel.y = Int2Fx(-2) - Int2Fx(rng(4));
}

const ActorTable TAB_LAVA_BUBBLER = {
    .load = load_lava_bubbler,
    .tick = tick_lava_bubbler,
};

/* ==========
   CLOUD FACE
   ========== */

static void load_cloud_face() {
    load_sprite("scenery/cloud/face", AKL_NEVER);
    load_sprite_num("scenery/cloud/face/%u", 14, AKL_NEVER);
    load_sprite_num("scenery/cloud/face/alt/%u", 2, AKL_NEVER);
}

static void create_cloud_face(GameActor* actor) {
    actor->depth = Int2Fx(69);
}

static void tick_cloud_face(GameActor* actor) {
    switch (VAL(actor, SCENERY_ANIMATION)) {
    default:
        break;

    case 1: {
        if (++VAL(actor, SCENERY_FRAME) >= 14)
            VAL(actor, SCENERY_ANIMATION) = VAL(actor, SCENERY_FRAME) = 0;

        break;
    }

    case 2: {
        if (++VAL(actor, SCENERY_FRAME) >= 50)
            VAL(actor, SCENERY_ANIMATION) = VAL(actor, SCENERY_FRAME) = 0;

        break;
    }
    }

    if ((gamestate()->time % 5) == 0 && in_any_view(actor->pos, Int2Fx(-32), VEF_ALL)
        && VAL(actor, SCENERY_ANIMATION) == 0)
    {
        switch (rng(20)) {
        default:
            break;

        case 10: {
            VAL(actor, SCENERY_ANIMATION) = 1;
            VAL(actor, SCENERY_FRAME) = 0;
            break;
        }

        case 15: {
            VAL(actor, SCENERY_ANIMATION) = 2;
            VAL(actor, SCENERY_FRAME) = 0;
            break;
        }
        }
    }
}

static void draw_cloud_face(const GameActor* actor) {
    batch_reset();

    const char* sprite = "scenery/cloud/face";
    switch (VAL(actor, SCENERY_ANIMATION)) {
    default:
        break;
    case 1:
        sprite = fmt("scenery/cloud/face/%i", VAL(actor, SCENERY_FRAME) % 14);
        break;
    case 2:
        sprite = fmt("scenery/cloud/face/alt/%i", (VAL(actor, SCENERY_FRAME) / 25) % 2);
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

const ActorTable TAB_CLOUD_FACE = {
    .load = load_cloud_face,
    .create = create_cloud_face,
    .tick = tick_cloud_face,
    .draw = draw_cloud_face,
};

/* ==============
   STARLAND LIGHT
   ============== */

static void load_starland_light() {
    load_sprite_num("scenery/tree/starland/light/%u", 3, AKL_NEVER);
}

static void create_starland_light(GameActor* actor) {
    actor->depth = 2097151;
}

static void draw_starland_light(const GameActor* actor) {
    batch_reset();
    batch_color(B_U4_ALPHA(135));
    draw_actor(actor, fmt("scenery/tree/starland/light/%i", (gamestate()->time / 2) % 3), FALSE);
}

const ActorTable TAB_STARLAND_LIGHT = {
    .load = load_starland_light,
    .create = create_starland_light,
    .draw = draw_starland_light,
};

/* =============
   STARLAND GLOW
   ============= */

static void load_starland_glow() {
    load_sprite_num("scenery/tree/starland/glow/%u", 3, AKL_NEVER);
}

static void create_starland_glow(GameActor* actor) {
    actor->depth = 1310719;
}

static void draw_starland_glow(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/tree/starland/glow/%i", (gamestate()->time / 2) % 3), FALSE);
}

const ActorTable TAB_STARLAND_GLOW = {
    .load = load_starland_glow,
    .create = create_starland_glow,
    .draw = draw_starland_glow,
};

/* ====
   STAR
   ==== */

static void load_star() {
    load_sprite_num("scenery/star/%u", 4, AKL_NEVER);
}

static void create_star(GameActor* actor) {
    actor->depth = Int2Fx(197);

    VAL(actor, SCENERY_ALPHA) = 100;
}

static void tick_star(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_SCENERY_ACTIVE)) {
        if (--VAL(actor, SCENERY_ALPHA) <= 0)
            FLAG_ON(actor, FLG_DESTROY);

        return;
    }

    if ((gamestate()->time % 10) > 0)
        return;

    Bool stay = TRUE;
    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL && pawn->type == ACT_PLAYER && pawn->pos.x > (levelinfo()->size.x - Int2Fx(2000))) {
            stay = FALSE;
            break;
        }
    }
    if (stay)
        return;

    GameActor* star = NULL;
    FOR_EACH_ACTOR (star) {
        if (star->type == ACT_STAR && !ANY_FLAG(star, FLG_SCENERY_ACTIVE)) {
            if (star->id == actor->id)
                break;
            else
                return;
        }
    }

    FOR_EACH_ACTOR (star) {
        if (star->type == ACT_STAR && rng(2) <= 0)
            break;
    }
    if (star != NULL && star->type == ACT_STAR)
        FLAG_ON(star, FLG_SCENERY_ACTIVE);
}

static void draw_star(const GameActor* actor) {
    batch_reset();

    const Fixed cx = videostate()->camera.pos.x - F_HALF_SCREEN_WIDTH;
    const FVec2 ipos = get_interp(actor);
    Fixed ax = ipos.x - cx + ((Fixed)screenticks() * actor->vel.x);
    while (ax < Int2Fx(-32))
        ax += F_SCREEN_WIDTH + Int2Fx(64);
    while (ax > (F_SCREEN_WIDTH + Int2Fx(32)))
        ax -= F_SCREEN_WIDTH + Int2Fx(64);

    batch_pos(B_F3(Fx2Int(cx + ax), Fx2Int(ipos.y), Fx2Float(actor->depth)));
    batch_color(B_U4_ALPHA(((float)VAL(actor, SCENERY_ALPHA) / 100.f) * 135.f));
    batch_sprite(fmt("scenery/star/%i", gamestate()->time % 4));
}

const ActorTable TAB_STAR = {
    .load = load_star,
    .create = create_star,
    .tick = tick_star,
    .draw = draw_star,
};

/* =====================
   SHOOTING STAR SPAWNER
   ===================== */

static void load_shooting_star_spawner() {
    load_actor(ACT_SHOOTING_STAR);
}

static void tick_shooting_star_spawner(GameActor* actor) {
    (void)actor;

    if ((gamestate()->time % 5) > 0)
        return;

    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER || rng(20) != 10)
            continue;

        FVec2 spos = get_player_view(player);
        spos.x += Int2Fx(100) + Int2Fx(rng(800));
        spos.y -= Int2Fx(32);

        GameActor* star = create_actor(ACT_SHOOTING_STAR, spos);
        if (star == NULL)
            continue;

        const Fixed dir = 244491 + (rng(4) * 12868);
        const Fixed speed = 163840 + (rng(60) * 8192);
        star->vel.x = Fmul(speed, Fcos(dir));
        star->vel.y = Fmul(speed, -Fsin(dir));
    }
}

const ActorTable TAB_SHOOTING_STAR_SPAWNER = {
    .load = load_shooting_star_spawner,
    .tick = tick_shooting_star_spawner,
};

/* =============
   SHOOTING STAR
   ============= */

static void load_shooting_star() {
    load_sprite("scenery/shooting_star", AKL_NEVER);
}

static void create_shooting_star(GameActor* actor) {
    actor->depth = -3276799;
}

static void tick_shooting_star(GameActor* actor) {
    VAL(actor, SCENERY_ANGLE) = Fmod(VAL(actor, SCENERY_ANGLE) + 12868, Fx2Pi);
    move_actor(actor, Vadd(actor->pos, actor->vel));

    if (below_nearest_view(actor->pos, Int2Fx(32)))
        FLAG_ON(actor, FLG_DESTROY);
}

static void draw_shooting_star(const GameActor* actor) {
    batch_reset();
    batch_angle(Fx2Float(VAL(actor, SCENERY_ANGLE)));
    draw_actor(actor, "scenery/shooting_star", FALSE);
}

const ActorTable TAB_SHOOTING_STAR = {
    .load = load_shooting_star,
    .create = create_shooting_star,
    .tick = tick_shooting_star,
    .draw = draw_shooting_star,
};

/* ===========
   BOUNCY TREE
   =========== */

static void load_bouncy_tree() {
    load_sprite_num("scenery/tree/bouncy/%u", 8, AKL_NEVER);
}

static void create_bouncy_tree(GameActor* actor) {
    actor->depth = Int2Fx(32);
}

static void tick_bouncy_tree(GameActor* actor) {
    VAL(actor, SCENERY_FRAME) += 16;
    while (VAL(actor, SCENERY_FRAME) >= 800)
        VAL(actor, SCENERY_FRAME) -= 700;
}

static void draw_bouncy_tree(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/tree/bouncy/%i", (VAL(actor, SCENERY_FRAME) / 100) % 8), FALSE);
}

const ActorTable TAB_BOUNCY_TREE = {
    .load = load_bouncy_tree,
    .create = create_bouncy_tree,
    .tick = tick_bouncy_tree,
    .draw = draw_bouncy_tree,
};

/* =============
   PORTRAIT EYES
   ============= */

static void load_portrait_eyes() {
    load_sprite_num("scenery/portrait/eyes/%u", 12, AKL_NEVER);
}

static void draw_portrait_eyes(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/portrait/eyes/%i", ((gamestate()->time * 33) / 50) % 12), FALSE);
}

const ActorTable TAB_PORTRAIT_EYES = {
    .load = load_portrait_eyes,
    .draw = draw_portrait_eyes,
};

/* ===============
   TEST TUBE CLONE
   =============== */

static void load_test_tube_clone() {
    load_sprite("scenery/clone", AKL_NEVER);
    load_sprite("scenery/clone/coder", AKL_NEVER);
    load_sprite("scenery/clone/jaws", AKL_NEVER);
    load_sprite("scenery/clone/mutant", AKL_NEVER);
    load_sprite("scenery/clone/zorro", AKL_NEVER);
    load_sprite("scenery/clone/star_closed", AKL_NEVER);
    load_sprite("scenery/clone/3a", AKL_NEVER);
    load_sprite("scenery/clone/big", AKL_NEVER);
}

static void create_test_tube_clone(GameActor* actor) {
    actor->depth = Int2Fx(19);

    VAL(actor, SCENERY_Y) = actor->pos.y;
    VAL(actor, SCENERY_ANGLE) = rng(360) * 1144;
}

static void tick_test_tube_clone(GameActor* actor) {
    VAL(actor, SCENERY_ANGLE) = Fmod(VAL(actor, SCENERY_ANGLE) + 1144, Fx2Pi);
    move_actor(actor, (FVec2){actor->pos.x, VAL(actor, SCENERY_Y) + Fmul(Int2Fx(14), Fcos(VAL(actor, SCENERY_ANGLE)))});
}

static void draw_test_tube_clone(const GameActor* actor) {
    batch_reset();

    const char* sprite = NULL;
    switch (VAL(actor, SCENERY_FRAME)) {
    default:
        sprite = "scenery/clone";
        break;
    case 1:
        sprite = "scenery/clone/coder";
        break;
    case 2:
        sprite = "scenery/clone/jaws";
        break;
    case 3:
        sprite = "scenery/clone/mutant";
        break;
    case 4:
        sprite = "scenery/clone/zorro";
        break;
    case 5:
        sprite = "scenery/clone/star_closed";
        break;
    case 6:
        sprite = "scenery/clone/3a";
        break;
    case 7:
        sprite = "scenery/clone/big";
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

const ActorTable TAB_TEST_TUBE_CLONE = {
    .load = load_test_tube_clone,
    .create = create_test_tube_clone,
    .tick = tick_test_tube_clone,
    .draw = draw_test_tube_clone,
};

/* =================
   TEST TUBE BUBBLES
   ================= */

static void load_test_tube_bubbles() {
    load_sound("bubbles", AKL_NEVER);
    load_actor(ACT_TEST_TUBE_BUBBLE);
}

static void tick_test_tube_bubbles(GameActor* actor) {
    if (((gamestate()->time * 2) % 5) <= 1 && in_any_view(actor->pos, Int2Fx(-256), VEF_ALL) && rng(20) > 15) {
        FVec2 bpos = actor->pos;
        bpos.x += Int2Fx(rng(35));
        bpos.x -= Int2Fx(rng(35));
        bpos.y -= Int2Fx(10);

        GameActor* bubble = create_actor(ACT_TEST_TUBE_BUBBLE, bpos);
        if (bubble != NULL) {
            bubble->vel.y = -8192 - (rng(24) * 8192);
            VAL(bubble, EFFECT_Y) = actor->pos.y + VAL(actor, SCENERY_Y);
        }
    }

    if (--VAL(actor, SCENERY_FRAME) <= 0) {
        VAL(actor, SCENERY_FRAME) = 110;
        if (in_any_view(actor->pos, Int2Fx(-256), VEF_ALL))
            play_state_sound("bubbles", PLAY_POS, A_ACTOR(actor));
    }
}

const ActorTable TAB_TEST_TUBE_BUBBLES = {
    .load = load_test_tube_bubbles,
    .tick = tick_test_tube_bubbles,
};

/* ========
   COMPUTER
   ======== */

static void load_computer() {
    load_sprite_num("scenery/house/computer/%u", 6, AKL_NEVER);
}

static void create_computer(GameActor* actor) {
    actor->depth = Int2Fx(27);
}

static void draw_computer(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt("scenery/house/computer/%i",
            ANY_FLAG(actor, FLG_SCENERY_ALT) ? (gamestate()->time % 6) : (((gamestate()->time * 11) / 100) % 2)),
        FALSE);
}

const ActorTable TAB_COMPUTER = {
    .load = load_computer,
    .create = create_computer,
    .draw = draw_computer,
};

/* ========
   KEYBOARD
   ======== */

static void load_keyboard() {
    load_sprite_num("scenery/house/clone/keyboard/%u", 8, AKL_NEVER);
}

static void create_keyboard(GameActor* actor) {
    actor->depth = Int2Fx(25);
}

static void draw_keyboard(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt("scenery/house/clone/keyboard/%i",
            ((gamestate()->time * 11) / (ANY_FLAG(actor, FLG_X_FLIP) ? 100 : 50)) % 8),
        FALSE);
}

const ActorTable TAB_KEYBOARD = {
    .load = load_keyboard,
    .create = create_keyboard,
    .draw = draw_keyboard,
};

/* =================
   HOUSE ZORRO CLONE
   ================= */

static void load_house_zorro_clone() {
    load_sprite_num("scenery/house/clone/zorro/%u", 2, AKL_NEVER);
}

static void create_house_zorro_clone(GameActor* actor) {
    actor->depth = Int2Fx(26);
}

static void draw_house_zorro_clone(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/house/clone/zorro/%i", ((gamestate()->time * 2) / 100) % 2), FALSE);
}

const ActorTable TAB_HOUSE_ZORRO = {
    .load = load_house_zorro_clone,
    .create = create_house_zorro_clone,
    .draw = draw_house_zorro_clone,
};

/* =================
   HOUSE AGENT CLONE
   ================= */

static void load_house_agent_clone() {
    load_sprite_num("scenery/house/clone/agent/%u", 2, AKL_NEVER);
}

static void draw_house_agent_clone(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/house/clone/agent/%i", ((gamestate()->time * 5) / 100) % 2), FALSE);
}

const ActorTable TAB_HOUSE_AGENT = {
    .load = load_house_agent_clone,
    .create = create_house_zorro_clone,
    .draw = draw_house_agent_clone,
};

/* ====================
   HOUSE WEREWOLF CLONE
   ==================== */

static void load_house_werewolf_clone() {
    load_sprite_num("scenery/house/clone/werewolf/%u", 3, AKL_NEVER);
}

static void tick_house_werewolf_clone(GameActor* actor) {
    if (VAL(actor, SCENERY_ANIMATION) == 1) {
        VAL(actor, SCENERY_FRAME) += 2;
        if (VAL(actor, SCENERY_FRAME) >= 200) {
            VAL(actor, SCENERY_ANIMATION) = 0;
            VAL(actor, SCENERY_FRAME) = 0;
        }
    }

    if ((gamestate()->time % 6) == 0)
        VAL(actor, SCENERY_ANGLE) = rng(10);

    if (VAL(actor, SCENERY_ANGLE) == 5) {
        VAL(actor, SCENERY_ANIMATION) = 1;
        VAL(actor, SCENERY_FRAME) = 0;
    }
}

static void draw_house_werewolf_clone(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        fmt("scenery/house/clone/werewolf/%i", VAL(actor, SCENERY_ANIMATION) + (VAL(actor, SCENERY_FRAME) / 100)),
        FALSE);
}

const ActorTable TAB_HOUSE_WEREWOLF = {
    .load = load_house_werewolf_clone,
    .create = create_house_zorro_clone,
    .tick = tick_house_werewolf_clone,
    .draw = draw_house_werewolf_clone,
};

/* ===============
   HOUSE BIG CLONE
   =============== */

static void load_house_big_clone() {
    load_sprite_num("scenery/house/clone/big/%u", 13, AKL_NEVER);
    load_sound_num("clone/game/%u", 5, AKL_NEVER);
}

static void create_house_big_clone(GameActor* actor) {
    actor->box.start.x = Int2Fx(-88);
    actor->box.start.y = Int2Fx(-77);
    actor->box.end.x = Int2Fx(134);
    actor->box.end.y = Int2Fx(83);

    actor->depth = Int2Fx(26);
}

static void tick_house_big_clone(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_SCENERY_ACTIVE))
        ++VAL(actor, SCENERY_FRAME);

    if (VAL(actor, SCENERY_FRAME) > 5) {
        VAL(actor, SCENERY_FRAME) = 0;

        const Sint32 r = rng(10);
        if (r >= 1 && r <= 5) {
            const FVec2 center = Rcenter(Radd(actor->box, actor->pos));

            // !!! CLIENT-SIDE !!!
            play_state_sound(fmt("clone/game/%i", r - 1), PLAY_POS, A_FVEC2(center));
            // !!! CLIENT-SIDE !!!
        }
    }

    FLAG_OFF(actor, FLG_SCENERY_ACTIVE);
}

static void draw_house_big_clone(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/house/clone/big/%i", ((gamestate()->time * 11) / 100) % 13), FALSE);
}

static void collide_house_big_clone(GameActor* actor, GameActor* from) {
    if (from->type == ACT_PLAYER)
        FLAG_ON(actor, FLG_SCENERY_ACTIVE);
}

const ActorTable TAB_HOUSE_BIG = {
    .load = load_house_big_clone,
    .create = create_house_big_clone,
    .tick = tick_house_big_clone,
    .draw = draw_house_big_clone,
    .collide = collide_house_big_clone,
};

/* ==============
   HOUSE CLONE 3A
   ============== */

static void load_house_clone_3a() {
    load_sprite_num("scenery/house/clone/3a/%u", 2, AKL_NEVER);
}

static void draw_house_clone_3a(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/house/clone/3a/%i", ((gamestate()->time * 3) / 50) % 2), FALSE);
}

const ActorTable TAB_HOUSE_3A = {
    .load = load_house_clone_3a,
    .create = create_house_zorro_clone,
    .draw = draw_house_clone_3a,
};

/* =================
   HOUSE CODER CLONE
   ================= */

static void load_house_coder_clone() {
    load_sprite_num("scenery/house/clone/coder/%u", 2, AKL_NEVER);
}

static void draw_house_coder_clone(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/house/clone/coder/%i", (gamestate()->time / 25) % 2), FALSE);
}

const ActorTable TAB_HOUSE_CODER = {
    .load = load_house_coder_clone,
    .create = create_house_zorro_clone,
    .draw = draw_house_coder_clone,
};

/* =================
   HOUSE JAWS CLONE
   ================= */

static void load_house_jaws_clone() {
    load_sprite_num("scenery/house/clone/jaws/%u", 4, AKL_NEVER);
}

static void draw_house_jaws_clone(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/house/clone/jaws/%i", ((gamestate()->time * 11) / 100) % 4), FALSE);
}

const ActorTable TAB_HOUSE_JAWS = {
    .load = load_house_jaws_clone,
    .create = create_house_zorro_clone,
    .draw = draw_house_jaws_clone,
};

/* =====
   FLUSH
   ===== */

static void load_flush() {
    load_sprite_num("scenery/house/flush/%u", 20, AKL_NEVER);
}

static void create_flush(GameActor* actor) {
    actor->depth = Int2Fx(27);
}

static void tick_flush(GameActor* actor) {
    VAL(actor, SCENERY_FRAME) += 11;
    while (VAL(actor, SCENERY_FRAME) >= 2000)
        VAL(actor, SCENERY_FRAME) -= 1900;
}

static void draw_flush(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/house/flush/%i", VAL(actor, SCENERY_FRAME) / 100), FALSE);
}

const ActorTable TAB_HOUSE_FLUSH = {
    .load = load_flush,
    .create = create_flush,
    .tick = tick_flush,
    .draw = draw_flush,
};

/* =======================
   HOUSE STAR CLOSED CLONE
   ======================= */

static void load_house_star_closed_clone() {
    load_sprite_num("scenery/house/clone/star_closed/%u", 5, AKL_NEVER);
}

static void draw_house_star_closed_clone(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/house/clone/star_closed/%i", ((gamestate()->time * 2) / 25) % 5), FALSE);
}

const ActorTable TAB_HOUSE_STAR_CLOSED = {
    .load = load_house_star_closed_clone,
    .create = create_house_zorro_clone,
    .draw = draw_house_star_closed_clone,
};

/* ===============
   HOUSE CLONE YAP
   =============== */

static void load_house_clone_yap() {
    load_sprite("scenery/house/clone/a", AKL_NEVER);
    load_sprite_num("scenery/house/clone/a/%u", 9, AKL_NEVER);
    load_sprite("scenery/house/clone/hat", AKL_NEVER);
    load_sprite_num("scenery/house/clone/hat/%u", 8, AKL_NEVER);
    load_sound_num("vo/clone/talk/%u", 5, AKL_NEVER);
    load_sound_num("vo/clone/talk/%ub", 5, AKL_NEVER);
}

static void tick_house_clone_yap(GameActor* actor) {
    if (VAL(actor, SCENERY_ANGLE) > 0) {
        ++VAL(actor, SCENERY_FRAME);

        if (--VAL(actor, SCENERY_ANGLE) <= 0) {
            VAL(actor, SCENERY_ANIMATION) = VAL(actor, SCENERY_FRAME) = 0;
            TOGGLE_FLAG(actor, FLG_SCENERY_ALT);
        }
    }

    if (VAL(actor, SCENERY_ANGLE) <= 0) {
        for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
            const GamePlayer* player = get_player(i);
            if (player == NULL)
                continue;

            const GameActor* pawn = get_actor(player->actor);
            if (pawn != NULL && pawn->type == ACT_PLAYER
                && Rcollide(Radd(
                                (FRect){
                                    {Int2Fx(-240), Int2Fx(-96)},
                                    {Int2Fx(145),  Int2Fx(6)  }
            },
                                actor->pos),
                    Radd(pawn->box, pawn->pos)))
            {
                ++VAL(actor, SCENERY_ALPHA);
                break;
            }
        }
    }

    if (VAL(actor, SCENERY_ALPHA) > 20) {
        VAL(actor, SCENERY_ALPHA) = 0;
        VAL(actor, SCENERY_ANIMATION) = 1;

        const Sint32 r = rng(5);
        switch (r) {
        default:
            VAL(actor, SCENERY_ANGLE) = ANY_FLAG(actor, FLG_SCENERY_ALT) ? 44 : 37;
            break;
        case 1:
            VAL(actor, SCENERY_ANGLE) = ANY_FLAG(actor, FLG_SCENERY_ALT) ? 94 : 68;
            break;
        case 2:
            VAL(actor, SCENERY_ANGLE) = ANY_FLAG(actor, FLG_SCENERY_ALT) ? 46 : 59;
            break;
        case 3:
            VAL(actor, SCENERY_ANGLE) = ANY_FLAG(actor, FLG_SCENERY_ALT) ? 146 : 48;
            break;
        case 4:
            VAL(actor, SCENERY_ANGLE) = ANY_FLAG(actor, FLG_SCENERY_ALT) ? 98 : 30;
            break;
        }

        // !!! CLIENT-SIDE !!!
        play_state_sound(
            fmt("vo/clone/talk/%i%s", r, ANY_FLAG(actor, FLG_SCENERY_ALT) ? "b" : ""), PLAY_POS, A_ACTOR(actor));
        // !!! CLIENT-SIDE !!!
    }
}

static void draw_house_clone_yap(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        (!ANY_FLAG(actor, FLG_SCENERY_ALT) && VAL(actor, SCENERY_ANIMATION) > 0)
            ? fmt("scenery/house/clone/a/%i", (VAL(actor, SCENERY_FRAME) / 2) % 9)
            : "scenery/house/clone/a",
        FALSE);

    batch_offset(B_F3_XY(-10.f, 20.f));
    draw_actor(actor,
        (ANY_FLAG(actor, FLG_SCENERY_ALT) && VAL(actor, SCENERY_ANIMATION) > 0)
            ? fmt("scenery/house/clone/hat/%i", (VAL(actor, SCENERY_FRAME) / 2) % 8)
            : "scenery/house/clone/hat",
        FALSE);
}

const ActorTable TAB_HOUSE_TALK = {
    .load = load_house_clone_yap,
    .create = create_house_zorro_clone,
    .tick = tick_house_clone_yap,
    .draw = draw_house_clone_yap,
};

/* ========
   SAMOCHOD
   ======== */

static void load_samochod() {
    load_sprite_num("scenery/clone/samochod/%u", 2, AKL_NEVER);
    load_sound("clone/car", AKL_NEVER);
}

static void create_samochod(GameActor* actor) {
    actor->depth = Int2Fx(26);

    VAL(actor, SCENERY_FRAME) = 99999;
}

static void tick_samochod(GameActor* actor) {
    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL && pawn->type == ACT_PLAYER
            && Rcollide(Radd(
                            (FRect){
                                {Int2Fx(-153), Int2Fx(-52)},
                                {Int2Fx(334),  Int2Fx(50) }
        },
                            actor->pos),
                Radd(pawn->box, pawn->pos)))
        {
            if (++VAL(actor, SCENERY_FRAME) > 70) {
                VAL(actor, SCENERY_FRAME) = 0;

                play_state_sound("clone/car", PLAY_POS, A_ACTOR(actor));
            }

            break;
        }
    }
}

static void draw_samochod(const GameActor* actor) {
    batch_reset();
    draw_actor(actor, fmt("scenery/clone/samochod/%i", (gamestate()->time / 2) % 2), FALSE);
}

const ActorTable TAB_SAMOCHOD = {
    .load = load_samochod,
    .create = create_samochod,
    .tick = tick_samochod,
    .draw = draw_samochod,
};

/* ==============
   KEYBOARD NOISE
   ============== */

static void load_keyboard_noise() {
    load_sound_num("clone/keyboard/%u", 4, AKL_NEVER);
}

static void create_keyboard_noise(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    FLAG_OFF(actor, FLG_VISIBLE);
}

static void tick_keyboard_noise(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_SCENERY_ACTIVE) && (gamestate()->time % 5) == 0) {
        const Sint32 r = rng(10);
        if (r >= 1 && r <= 4) {
            const FVec2 center = Rcenter(Radd(actor->box, actor->pos));

            // !!! CLIENT-SIDE !!!
            play_state_sound(fmt("clone/keyboard/%i", r - 1), PLAY_POS, A_FVEC2(center));
            // !!! CLIENT-SIDE !!!
        }
    }

    FLAG_OFF(actor, FLG_SCENERY_ACTIVE);
}

const ActorTable TAB_KEYBOARD_NOISE = {
    .load = load_keyboard_noise,
    .create = create_keyboard_noise,
    .tick = tick_keyboard_noise,
    .collide = collide_house_big_clone,
};

/* ======
   FLOWER
   ====== */

static void load_flower() {
    load_sprite("scenery/flower", AKL_NEVER);
}

static void create_flower(GameActor* actor) {
    VAL(actor, SCENERY_ANGLE) = rng(Int2Fx(10));
    VAL(actor, SCENERY_SCALE) = rng(FxHalf + 1) + FxHalf;
}

static void tick_flower(GameActor* actor) {
    // EVENTS FROM... Block Party!? (modified)

    VAL(actor, SCENERY_ANGLE) = Fmod(VAL(actor, SCENERY_ANGLE) + VAL(actor, SCENERY_SPEED), Fx2Pi);
    if ((gamestate()->time % 100) == 0)
        VAL(actor, SCENERY_SPEED) = 3932 + rng(3933);
}

static void draw_flower(const GameActor* actor) {
    batch_reset();
    const float scale = Fx2Float(VAL(actor, SCENERY_SCALE));
    batch_scale(B_F2_S(scale));
    batch_angle(((5.f * SDL_sinf(Fx2Float(VAL(actor, SCENERY_ANGLE))) + 5.f) / 360.f) * 2.f * SDL_PI_F);
    draw_actor(actor, "scenery/flower", FALSE);
}

const ActorTable TAB_FLOWER = {
    .load = load_flower,
    .create = create_flower,
    .tick = tick_flower,
    .draw = draw_flower,
};
