#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_water.h"

/* =====
   WATER
   ===== */

static void load() {
    if (gamestate()->flags & (GF_HARDCORE | GF_LOST_MAP))
        load_sprite_num("markers/water/alt/%u", 7, AKL_NEVER);
    else
        load_sprite_num("markers/water/%u", 5, AKL_NEVER);
    load_actor(ACT_WATER_SPLASH);
    load_actor(ACT_BUBBLE);
}

static void create(GameActor* actor) {
    actor->depth = Int2Fx(-100);

    VAL(actor, WATER_TO) = VAL(actor, WATER_Y) = actor->pos.y;

    GameState* game_state = gamestate();
    GameActor* water = get_actor(game_state->water);
    if (water != NULL)
        FLAG_ON(water, FLG_DESTROY);
    game_state->water = actor->id;
}

static void cleanup(GameActor* actor) {
    GameState* game_state = gamestate();
    if (game_state->water == actor->id)
        game_state->water = NULL_ACTOR;
}

static void tick(GameActor* actor) {
    if (actor->pos.y == VAL(actor, WATER_TO))
        return;

    const Fixed move = ((actor->pos.y > VAL(actor, WATER_TO)) ? -VAL(actor, WATER_SPEED) : VAL(actor, WATER_SPEED));
    move_actor(actor, (Fabs((actor->pos.y + move) - VAL(actor, WATER_TO)) <= VAL(actor, WATER_SPEED))
                          ? (FVec2){actor->pos.x, VAL(actor, WATER_TO)}
                          : Vadd(actor->pos, (FVec2){Fx0, move}));
}

static void draw(const GameActor* actor) {
    batch_reset();

    const VideoState* video_state = videostate();
    const Sint32 ay = Fx2Int(get_interp(actor).y), cbottom = Fx2Int(video_state->camera.pos.y) + HALF_SCREEN_HEIGHT;
    if (ay >= cbottom)
        return;

    const Sint32 cx = Fx2Int(video_state->camera.pos.x) - HALF_SCREEN_WIDTH, az = Fx2Int(actor->depth);
    batch_pos(B_F3(cx, ay, az));
    batch_color(B_U4_ALPHA(135));
    const GameState* game_state = gamestate();
    batch_sprite((game_state->flags & (GF_HARDCORE | GF_LOST_MAP))
                     ? fmt("markers/water/alt/%i", (game_state->time / 5) % 7)
                     : fmt("markers/water/%i", (game_state->time / 5) % 5));

    const Sint32 ay2 = ay + 16;
    if (ay2 >= cbottom)
        return;

    batch_pos(B_F3(cx, ay2, az));
    batch_color(B_U4(88, 136, 224, 135));
    batch_rectangle(NULL, B_F2(SCREEN_WIDTH, cbottom - ay2));
}

const ActorTable TAB_WATER = {
    .load = load,
    .create = create,
    .cleanup = cleanup,
    .tick = tick,
    .draw = draw,
};

/* =============
   WATER TRIGGER
   ============= */

static void load_trigger() {
    load_sound("water", AKL_NEVER);
}

static void create_trigger(GameActor* actor) {
    actor->box.end.x = actor->box.end.y = Int2Fx(32);

    VAL(actor, WATER_TO) = actor->pos.y;
    VAL(actor, WATER_SPEED) = Fx1;
}

static void tick_trigger(GameActor* actor) {
    VAL_TICK(actor, WATER_OVERLAP);

    if (!in_any_view(Rcenter(Radd(actor->box, actor->pos)), Int2Fx(-32), VEF_ALL))
        return;

    const PlayerID n = gamecontext()->num_players;
    for (PlayerID i = 0; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        const GameActor* pawn = get_actor(player->actor);
        if (pawn == NULL || pawn->type != ACT_PLAYER || pawn->pos.x <= (actor->pos.x + actor->box.start.x)
            || pawn->pos.x >= (actor->pos.x + actor->box.end.x))
        {
            continue;
        }

        if (VAL(actor, WATER_OVERLAP) > 0) {
            VAL(actor, WATER_OVERLAP) = 2;
            break;
        }

        GameActor* water = get_actor(gamestate()->water);
        if (water == NULL)
            break;

        if (VAL(water, WATER_TO) != VAL(actor, WATER_TO) || VAL(water, WATER_SPEED) != VAL(actor, WATER_SPEED)) {
            VAL(water, WATER_TO) = VAL(actor, WATER_TO);
            VAL(water, WATER_SPEED) = VAL(actor, WATER_SPEED);

            play_state_sound("water", 0, NULL);
        }

        VAL(actor, WATER_OVERLAP) = 2;

        if (n <= 1)
            FLAG_ON(actor, FLG_DESTROY);

        break;
    }
}

const ActorTable TAB_WATER_TRIGGER = {
    .load = load_trigger,
    .create = create_trigger,
    .tick = tick_trigger,
};
