#include "K_player.h"
#include "K_powerup.h" // IWYU pragma: keep

// =======
// STARMAN
// =======

static void load_starman() {
	load_texture("items/starman");
	load_texture("items/starman2");
	load_texture("items/starman3");
	load_texture("items/starman4");

	load_sound("grow");
	load_sound("starman");

	load_track("starman");
}

static void create_starman(GameActor* actor) {
	actor->box.start.x = FfInt(-16L);
	actor->box.start.y = FfInt(-32L);
	actor->box.end.x = FfInt(17L);

	actor->depth = FxOne;
}

static void tick_starman(GameActor* actor) {
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, VAL_Y_SPEED) += 13107L;
	displace_actor(actor, FfInt(10L), false);
	if (VAL(actor, VAL_X_TOUCH) != 0L)
		VAL(actor, VAL_X_SPEED) = VAL(actor, VAL_X_TOUCH) * -163840L;
	if (VAL(actor, VAL_Y_TOUCH) > 0L)
		VAL(actor, VAL_Y_SPEED) = FfInt(-5L);
}

static void draw_starman(const GameActor* actor) {
	const char* tex;
	switch ((int)((float)game_state.time / 2.040816326530612f) % 4L) {
	default:
		tex = "items/starman";
		break;
	case 1:
		tex = "items/starman2";
		break;
	case 2:
		tex = "items/starman3";
		break;
	case 3:
		tex = "items/starman4";
		break;
	}
	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide_starman(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;

	// !!! CLIENT-SIDE !!!
	if (localplayer() == VAL(from, VAL_PLAYER_INDEX))
		play_state_track(TS_POWER, "starman", true);
	// !!! CLIENT-SIDE !!!

	VAL(from, VAL_PLAYER_STARMAN) = 500L;
	play_actor_sound(from, "grow");
	FLAG_ON(actor, FLG_DESTROY);
}

const GameActorTable TAB_STARMAN = {.load = load_starman,
	.create = create_starman,
	.tick = tick_starman,
	.draw = draw_starman,
	.collide = collide_starman};
