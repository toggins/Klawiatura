#include "K_game.h"

#include "actors/K_player.h"

enum {
	VAL_PSWITCH = VAL_CUSTOM,
};

enum {
	FLG_PSWITCH_ONCE = CUSTOM_FLAG(0),
};

static void load() {
	load_texture_wild("markers/pswitch?");
	load_texture("markers/pswitch_flat");

	load_sound("toggle");
	load_sound("starman");

	load_track("pswitch");

	load_actor(ACT_PSWITCH_COIN);
	load_actor(ACT_PSWITCH_BRICK);
}

static void create(GameActor* actor) {
	actor->box.end.x = FfInt(31L);
	actor->box.end.y = FfInt(32L);
	actor->depth = FxOne;
}

static void tick(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_PSWITCH_ONCE))
		VAL_TICK(actor, PSWITCH);
}

static void draw(const GameActor* actor) {
	const char* tex;
	if (VAL(actor, PSWITCH) > 0L)
		tex = "markers/pswitch_flat";
	else
		switch ((int)((float)game_state.time / 3.703703703703704f) % 3L) {
		default:
			tex = "markers/pswitch";
			break;
		case 1L:
			tex = "markers/pswitch2";
			break;
		case 2L:
			tex = "markers/pswitch3";
			break;
		}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || VAL(actor, PSWITCH) > 0L)
		return;

	if (from->pos.y < (actor->pos.y + FfInt(16L))
		&& (VAL(from, Y_SPEED) > FfInt(2L) || ANY_FLAG(from, FLG_PLAYER_STOMP)))
	{
		VAL(from, Y_SPEED) = -FxOne;
		VAL(actor, PSWITCH) = game_state.pswitch = 600L;

		replace_actors(ACT_COIN, ACT_PSWITCH_BRICK);
		replace_actors(ACT_BRICK_BLOCK, ACT_PSWITCH_COIN);

		play_actor_sound(actor, "toggle");
		play_state_track(TS_EVENT, "pswitch", true);
	}
}

const GameActorTable TAB_PSWITCH = {.load = load, .create = create, .tick = tick, .draw = draw, .collide = collide};
