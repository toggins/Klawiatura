#include "actors/K_autoscroll.h"

static void load() {
	load_texture("markers/platform/tanks");
}

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void tick(GameActor* actor) {
	if (game_state.autoscroll != actor->id && in_any_view(actor, FxZero, false)) {
		GameActor* autoscroll = get_actor(game_state.autoscroll);
		if (autoscroll != NULL) {
			VAL(autoscroll, X_SPEED) = VAL(actor, X_SPEED);
			VAL(autoscroll, Y_SPEED) = VAL(actor, Y_SPEED);
			FLAG_ON(autoscroll, actor->flags & FLG_SCROLL_TANKS);
			FLAG_ON(actor, FLG_DESTROY);
			return;
		} else {
			game_state.autoscroll = actor->id;
			for (PlayerID i = 0; i < numplayers(); i++) {
				GamePlayer* player = get_player(i);
				if (player == NULL)
					continue;

				GameActor* pawn = get_actor(player->actor);
				if (pawn != NULL && !in_any_view(pawn, FxZero, false))
					respawn_player(player);
			}
		}
	}

	if (game_state.autoscroll == actor->id) {
		move_actor(actor, POS_SPEED(actor));
		if (ANY_FLAG(actor, FLG_SCROLL_TANKS)) {
			const fixed end = game_state.size.x - F_SCREEN_WIDTH;
			if (actor->pos.x > end) {
				VAL(actor, X_SPEED) = FxZero;
				move_actor(actor, (fvec2){end, actor->pos.y});
			}
		}
	}
}

static void draw(const GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_SCROLL_TANKS))
		return;

	const InterpActor* iactor = get_interp(actor);
	batch_start(XYZ(FtInt(iactor->pos.x) - 32.f, FtInt(iactor->pos.y + F_SCREEN_HEIGHT) - 64.f, 20.f), 0.f, WHITE);
	batch_rectangle("markers/platform/tanks", XY(SCREEN_WIDTH + 64.f, 64.f));
}

static void cleanup(GameActor* actor) {
	if (game_state.autoscroll == actor->id)
		game_state.autoscroll = NULLACT;
}

const GameActorTable TAB_AUTOSCROLL = {.load = load, .create = create, .tick = tick, .draw = draw, .cleanup = cleanup};
