#include "actors/K_bill.h"
#include "actors/K_enemies.h"
#include "actors/K_player.h"
#include "actors/K_points.h"

// ===========
// BULLET BILL
// ===========

static void load() {
	load_texture("enemies/bullet");

	load_sound("stomp");
	load_sound("kick");

	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-16L);
	actor->box.start.y = FfInt(-13L);
	actor->box.end.x = FfInt(16L);
	actor->box.end.y = FfInt(15L);
}

static void tick(GameActor* actor) {
	if (ANY_FLAG(actor, FLG_BILL_DEAD)) {
		VAL(actor, Y_SPEED) += 26214L;
		if (!in_any_view(actor, FxZero, false)) {
			FLAG_ON(actor, FLG_DESTROY);
			return;
		}
	} else if (!in_any_view(actor, FfInt(224L), false))
		FLAG_ON(actor, FLG_DESTROY);

	move_actor(actor, POS_SPEED(actor));
}

static void draw(const GameActor* actor) {
	draw_actor(actor, "enemies/bullet", 0.f, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (ANY_FLAG(actor, FLG_BILL_DEAD))
		return;

	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (VAL(from, PLAYER_STARMAN) > 0L) {
			player_starman(from, actor);
			break;
		}

		if (from->pos.y < actor->pos.y && (VAL(from, Y_SPEED) >= FxZero || ANY_FLAG(from, FLG_PLAYER_STOMP))) {
			GamePlayer* player = get_owner(from);

			VAL(from, Y_SPEED) = Fmin(
				VAL(from, Y_SPEED), FfInt((player != NULL && ANY_INPUT(player, GI_JUMP)) ? -13L : -8L));
			FLAG_ON(from, FLG_PLAYER_STOMP);

			play_actor_sound(actor, "stomp");
			give_points(actor, player, 100L);
			kill_enemy(actor, false);
		} else
			hit_player(from);

		break;
	}

	case ACT_MISSILE_FIREBALL:
		block_fireball(from);
		break;
	}
}

const GameActorTable TAB_BULLET_BILL = {.load = load, .create = create, .tick = tick, .draw = draw, .collide = collide};

// ============
// BILL BLASTER
// ============

static void load_blaster() {
	load_sound("bang");
	load_sound("bang4");

	load_actor(ACT_BULLET_BILL);
	load_actor(ACT_EXPLODE);
}

static void create_blaster(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void tick_blaster(GameActor* actor) {
	if ((game_state.time % 10L) == 0L)
		FLAG_OFF(actor, FLG_BILL_BLOCKED);

	GameActor* nearest = nearest_pawn(POS_ADD(actor, FfInt(16L), FfInt(16L)));
	if (nearest != NULL && nearest->pos.x > (actor->pos.x - FfInt(64L))
		&& nearest->pos.x < (actor->pos.x + FfInt(96L)))
		FLAG_ON(actor, FLG_BILL_BLOCKED);

	if (!ANY_FLAG(actor, FLG_BILL_BLOCKED) && in_any_view(actor, FxZero, false))
		++VAL(actor, BILL_TIME);

	if (VAL(actor, BILL_TIME) <= 25L)
		return;
	Bool flip = (nearest == NULL || nearest->pos.x < (actor->pos.x + FfInt(16L)));
	GameActor* bullet = create_actor(ACT_BULLET_BILL, POS_ADD(actor, flip ? FxZero : FfInt(32L), FfInt(16L)));
	if (bullet != NULL) {
		VAL(bullet, X_SPEED) = (game_state.flags & GF_FUNNY) ? 425984L : 212992L;
		if (flip) {
			VAL(bullet, X_SPEED) = Fmul(VAL(bullet, X_SPEED), -FxOne);
			FLAG_ON(bullet, FLG_X_FLIP);
		}

		create_actor(ACT_EXPLODE, bullet->pos);
		play_actor_sound(bullet, "bang");
	}

	VAL(actor, BILL_TIME) = -50L - rng(150L);
}

const GameActorTable TAB_BILL_BLASTER = {.load = load_blaster, .create = create_blaster, .tick = tick_blaster};
