#include "K_game.h"
#include "K_player.h"

enum {
	VAL_PLATFORM_TYPE = VAL_CUSTOM,
	VAL_PLATFORM_X,
	VAL_PLATFORM_Y,
	VAL_PLATFORM_X_SPEED,
	VAL_PLATFORM_Y_SPEED,
	VAL_PLATFORM_FLAGS,
	VAL_PLATFORM_RESPAWN,
	VAL_PLATFORM_FRAME,
};

enum {
	FLG_PLATFORM_START = CUSTOM_FLAG(0),
	FLG_PLATFORM_ADD = CUSTOM_FLAG(1),
	FLG_PLATFORM_FALL = CUSTOM_FLAG(2),
	FLG_PLATFORM_WRAP = CUSTOM_FLAG(3),
	FLG_PLATFORM_FALLING = CUSTOM_FLAG(4),
};

static void load() {
	load_texture("markers/platform/big");
	load_texture("markers/platform/small");
	load_texture("markers/platform/cloud");
	load_texture("markers/platform/cloud2");
	load_texture("markers/platform/cloud3");
	load_texture("markers/platform/cloud4");
	load_texture("markers/platform/castle");
	load_texture("markers/platform/castle_big");
	load_texture("markers/platform/castle_button");
}

static void tick(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_PLATFORM_START)) {
		switch (VAL(actor, PLATFORM_TYPE)) {
		default: {
			actor->box.start.x = actor->box.start.y = FxZero;
			actor->box.end.x = FfInt(95);
			actor->box.end.y = FfInt(16);
			break;
		}

		case PLAT_SMALL: {
			actor->box.start.x = actor->box.start.y = FxZero;
			actor->box.end.x = FfInt(31);
			actor->box.end.y = FfInt(16);
			break;
		}

		case PLAT_CLOUD: {
			actor->box.start.x = actor->box.start.y = FxZero;
			actor->box.end.x = FfInt(127);
			actor->box.end.y = FfInt(32);
			break;
		}

		case PLAT_CASTLE: {
			actor->box.start.x = actor->box.start.y = FxZero;
			actor->box.end.x = FfInt(64);
			actor->box.end.y = FfInt(32);
			break;
		}

		case PLAT_CASTLE_BIG: {
			actor->box.start.x = actor->box.start.y = FxZero;
			actor->box.end.x = FfInt(120);
			actor->box.end.y = FfInt(32);
			break;
		}

		case PLAT_CASTLE_BUTTON: {
			actor->box.start.x = actor->box.start.y = FxZero;
			actor->box.end.x = FfInt(76);
			actor->box.end.y = FfInt(32);
			break;
		}
		}

		VAL(actor, PLATFORM_X) = actor->pos.x, VAL(actor, PLATFORM_Y) = actor->pos.y;
		VAL(actor, PLATFORM_X_SPEED) = VAL(actor, X_SPEED), VAL(actor, PLATFORM_Y_SPEED) = VAL(actor, Y_SPEED);
		VAL(actor, PLATFORM_FLAGS) = (ActorValue)actor->flags;
		FLAG_ON(actor, FLG_PLATFORM_START);
	}

	if (ANY_FLAG(actor, FLG_PLATFORM_WRAP) && !ANY_FLAG(actor, FLG_PLATFORM_FALLING)) {
		if (VAL(actor, Y_SPEED) > FxZero && actor->pos.y > (game_state.size.y + FfInt(10L))) {
			move_actor(actor, (fvec2){actor->pos.x, FfInt(-10L)});
			skip_interp(actor);
		} else if (VAL(actor, Y_SPEED) < FxZero && actor->pos.y < FfInt(-10L)) {
			move_actor(actor, (fvec2){actor->pos.x, game_state.size.y + FfInt(10L)});
			skip_interp(actor);
		}
	}

	if (actor->pos.y > (game_state.size.y + FfInt(32L))) {
		if (!ANY_FLAG(actor, FLG_PLATFORM_WRAP) && (game_state.flags & GF_SINGLE)) {
			FLAG_ON(actor, FLG_DESTROY);
			return;
		} else if (++VAL(actor, PLATFORM_RESPAWN) >= 150L) {
			move_actor(actor, (fvec2){VAL(actor, PLATFORM_X), VAL(actor, PLATFORM_Y)});
			skip_interp(actor);

			VAL(actor, X_SPEED) = VAL(actor, PLATFORM_X_SPEED),
				   VAL(actor, Y_SPEED) = VAL(actor, PLATFORM_Y_SPEED);
			actor->flags = (ActorFlag)VAL(actor, PLATFORM_FLAGS);
			VAL(actor, PLATFORM_RESPAWN) = 0L;
		}
	}

	if (VAL(actor, PLATFORM_TYPE) == PLAT_CLOUD)
		VAL(actor, PLATFORM_TYPE) += 8L;

	if (ANY_FLAG(actor, FLG_PLATFORM_FALLING))
		VAL(actor, Y_SPEED) += 13107L;
	move_actor(actor, POS_SPEED(actor));
	collide_actor(actor);
}

static void draw(const GameActor* actor) {
	const char* tex;
	switch (VAL(actor, PLATFORM_TYPE)) {
	default:
		tex = "markers/platform/big";
		break;

	case PLAT_SMALL:
		tex = "markers/platform/small";
		break;

	case PLAT_CLOUD: {
		switch ((VAL(actor, PLATFORM_FRAME) / 100L) % 4L) {
		default:
			tex = "markers/platform/cloud";
			break;
		case 1:
			tex = "markers/platform/cloud2";
			break;
		case 2:
			tex = "markers/platform/cloud3";
			break;
		case 3:
			tex = "markers/platform/cloud4";
			break;
		}
		break;
	}

	case PLAT_CASTLE:
		tex = "markers/platform/castle";
		break;

	case PLAT_CASTLE_BIG:
		tex = "markers/platform/castle_big";
		break;

	case PLAT_CASTLE_BUTTON:
		tex = "markers/platform/castle_button";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void on_top(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;

	VAL(from, PLAYER_PLATFORM) = actor->id;
	if (ANY_FLAG(actor, FLG_PLATFORM_FALL))
		FLAG_ON(actor, FLG_PLATFORM_FALLING);
}

const GameActorTable TAB_PLATFORM
	= {.is_solid = always_top, .load = load, .tick = tick, .draw = draw, .on_top = on_top};

// ===============
// PLATFORM TURNER
// ===============

static void create_turn(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void collide_turn(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLATFORM)
		return;

	if (ANY_FLAG(actor, FLG_PLATFORM_ADD)) {
		VAL(from, X_SPEED) += VAL(actor, X_SPEED);
		if (!ANY_FLAG(actor, FLG_PLATFORM_FALLING))
			VAL(from, Y_SPEED) += VAL(actor, Y_SPEED);
	} else {
		VAL(from, X_SPEED) = VAL(actor, X_SPEED);
		if (!ANY_FLAG(actor, FLG_PLATFORM_FALLING))
			VAL(from, Y_SPEED) = VAL(actor, Y_SPEED);
	}
	from->flags = (actor->flags & ~FLG_PLATFORM_ADD) | (from->flags & FLG_PLATFORM_FALLING);
}

const GameActorTable TAB_PLATFORM_TURN = {.create = create_turn, .collide = collide_turn};
