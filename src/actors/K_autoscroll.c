#include "actors/K_autoscroll.h"

// ==========
// AUTOSCROLL
// ==========

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
		}

		game_state.autoscroll = actor->id;
		game_state.bounds.start = game_state.bounds.end = actor->pos;
		game_state.bounds.end.x += F_SCREEN_WIDTH, game_state.bounds.end.y += F_SCREEN_HEIGHT;

		for (PlayerID i = 0; i < numplayers(); i++) {
			GamePlayer* player = get_player(i);
			if (player == NULL)
				continue;

			GameActor* pawn = get_actor(player->actor);
			if (pawn != NULL && pawn->pos.x < actor->pos.x)
				move_actor(respawn_player(player), POS_ADD(actor, F_HALF_SCREEN_WIDTH, FxZero));
		}
	}

	if (game_state.autoscroll == actor->id) {
		move_actor(actor, POS_SPEED(actor));

		Bool clamp = false;
		if (actor->pos.x < FxZero || actor->pos.x > (game_state.size.x - F_SCREEN_WIDTH)) {
			VAL(actor, X_SPEED) = FxZero;
			clamp = true;
		}
		if (actor->pos.y < FxZero || actor->pos.x > (game_state.size.y - F_SCREEN_HEIGHT)) {
			VAL(actor, Y_SPEED) = FxZero;
			clamp = true;
		}
		if (clamp)
			move_actor(actor, (fvec2){Fclamp(actor->pos.x, FxZero, game_state.size.x - F_SCREEN_WIDTH),
						  Fclamp(actor->pos.y, FxZero, game_state.size.y - F_SCREEN_HEIGHT)});
	}
}

static void draw(const GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_SCROLL_TANKS))
		return;

	const InterpActor* iactor = get_interp(actor);
	batch_reset();
	batch_pos(B_XYZ(FtInt(iactor->pos.x) - 32.f, FtInt(iactor->pos.y + F_SCREEN_HEIGHT) - 64.f, 20.f));
	batch_tile(B_TILE(true, false));
	batch_rectangle("markers/platform/tanks", B_XY(SCREEN_WIDTH + 64.f, 64.f));
}

static void cleanup(GameActor* actor) {
	if (game_state.autoscroll == actor->id)
		game_state.autoscroll = NULLACT;
}

const GameActorTable TAB_AUTOSCROLL = {.load = load, .create = create, .tick = tick, .draw = draw, .cleanup = cleanup};

// ============
// WAVING LEVEL
// ============

static void create_wave(GameActor* actor) {
	game_state.wave = actor->id;
}

static void tick_wave(GameActor* actor) {
	const fixed old_wave = Fmul(VAL(actor, WAVE_LENGTH), Fsin(VAL(actor, WAVE_ANGLE)));
	if (!ANY_FLAG(actor, FLG_WAVE_START)) {
		VAL(actor, WAVE_DELTA) = old_wave;
		FLAG_ON(actor, FLG_WAVE_START);
	}

	VAL(actor, WAVE_ANGLE) += VAL(actor, WAVE_SPEED);
	VAL(actor, WAVE_DELTA) = old_wave - Fmul(VAL(actor, WAVE_LENGTH), Fsin(VAL(actor, WAVE_ANGLE)));
}

static void cleanup_wave(GameActor* actor) {
	if (game_state.wave == actor->id)
		game_state.wave = NULLACT;
}

const GameActorTable TAB_WAVING_LEVEL = {.create = create_wave, .tick = tick_wave, .cleanup = cleanup_wave};
