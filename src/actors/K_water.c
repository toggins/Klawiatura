#include "K_audio.h"
#include "K_game.h"
#include "K_string.h"
#include "K_video.h"

enum {
    VAL_WATER_TO = VAL_CUSTOM,
    VAL_WATER_SPEED,
};

enum {
    FLG_WATER_UNSWIMMABLE = CUSTOM_FLAG(0),
};

/* =====
   WATER
   ===== */

static void load_water() {
    load_sprite_num("markers/water/%u", 5, AKL_NEVER);
}

static void create_water(GameActor* actor) {
    actor->depth = Int2Fx(-1000);

    VAL(actor, WATER_TO) = actor->pos.y;

    GameState* game_state = gamestate();

    GameActor* water = get_actor(game_state->water);
    if (water != NULL)
        FLAG_ON(water, FLG_DESTROY);

    game_state->water = actor->id;
}

static void cleanup_water(GameActor* actor) {
    GameState* game_state = gamestate();
    if (game_state->water == actor->id)
        game_state->water = NULL_ACTOR;
}

static void tick_water(GameActor* actor) {
    if (actor->pos.y == VAL(actor, WATER_TO))
        return;

    const Fixed move = ((actor->pos.y < VAL(actor, WATER_TO)) ? -VAL(actor, WATER_SPEED) : VAL(actor, WATER_SPEED));
    move_actor(actor, (Fabs((actor->pos.y + move) - VAL(actor, WATER_TO)) <= VAL(actor, WATER_SPEED))
                          ? (FVec2){actor->pos.x, VAL(actor, WATER_TO)}
                          : Vadd(actor->pos, (FVec2){Fx0, move}));
}

static void draw_water(const GameActor* actor) {
    batch_reset();

    const VideoState* video_state = videostate();
    const Sint32 ay = Fx2Int(actor->pos.y), cbottom = Fx2Int(video_state->camera.pos.y) + HALF_SCREEN_HEIGHT;
    if (ay >= cbottom)
        return;

    const Sint32 cx = Fx2Int(video_state->camera.pos.x) - HALF_SCREEN_WIDTH, az = Fx2Int(actor->depth);
    batch_pos(B_XYZ(cx, ay, az));
    batch_color(B_ALPHA(135));
    batch_sprite(fmt("markers/water/%i", (gamestate()->time / 5) % 5));

    const Sint32 ay2 = ay + 16;
    if (ay2 >= cbottom)
        return;

    batch_pos(B_XYZ(cx, ay2, az));
    batch_color(B_RGBA(88, 136, 224, 135));
    batch_rectangle(NULL, B_WH(SCREEN_WIDTH, cbottom - ay2));
}

const GameActorTable TAB_WATER = {
    .load = load_water,
    .create = create_water,
    .cleanup = cleanup_water,
    .tick = tick_water,
    .draw = draw_water,
};

/* =============
   WATER TRIGGER
   ============= */

static void load_water_trigger() {
    load_sound("water", AKL_NEVER);
}

static void create_water_trigger(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);
}

static void collide_water_trigger(GameActor* actor, GameActor* other) {
    if (other->type != ACT_PLAYER)
        return;

    GameActor* water = get_actor(gamestate()->water);
    if (water == NULL
        || (VAL(water, WATER_TO) == VAL(actor, WATER_TO) && VAL(water, WATER_SPEED) == VAL(actor, WATER_SPEED)))
    {
        return;
    }

    VAL(water, WATER_TO) = VAL(actor, WATER_TO);
    VAL(water, WATER_SPEED) = VAL(actor, WATER_SPEED);
    play_state_sound("water", 0, NULL);
}

const GameActorTable TAB_WATER_TRIGGER = {
    .load = load_water_trigger,
    .create = create_water_trigger,
    .collide = collide_water_trigger,
};
