#include "K_game.h"

enum {
	VAL_BOUNDS_X1 = VAL_CUSTOM,
	VAL_BOUNDS_Y1,
	VAL_BOUNDS_X2,
	VAL_BOUNDS_Y2,
};

static void load() {
	load_sound("warp", FALSE);
}

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;
	if (VAL(actor, BOUNDS_X1) == VAL(actor, BOUNDS_X2) || VAL(actor, BOUNDS_Y1) == VAL(actor, BOUNDS_Y2))
		return;
	if (player->bounds.start.x != VAL(actor, BOUNDS_X1) || player->bounds.start.y != VAL(actor, BOUNDS_Y1)
		|| player->bounds.end.x != VAL(actor, BOUNDS_X2) || player->bounds.end.y != VAL(actor, BOUNDS_Y2))
	{
		player->bounds.start.x = VAL(actor, BOUNDS_X1), player->bounds.start.y = VAL(actor, BOUNDS_Y1),
		player->bounds.end.x = VAL(actor, BOUNDS_X2), player->bounds.end.y = VAL(actor, BOUNDS_Y2);

		// !!! CLIENT-SIDE !!!
		if (viewplayer() == player->id) {
			lerp_camera(25.f);
			play_state_sound("warp");
		}
		// !!! CLIENT-SIDE !!!
	}
}

const GameActorTable TAB_BOUNDS = {.load = load, .create = create, .collide = collide};
