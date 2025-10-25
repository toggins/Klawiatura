#include "K_points.h"

// ================
// HELPER FUNCTIONS
// ================

void give_points(GameActor* actor, GamePlayer* player, int32_t points) {
	if (actor == NULL || player == NULL || points == 0L)
		return;

	if (points < 0L) {
		const int32_t gimme = (int32_t)(player->lives) - points;
		player->lives = SDL_min(gimme, 100L);

		// !!! CLIENT-SIDE !!!
		if (viewplayer() == player->id)
			play_actor_sound(actor, "1up");
		// !!! CLIENT-SIDE !!!
	} else
		player->score += points;

	GameActor* pts = create_actor(
		ACT_POINTS, POS_ADD(actor, Flerp(actor->box.start.x, actor->box.end.x, FxHalf), actor->box.start.y));
	if (pts != NULL) {
		VAL(pts, VAL_POINTS_PLAYER) = (ActorValue)player->id;
		VAL(pts, VAL_POINTS) = points;
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
	VAL(actor, VAL_POINTS_PLAYER) = (ActorValue)NULLPLAY;
}

static void tick(GameActor* actor) {
	++VAL(actor, VAL_POINTS_TIME);
	const ActorValue time = VAL(actor, VAL_POINTS_TIME);
	if (time < 35L)
		move_actor(actor, POS_ADD(actor, FxZero, -FxOne));

	const ActorValue pts = VAL(actor, VAL_POINTS);
	if ((pts >= 10000L && time >= 300L) || (pts >= 1000L && time >= 200L) || (pts < 1000L && time >= 100L))
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw(const GameActor* actor) {
	// !!! CLIENT-SIDE !!!
	if (viewplayer() != VAL(actor, VAL_POINTS_PLAYER))
		return;
	// !!! CLIENT-SIDE !!!

	const char* tex;
	switch (VAL(actor, VAL_POINTS)) {
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
	return (PlayerID)VAL(actor, VAL_POINTS_PLAYER);
}

const GameActorTable TAB_POINTS = {.load = load, .create = create, .tick = tick, .draw = draw, .owner = owner};
