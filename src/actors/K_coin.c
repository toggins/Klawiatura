#include "K_string.h"

#include "actors/K_coin.h"
#include "actors/K_points.h"

// ====
// COIN
// ====

static void load() {
	load_texture_num("items/coin%u", 3L);

	load_sound("coin");

	load_actor(ACT_COIN_POP);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(6L);
	actor->box.start.y = FfInt(2L);
	actor->box.end.x = FfInt(25L);
	actor->box.end.y = FfInt(30L);
	actor->depth = FfInt(2L);
}

static void draw(const GameActor* actor) {
	const char* tex = NULL;
	switch ((game_state.time / 5L) % 4L) {
	default:
		tex = "items/coin0";
		break;
	case 1L:
	case 3L:
		tex = "items/coin1";
		break;
	case 2L:
		tex = "items/coin2";
		break;
	}
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		GamePlayer* player = get_owner(from);
		if (player == NULL)
			break;

		++player->coins;
		while (player->coins >= 100L) {
			give_points(actor, player, -1L);
			player->coins -= 100L;
		}
		player->score += 200L;
		play_actor_sound(actor, "coin");

		FLAG_ON(actor, FLG_DESTROY);
		break;
	}

	case ACT_BLOCK_BUMP: {
		GamePlayer* player = get_owner(from);
		if (player == NULL)
			break;

		GameActor* pop = create_actor(ACT_COIN_POP, POS_ADD(actor, FfInt(16L), FfInt(28L)));
		if (pop != NULL)
			VAL(pop, COIN_POP_PLAYER) = (ActorValue)player->id;
		FLAG_ON(actor, FLG_DESTROY);
		break;
	}
	}
}

const GameActorTable TAB_COIN = {.load = load, .create = create, .draw = draw, .collide = collide};
const GameActorTable TAB_PSWITCH_COIN = {.load = load, .create = create, .draw = draw, .collide = collide};

// ========
// COIN POP
// ========

static void load_pop() {
	load_texture_num("items/coin_pop%u", 4L);
	load_texture_num("effects/spark%u", 4L);

	load_sound("coin");

	load_actor(ACT_POINTS);
}

static void create_pop(GameActor* actor) {
	VAL(actor, COIN_POP_PLAYER) = (ActorValue)NULLPLAY;
}

static void tick_pop(GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_COIN_POP_START)) {
		GamePlayer* player = get_owner(actor);
		if (player != NULL) {
			++player->coins;
			while (player->coins >= 100L) {
				give_points(actor, player, -1L);
				player->coins -= 100L;
			}
			player->score += 200L;
		}

		play_actor_sound(actor, "coin");
		VAL(actor, Y_SPEED) = -278528L;
		FLAG_ON(actor, FLG_COIN_POP_START);
	}

	if (VAL(actor, Y_SPEED) < FxZero) {
		move_actor(actor, POS_SPEED(actor));
		VAL(actor, Y_SPEED) = Fmin(VAL(actor, Y_SPEED) + 10945L, FxZero);
	}

	if (ANY_FLAG(actor, FLG_COIN_POP_SPARK)) {
		VAL(actor, COIN_POP_FRAME) += 35L;
		if (VAL(actor, COIN_POP_FRAME) >= 400L) {
			GameActor* points = create_actor(ACT_POINTS, actor->pos);
			if (points != NULL) {
				VAL(points, POINTS_PLAYER) = (ActorValue)get_owner_id(actor);
				VAL(points, POINTS) = 200L;
			}
			FLAG_ON(actor, FLG_DESTROY);
		}
	} else {
		VAL(actor, COIN_POP_FRAME) += 70L;
		if (VAL(actor, COIN_POP_FRAME) >= 1400L) {
			VAL(actor, COIN_POP_FRAME) = 0L;
			FLAG_ON(actor, FLG_COIN_POP_SPARK);
		}
	}
}

static void draw_pop(const GameActor* actor) {
	const char* tex = NULL;
	if (ANY_FLAG(actor, FLG_COIN_POP_SPARK))
		tex = fmt("effects/spark%u", (VAL(actor, COIN_POP_FRAME) / 100L));
	else {
		const uint32_t frame = (VAL(actor, COIN_POP_FRAME) / 100L) % 5L;
		tex = fmt("items/coin_pop%u", (frame >= 2L) ? (frame - 1L) : 0L);
	}
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static PlayerID pop_owner(const GameActor* actor) {
	return (PlayerID)VAL(actor, COIN_POP_PLAYER);
}

const GameActorTable TAB_COIN_POP = {
	.load = load_pop,
	.create = create_pop,
	.tick = tick_pop,
	.draw = draw_pop,
	.owner = pop_owner,
};
