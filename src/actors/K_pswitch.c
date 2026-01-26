#include "K_game.h"
#include "K_string.h"

#include "actors/K_player.h"

enum {
	VAL_PSWITCH = VAL_CUSTOM,
};

enum {
	FLG_PSWITCH_ONCE = CUSTOM_FLAG(0),
};

static void load() {
	load_texture_num("markers/pswitch%u", 3L, FALSE);
	load_texture("markers/pswitch_flat", FALSE);

	load_sound("toggle", FALSE);
	load_sound("starman", FALSE);

	load_track("pswitch", FALSE);

	load_actor(ACT_PSWITCH_COIN);
	load_actor(ACT_PSWITCH_BRICK);
}

static void create(GameActor* actor) {
	actor->box.end.x = FxFrom(31L);
	actor->box.end.y = FxFrom(32L);
	actor->depth = FxOne;
}

static void tick(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_PSWITCH_ONCE))
		VAL_TICK(actor, PSWITCH);
}

static void draw(const GameActor* actor) {
	const char* tex = (VAL(actor, PSWITCH) > 0L)
	                          ? "markers/pswitch_flat"
	                          : fmt("markers/pswitch%u", (int)((float)game_state.time / 3.703703703703704f) % 3L);
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || VAL(actor, PSWITCH) > 0L)
		return;

	if (from->pos.y < (actor->pos.y + FxFrom(16L))
		&& (VAL(from, Y_SPEED) > FxFrom(2L) || ANY_FLAG(from, FLG_PLAYER_STOMP)))
	{
		VAL(from, Y_SPEED) = -FxOne;
		VAL(actor, PSWITCH) = game_state.pswitch = 600L;

		replace_actors(ACT_COIN, ACT_PSWITCH_BRICK);
		replace_actors(ACT_BRICK_BLOCK, ACT_PSWITCH_COIN);

		play_state_sound("toggle", PLAY_POS, 0L, A_ACTOR(actor));
		play_state_track(TS_EVENT, "pswitch", PLAY_LOOPING, 0L);
	}
}

const GameActorTable TAB_PSWITCH = {.load = load, .create = create, .tick = tick, .draw = draw, .collide = collide};
