#include "actors/K_player.h"
#include "actors/K_warp.h"

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-8L);
	actor->box.start.y = FfInt(-16L);
	actor->box.end.x = FfInt(8L);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || get_actor(VAL(from, PLAYER_WARP)) != NULL)
		return;
	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	Bool warped = false;
	switch (VAL(actor, WARP_ANGLE)) {
	case 0L: {
		if (VAL(from, X_TOUCH) > 0L && ANY_INPUT(player, GI_RIGHT)) {
			move_actor(from, POS_ADD(actor, actor->box.end.x - from->box.end.x, FxZero));
			warped = true;
		}
		break;
	}

	case 1L: {
		if (VAL(from, Y_TOUCH) < 0L && ANY_INPUT(player, GI_UP)) {
			move_actor(from, POS_ADD(actor, FxZero, actor->box.start.y - from->box.start.y));
			warped = true;
		}
		break;
	}

	case 2L: {
		if (VAL(from, X_TOUCH) < 0L && ANY_INPUT(player, GI_LEFT)) {
			move_actor(from, POS_ADD(actor, actor->box.start.x - from->box.start.x, FxZero));
			warped = true;
		}
		break;
	}

	default: {
		if (VAL(from, PLAYER_GROUND) > 0L && ANY_INPUT(player, GI_DOWN)) {
			move_actor(from, actor->pos);
			warped = true;
		}
		break;
	}
	}

	if (warped) {
		from->depth = FfInt(21L);
		VAL(from, X_SPEED) = VAL(from, Y_SPEED) = FxZero;
		VAL(from, PLAYER_WARP) = actor->id;
		VAL(from, PLAYER_WARP_STATE) = 0L;
		FLAG_OFF(from, FLG_PLAYER_WARP_OUT);
		play_actor_sound(actor, "warp");
	}
}

const GameActorTable TAB_WARP = {.create = create, .collide = collide};
