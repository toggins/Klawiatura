#include "actors/K_bill.h"
#include "actors/K_enemies.h"
#include "actors/K_points.h"

// ================
// HELPER FUNCTIONS
// ================

GameActor* kill_enemy(GameActor* actor, Bool kick) {
	if (actor == NULL)
		return NULL;

	switch (actor->type) {
	default:
		break;

	case ACT_PIRANHA_PLANT:
	case ACT_ROTODISC: {
		if (kick)
			play_actor_sound(actor, "kick");
		FLAG_ON(actor, FLG_DESTROY);
		return NULL;
	}

	case ACT_BULLET_BILL: {
		VAL(actor, X_SPEED) = Fmul(VAL(actor, X_SPEED), 45371L);
		VAL(actor, Y_SPEED) = Fmax(VAL(actor, Y_SPEED), FxZero);
		FLAG_ON(actor, FLG_BILL_DEAD);

		if (kick)
			play_actor_sound(actor, "kick");

		return actor;
	}
	}

	GameActor* dead = create_actor(ACT_DEAD, POS_ADD(actor, FxZero, FfInt(-32L)));
	if (dead == NULL)
		return NULL;

	VAL(dead, DEAD_TYPE) = actor->type;
	FLAG_ON(dead, (actor->flags & FLG_X_FLIP) | FLG_Y_FLIP);

	switch (actor->type) {
	default:
		break;
	}

	if (kick) {
		fixed rnd = FfInt(rng(5L)), dir = 77208L + Fmul(rnd, 12868L);
		VAL(dead, X_SPEED) = Fmul(Fcos(dir), FfInt(3L));
		VAL(dead, Y_SPEED) = Fmul(Fsin(dir), FfInt(-3L));
		play_actor_sound(actor, "kick");
	}

	FLAG_ON(actor, FLG_DESTROY);
	return dead;
}

void block_fireball(GameActor* actor) {
	if (actor == NULL || get_owner(actor) == NULL)
		return;
	play_actor_sound(actor, "bump");
	FLAG_ON(actor, FLG_DESTROY);
}

void hit_fireball(GameActor* actor, GameActor* from, int32_t points) {
	if (actor == NULL || from == NULL)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	give_points(actor, player, points);
	kill_enemy(actor, true);
	FLAG_ON(from, FLG_DESTROY);
}

// ==========
// DEAD ENEMY
// ==========

static void create_dead(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-32L);
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void tick_dead(GameActor* actor) {
	if (!in_any_view(actor, FxZero, true)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, Y_SPEED) += 13107L;
	move_actor(actor, POS_SPEED(actor));
}

const GameActorTable TAB_DEAD = {.create = create_dead, .tick = tick_dead, .draw = draw_dead};
