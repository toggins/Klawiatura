
#include "K_audio.h"
#include "K_video.h"

#include "actors/K_player.h"

/* ============
   PLAYER SPAWN
   ============ */

static void load_spawn() {
    load_actor(ACT_PLAYER);
}

static void create_spawn(GameActor* actor) {
    GameState* game_state = gamestate();

    GameActor* spawn = get_actor(game_state->spawn);
    if (spawn != NULL)
        FLAG_ON(spawn, FLG_DESTROY);

    game_state->spawn = actor->id;
}

static void cleanup_spawn(GameActor* actor) {
    GameState* game_state = gamestate();
    if (game_state->spawn == actor->id)
        game_state->spawn = NULL_ACTOR;
}

const ActorTable TAB_PLAYER_SPAWN = {
    .load = load_spawn,
    .create = create_spawn,
    .cleanup = cleanup_spawn,
};

/* ======
   PLAYER
   ====== */

static void load() {
    const GameContext* game_context = gamecontext();
    for (PlayerID i = 0; i < game_context->num_players; i++) {
        for (PlayerPowerup j = 0; j < (PlayerPowerup)POW_SIZE; j++)
            for (PlayerFrame k = 0; k < (PlayerFrame)PF_SIZE; k++)
                load_sprite(get_character_sprite(game_context->players[i].character, j, k), AKL_NEVER);
    }
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-9);
    actor->box.start.y = Int2Fx(-25);
    actor->box.end.x = Int2Fx(10);
    actor->box.end.y = Fx1;

    VAL(actor, PLAYER) = (ActorValue)NULL_PLAYER;
}

static void cleanup(GameActor* actor) {
    GamePlayer* player = get_player(VAL(actor, PLAYER));
    if (player != NULL && player->actor == actor->id)
        player->actor = NULL_ACTOR;
}

static void tick(GameActor* actor) {
    GamePlayer* player = get_player(VAL(actor, PLAYER));
    if (player == NULL) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    actor->box.start.y = (player->powerup == POW_NONE) ? Int2Fx(-25) : Int2Fx(-51);

    VAL(actor, X_SPEED) = Fmul(VAL(actor, X_SPEED), 58982);
    VAL(actor, Y_SPEED) = Fmul(VAL(actor, Y_SPEED), 58982);

    if (ANY_INPUT(player, GI_LEFT))
        VAL(actor, X_SPEED) -= Fx1;
    if (ANY_INPUT(player, GI_RIGHT))
        VAL(actor, X_SPEED) += Fx1;
    if (ANY_INPUT(player, GI_UP))
        VAL(actor, Y_SPEED) -= Fx1;
    if (ANY_INPUT(player, GI_DOWN))
        VAL(actor, Y_SPEED) += Fx1;

    move_actor(actor, POS_SPEED(actor));
    player->pos = actor->pos;
}

static void draw(const GameActor* actor) {
    const GamePlayer* player = get_player(VAL(actor, PLAYER));
    if (player == NULL)
        return;

    batch_reset();
    const FVec2 ipos = get_interp(actor);
    const Sint32 ax = Fx2Int(ipos.x), ay = Fx2Int(ipos.y);
    batch_pos(B_XYZ(ax - 9.f, ay - ((player->powerup == POW_NONE) ? 25.f : 51.f), Fx2Float(actor->depth)));
    batch_color(B_BLACK);
    batch_rectangle(NULL, B_WH(19.f, (player->powerup == POW_NONE) ? 26.f : 52.f));
}

static PlayerID owner(const GameActor* actor) {
    return VAL(actor, PLAYER);
}

const ActorTable TAB_PLAYER = {
    .load = load,
    .create = create,
    .cleanup = cleanup,
    .tick = tick,
    .draw = draw,
    .owner = owner,
};
