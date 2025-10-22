#include "K_game.h"
#include "K_player.h"

enum {
	VAL_PSWITCH = VAL_CUSTOM,
};

enum {
	FLG_PSWITCH_ONCE = CUSTOM_FLAG(0),
};

static void load() {
	load_texture("markers/pswitch");
	load_texture("markers/pswitch2");
	load_texture("markers/pswitch3");
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
	if (actor->values[VAL_PSWITCH] > 0L && !ANY_FLAG(actor, FLG_PSWITCH_ONCE))
		--actor->values[VAL_PSWITCH];
}

static void draw(const GameActor* actor) {
	const char* tex;
	if (actor->values[VAL_PSWITCH] > 0L)
		tex = "markers/pswitch_flat";
	else
		switch ((int)((float)game_state.time / 3.703703703703704f) % 3L) {
		default:
			tex = "markers/pswitch";
			break;
		case 1:
			tex = "markers/pswitch2";
			break;
		case 2:
			tex = "markers/pswitch3";
			break;
		}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || actor->values[VAL_PSWITCH] > 0L)
		return;

	if (from->pos.y < (actor->pos.y + FfInt(16L))
		&& (from->values[VAL_Y_SPEED] > FfInt(2L) || ANY_FLAG(from, FLG_PLAYER_STOMP)))
	{
		from->values[VAL_Y_SPEED] = -FxOne;
		actor->values[VAL_PSWITCH] = game_state.pswitch = 600L;

		replace_actors(ACT_COIN, ACT_PSWITCH_BRICK);
		replace_actors(ACT_BRICK_BLOCK, ACT_PSWITCH_COIN);

		play_actor_sound(actor, "toggle");
		play_state_track(TS_EVENT, "pswitch", true);
	}
}

const GameActorTable TAB_PSWITCH = {.load = load, .create = create, .tick = tick, .draw = draw, .collide = collide};
