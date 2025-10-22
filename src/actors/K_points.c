#include "K_points.h"

// ================
// HELPER FUNCTIONS
// ================

void give_points(GameActor* actor, GamePlayer* player, int32_t points) {
	if (actor == NULL || player == NULL)
		return;

	if (points < 0L) {
		const int32_t gimme = (int32_t)(player->lives) - points;
		player->lives = SDL_min(gimme, 100L);

		// !!! CLIENT-SIDE !!!
		if (player->id != viewplayer())
			play_actor_sound(actor, "1up");
		// !!! CLIENT-SIDE !!!
	} else
		player->score += points;

	GameActor* pts = create_actor(
		ACT_POINTS, POS_ADD(actor, Flerp(actor->box.start.x, actor->box.end.x, FxHalf), actor->box.start.y));
	if (pts != NULL) {
		pts->values[VAL_POINTS_PLAYER] = (ActorValue)player->id;
		pts->values[VAL_POINTS] = points;
	}
}

// ======
// POINTS
// ======

static void load() {
	load_texture("ui/points/100");
	load_texture("ui/points/200");
	load_texture("ui/points/500");
	load_texture("ui/points/1000");
	load_texture("ui/points/2000");
	load_texture("ui/points/5000");
	load_texture("ui/points/10000");
	load_texture("ui/points/1000000");
	load_texture("ui/points/1up");

	load_sound("1up");
}

static void create(GameActor* actor) {
	actor->depth = FfInt(-1000L);
	actor->values[VAL_POINTS_PLAYER] = (ActorValue)NULLPLAY;
}

static void tick(GameActor* actor) {
	++actor->values[VAL_POINTS_TIME];
	const ActorValue time = actor->values[VAL_POINTS_TIME];
	if (time < 35L)
		move_actor(actor, POS_ADD(actor, FxZero, -FxOne));

	if ((actor->values[VAL_POINTS] >= 10000L && time >= 300L)
		|| (actor->values[VAL_POINTS] >= 1000L && time >= 200L)
		|| (actor->values[VAL_POINTS] < 1000L && time >= 100L))
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw(const GameActor* actor) {
	if (actor->values[VAL_POINTS_PLAYER] != viewplayer())
		return;

	const char* tex;
	switch (actor->values[VAL_POINTS]) {
	default:
	case 100L:
		tex = "ui/points/100";
		break;
	case 200L:
		tex = "ui/points/200";
		break;
	case 500L:
		tex = "ui/points/500";
		break;
	case 1000L:
		tex = "ui/points/1000";
		break;
	case 2000L:
		tex = "ui/points/2000";
		break;
	case 5000L:
		tex = "ui/points/5000";
		break;
	case 10000L:
		tex = "ui/points/10000";
		break;
	case 1000000L:
		tex = "ui/points/1000000";
		break;
	case -1L:
		tex = "ui/points/1up";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

static PlayerID owner(const GameActor* actor) {
	return (PlayerID)actor->values[VAL_POINTS_PLAYER];
}

const GameActorTable TAB_POINTS = {.load = load, .create = create, .tick = tick, .draw = draw, .owner = owner};
