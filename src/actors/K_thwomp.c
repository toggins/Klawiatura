#include "K_game.h"

#include "actors/K_enemies.h"

enum {
	VAL_THWOMP_Y = VAL_CUSTOM,
	VAL_THWOMP_STATE,
	VAL_THWOMP_FRAME,
};

enum {
	FLG_THWOMP_START = CUSTOM_FLAG(0),
	FLG_THWOMP_LAUGH = CUSTOM_FLAG(1),
};

static void load() {
	load_texture("enemies/thwomp");
	load_texture("enemies/thwomp2");
	load_texture("enemies/thwomp3");
	load_texture("enemies/thwomp4");
	load_texture("enemies/thwomp5");
	load_texture("enemies/thwomp_laugh");
	load_texture("enemies/thwomp_laugh2");
	load_texture("enemies/thwomp_laugh3");

	load_sound("hurt");
	load_sound("thwomp");

	load_actor(ACT_EXPLODE);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-27L);
	actor->box.start.y = FfInt(-33L);
	actor->box.end.x = FfInt(26L);
	actor->box.end.y = FfInt(35L);
}

static void tick(GameActor* actor) {
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if (!ANY_FLAG(actor, FLG_THWOMP_START)) {
		VAL(actor, THWOMP_Y) = actor->pos.y;
		FLAG_ON(actor, FLG_THWOMP_START);
	}

	if (ANY_FLAG(actor, FLG_THWOMP_LAUGH)) {
		VAL(actor, THWOMP_FRAME) += 10486L;
		if (VAL(actor, THWOMP_FRAME) >= FfInt(15L)) {
			VAL(actor, THWOMP_FRAME) = FxZero;
			FLAG_OFF(actor, FLG_THWOMP_LAUGH);
		}
	} else if (VAL(actor, THWOMP_FRAME) > FxZero) {
		VAL(actor, THWOMP_FRAME) += FxOne;
		if (VAL(actor, THWOMP_FRAME) >= FfInt(7L))
			VAL(actor, THWOMP_FRAME) = FxZero;
	}

	if ((game_state.time % 5L) == 0L && rng(20L) == 5L && VAL(actor, THWOMP_FRAME) <= FxZero)
		VAL(actor, THWOMP_FRAME) = FxOne;

	switch (VAL(actor, THWOMP_STATE)) {
	default: {
		++VAL(actor, THWOMP_STATE);
		break;
	}

	case 0L: {
		GameActor* nearest = nearest_pawn(actor->pos);
		if (nearest != NULL && actor->pos.x < (nearest->pos.x + FfInt(100L))
			&& actor->pos.x > (nearest->pos.x - FfInt(100L))
			&& (game_state.size.y <= F_SCREEN_HEIGHT
				|| in_player_view(actor, get_owner(nearest), FfInt(64L), false)))
			++VAL(actor, THWOMP_STATE);
		break;
	}

	case 1L: {
		VAL(actor, Y_SPEED) += FxOne;
		displace_actor(actor, FxZero, false);
		if (VAL(actor, Y_TOUCH) > 0L) {
			create_actor(ACT_EXPLODE, POS_ADD(actor, FfInt(-17L), FfInt(34L)));
			create_actor(ACT_EXPLODE, POS_ADD(actor, FfInt(17L), FfInt(34L)));
			++VAL(actor, THWOMP_STATE);
			quake_at_actor(actor, 10.f);
			play_actor_sound(actor, "hurt");
		}
		break;
	}

	case 101L: {
		move_actor(actor, POS_ADD(actor, FxZero, -FxOne));
		if (actor->pos.y < VAL(actor, THWOMP_Y)) {
			move_actor(actor, (fvec2){actor->pos.x, VAL(actor, THWOMP_Y)});
			VAL(actor, THWOMP_STATE) = 0L;
		}
		break;
	}
	}
}

static void draw(const GameActor* actor) {
	const char* tex;
	if (ANY_FLAG(actor, FLG_THWOMP_LAUGH))
		switch (FtInt(VAL(actor, THWOMP_FRAME)) % 3L) {
		default:
			tex = "enemies/thwomp_laugh";
			break;
		case 1L:
			tex = "enemies/thwomp_laugh2";
			break;
		case 2L:
			tex = "enemies/thwomp_laugh3";
			break;
		}
	else
		switch (FtInt(VAL(actor, THWOMP_FRAME)) % 7L) {
		default:
			tex = "enemies/thwomp";
			break;
		case 1L:
			tex = "enemies/thwomp2";
			break;
		case 2L:
		case 6L:
			tex = "enemies/thwomp3";
			break;
		case 3L:
		case 5L:
			tex = "enemies/thwomp4";
			break;
		case 4L:
			tex = "enemies/thwomp5";
			break;
		}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/thwomp", 0.f, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (!maybe_hit_player(actor, from))
			break;
		VAL(actor, THWOMP_FRAME) = FxZero;
		FLAG_ON(actor, FLG_THWOMP_LAUGH);
		play_actor_sound(actor, "thwomp");
		break;
	}

	case ACT_MISSILE_FIREBALL:
		block_fireball(from);
		break;
	case ACT_MISSILE_BEETROOT:
		block_beetroot(from);
		break;
	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 1000L);
		break;
	}
}

const GameActorTable TAB_THWOMP
	= {.load = load, .create = create, .tick = tick, .draw = draw, .draw_dead = draw_corpse, .collide = collide};
