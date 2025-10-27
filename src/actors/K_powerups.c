#include "actors/K_player.h"
#include "actors/K_points.h"
#include "actors/K_powerups.h"

// ================
// HELPER FUNCTIONS
// ================

static void collide_powerup(GameActor* actor, GameActor* from, PlayerPower power) {
	if (from->type != ACT_PLAYER)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	if (player->power == POW_SMALL && !ANY_FLAG(actor, FLG_POWERUP_FULL)) {
		VAL(from, PLAYER_POWER) = 3000L;
		player->power = POW_BIG;
	} else if (player->power != POW_FIRE) {
		VAL(from, PLAYER_POWER) = 4000L;
		player->power = POW_FIRE;
	}
	give_points(actor, player, 1000L);

	play_actor_sound(from, "grow");
	FLAG_ON(actor, FLG_DESTROY);
}

// ========
// MUSHROOM
// ========

static void load_mushroom() {
	load_texture("items/mushroom");

	load_sound("grow");

	load_actor(ACT_POINTS);
}

static void create_mushroom(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-32L);
	actor->box.end.x = FfInt(15L);
	actor->depth = FxOne;

	FLAG_ON(actor, FLG_POWERUP_FULL);
}

static void tick_mushroom(GameActor* actor) {
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, Y_SPEED) += 19005L;
	displace_actor(actor, FfInt(10L), false);
	if (VAL(actor, X_TOUCH) != 0L)
		VAL(actor, X_SPEED) = VAL(actor, X_TOUCH) * FfInt(-2L);
}

static void draw_mushroom(const GameActor* actor) {
	draw_actor(actor, "items/mushroom", 0.f, WHITE);
}

static void collide_mushroom(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	if (player->power == POW_SMALL) {
		VAL(from, PLAYER_POWER) = 3000L;
		player->power = POW_BIG;
	}
	give_points(actor, player, 1000L);

	play_actor_sound(from, "grow");
	FLAG_ON(actor, FLG_DESTROY);
}

const GameActorTable TAB_MUSHROOM = {.load = load_mushroom,
	.create = create_mushroom,
	.tick = tick_mushroom,
	.draw = draw_mushroom,
	.collide = collide_mushroom};

// ============
// 1UP MUSHROOM
// ============

static void load_1up_mushroom() {
	load_texture("items/mushroom_1up");

	load_actor(ACT_POINTS);
}

static void draw_1up_mushroom(const GameActor* actor) {
	draw_actor(actor, "items/mushroom_1up", 0.f, WHITE);
}

static void collide_1up_mushroom(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;

	give_points(actor, get_owner(from), -1L);
	FLAG_ON(actor, FLG_DESTROY);
}

const GameActorTable TAB_MUSHROOM_1UP = {.load = load_1up_mushroom,
	.create = create_mushroom,
	.tick = tick_mushroom,
	.draw = draw_1up_mushroom,
	.collide = collide_1up_mushroom};

// ===============
// POISON MUSHROOM
// ===============

static void load_poison_mushroom() {
	load_texture("items/mushroom_poison");
	load_texture("items/mushroom_poison2");

	load_actor(ACT_EXPLODE);
}

static void create_poison_mushroom(GameActor* actor) {
	create_mushroom(actor);
	VAL(actor, X_SPEED) = FfInt(2L);
}

static void draw_poison_mushroom(const GameActor* actor) {
	draw_actor(actor,
		((int)((float)game_state.time / 11.11111111111111f) % 2L) ? "items/mushroom_poison2"
									  : "items/mushroom_poison",
		0.f, WHITE);
}

static void collide_poison_mushroom(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || VAL(from, PLAYER_STARMAN) > 0L)
		return;

	kill_player(from);
	align_interp(create_actor(ACT_EXPLODE, POS_ADD(actor, FxZero, FfInt(-15L))), actor);
	FLAG_ON(actor, FLG_DESTROY);
}

const GameActorTable TAB_MUSHROOM_POISON = {.load = load_poison_mushroom,
	.create = create_poison_mushroom,
	.tick = tick_mushroom,
	.draw = draw_poison_mushroom,
	.collide = collide_poison_mushroom};

// ===========
// FIRE FLOWER
// ===========

static void load_flower() {
	load_texture("items/flower");
	load_texture("items/flower2");
	load_texture("items/flower3");
	load_texture("items/flower4");

	load_sound("grow");

	load_actor(ACT_POINTS);
}

static void create_flower(GameActor* actor) {
	actor->box.start.x = FfInt(-17L);
	actor->box.start.y = FfInt(-32L);
	actor->box.end.x = FfInt(16L);
	actor->depth = FxOne;

	FLAG_ON(actor, FLG_POWERUP_FULL);
}

static void draw_flower(const GameActor* actor) {
	const char* tex;
	switch ((int)((float)game_state.time / 3.703703703703704f) % 4L) {
	default:
		tex = "items/flower";
		break;
	case 1L:
		tex = "items/flower2";
		break;
	case 2L:
		tex = "items/flower3";
		break;
	case 3L:
		tex = "items/flower4";
		break;
	}
	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide_flower(GameActor* actor, GameActor* from) {
	collide_powerup(actor, from, POW_FIRE);
}

const GameActorTable TAB_FIRE_FLOWER
	= {.load = load_flower, .create = create_flower, .draw = draw_flower, .collide = collide_flower};

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

	VAL(actor, Y_SPEED) += 13107L;
	displace_actor(actor, FfInt(10L), false);
	if (VAL(actor, X_TOUCH) != 0L)
		VAL(actor, X_SPEED) = VAL(actor, X_TOUCH) * -163840L;
	if (VAL(actor, Y_TOUCH) > 0L)
		VAL(actor, Y_SPEED) = FfInt(-5L);
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
	if (localplayer() == VAL(from, PLAYER_INDEX))
		play_state_track(TS_POWER, "starman", true);
	// !!! CLIENT-SIDE !!!

	VAL(from, PLAYER_STARMAN) = 500L;
	play_actor_sound(from, "grow");
	FLAG_ON(actor, FLG_DESTROY);
}

const GameActorTable TAB_STARMAN = {.load = load_starman,
	.create = create_starman,
	.tick = tick_starman,
	.draw = draw_starman,
	.collide = collide_starman};
