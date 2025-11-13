#include "actors/K_artillery.h"
#include "actors/K_enemies.h"
#include "actors/K_player.h"

// ===========
// BULLET BILL
// ===========

static void load_bullet() {
	load_texture("enemies/bullet");

	load_sound("stomp");
	load_sound("kick");

	load_actor(ACT_POINTS);
}

static void create_bullet(GameActor* actor) {
	actor->box.start.x = FfInt(-16L);
	actor->box.start.y = FfInt(-13L);
	actor->box.end.x = FfInt(16L);
	actor->box.end.y = FfInt(15L);
}

static void tick_bullet(GameActor* actor) {
	if (ANY_FLAG(actor, FLG_ARTILLERY_DEAD)) {
		VAL(actor, Y_SPEED) += 26214L;
		if (!in_any_view(actor, FxZero, false)) {
			FLAG_ON(actor, FLG_DESTROY);
			return;
		}
	} else if (!in_any_view(actor, FfInt(224L), false))
		FLAG_ON(actor, FLG_DESTROY);

	move_actor(actor, POS_SPEED(actor));
}

static void draw_bullet(const GameActor* actor) {
	draw_actor(actor, "enemies/bullet", 0.f, WHITE);
}

static void collide_bullet(GameActor* actor, GameActor* from) {
	if (ANY_FLAG(actor, FLG_ARTILLERY_DEAD))
		return;

	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (check_stomp(actor, from, FxZero, 100L))
			kill_enemy(actor, from, false);
		else
			maybe_hit_player(actor, from);
		break;
	}

	case ACT_BLOCK_BUMP:
		hit_bump(actor, from, 100L);
		break;
	case ACT_KOOPA_SHELL:
	case ACT_BUZZY_SHELL:
		hit_shell(actor, from);
		break;
	case ACT_MISSILE_FIREBALL:
		block_fireball(from);
		break;
	case ACT_MISSILE_BEETROOT:
		hit_beetroot(actor, from, 100L);
		break;
	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 100L);
		break;
	}
}

const GameActorTable TAB_BULLET_BILL = {.load = load_bullet,
	.create = create_bullet,
	.tick = tick_bullet,
	.draw = draw_bullet,
	.collide = collide_bullet};

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
		FLAG_OFF(actor, FLG_ARTILLERY_BLOCKED);

	GameActor* nearest = nearest_pawn(POS_ADD(actor, FfInt(16L), FfInt(16L)));
	if (nearest != NULL) {
		const fixed x = actor->pos.x + FfInt(16L);
		if (nearest->pos.x < x)
			FLAG_ON(actor, FLG_X_FLIP);
		else if (nearest->pos.x > x)
			FLAG_OFF(actor, FLG_X_FLIP);
		if (nearest->pos.x > (x - FfInt(80L)) && nearest->pos.x < (x + FfInt(80L)))
			FLAG_ON(actor, FLG_ARTILLERY_BLOCKED);
	}

	if (!ANY_FLAG(actor, FLG_ARTILLERY_BLOCKED) && in_any_view(actor, FxZero, false))
		++VAL(actor, ARTILLERY_TIME);

	if (VAL(actor, ARTILLERY_TIME) <= 25L)
		return;
	GameActor* bullet = create_actor(
		ACT_BULLET_BILL, POS_ADD(actor, ANY_FLAG(actor, FLG_X_FLIP) ? FxZero : FfInt(32L), FfInt(16L)));
	if (bullet != NULL) {
		const fixed spd = (game_state.flags & GF_FUNNY) ? 425984L : 212992L;
		VAL(bullet, X_SPEED) = ANY_FLAG(actor, FLG_X_FLIP) ? -spd : spd;
		FLAG_ON(bullet, actor->flags & FLG_X_FLIP);
		create_actor(ACT_EXPLODE, bullet->pos);
		play_actor_sound(bullet, "bang");
	}

	VAL(actor, ARTILLERY_TIME) = -50L - rng(150L);
}

const GameActorTable TAB_BILL_BLASTER = {.load = load_blaster, .create = create_blaster, .tick = tick_blaster};

// =================
// SPIKE BALL CANNON
// =================

static void load_spike_cannon() {
	load_texture("enemies/spike_cannon");

	load_sound("bang2");

	load_actor(ACT_SPIKE_BALL);
	load_actor(ACT_EXPLODE);
}

static void create_spike_cannon(GameActor* actor) {
	actor->box.start.y = FfInt(22L);
	actor->box.end.x = actor->box.end.y = FfInt(64L);
}

static void tick_spike_cannon(GameActor* actor) {
	if ((game_state.time % ((game_state.flags & GF_FUNNY) ? 20L : 50L)) == 0L && in_any_view(actor, FxZero, false))
		++VAL(actor, ARTILLERY_TIME);

	if (VAL(actor, ARTILLERY_TIME) < 2L)
		return;
	GameActor* ball = create_actor(ACT_SPIKE_BALL, POS_ADD(actor, FfInt(32L), FfInt(22L)));
	if (ball != NULL) {
		VAL(ball, X_SPEED) = FfInt(-2L + rng(5L));
		if (!ANY_FLAG(actor, FLG_Y_FLIP))
			ball->values[VAL_Y_SPEED] = FfInt(-7L - rng(5L));

		create_actor(ACT_EXPLODE, ball->pos);
		play_actor_sound(ball, "bang2");
	}

	VAL(actor, ARTILLERY_TIME) = 0L;
}

static void draw_spike_cannon(const GameActor* actor) {
	draw_actor(actor, "enemies/spike_cannon", 0.f, WHITE);
}

const GameActorTable TAB_SPIKE_CANNON = {.is_solid = always_solid,
	.load = load_spike_cannon,
	.create = create_spike_cannon,
	.tick = tick_spike_cannon,
	.draw = draw_spike_cannon};

// ==========
// SPIKE BALL
// ==========

static void load_spike() {
	load_texture("missiles/spike_ball");

	load_actor(ACT_SPIKE_BALL_EFFECT);
}

static void create_spike(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FfInt(-20L);
	actor->box.end.x = actor->box.end.y = FfInt(20L);
}

static void tick_spike(GameActor* actor) {
	if (below_level(actor)) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, ARTILLERY_ANGLE) += Frad(FfInt(120L));
	VAL(actor, Y_SPEED) += 6554L;
	move_actor(actor, POS_SPEED(actor));

	if ((game_state.time % 3L) == 0L && in_any_view(actor, FxZero, false))
		align_interp(create_actor(ACT_SPIKE_BALL_EFFECT, actor->pos), actor);
}

static void draw_spike(const GameActor* actor) {
	draw_actor(actor, "missiles/spike_ball", FtFloat(VAL(actor, ARTILLERY_ANGLE)), WHITE);
}

static void collide_spike(GameActor* actor, GameActor* from) {
	hit_player(from);
}

const GameActorTable TAB_SPIKE_BALL = {
	.load = load_spike, .create = create_spike, .tick = tick_spike, .draw = draw_spike, .collide = collide_spike};

// =================
// SPIKE BALL EFFECT
// =================

static void load_spike_effect() {
	load_texture("missiles/spike_ball");
}

static void create_spike_effect(GameActor* actor) {
	VAL(actor, ARTILLERY_ALPHA) = 255L;
}

static void tick_spike_effect(GameActor* actor) {
	actor->depth += 4095L;
	VAL(actor, ARTILLERY_ANGLE) += Frad(FfInt(120L));
	VAL(actor, ARTILLERY_ALPHA) -= 5L;
	if (VAL(actor, ARTILLERY_ALPHA) <= 0L)
		FLAG_ON(actor, FLG_DESTROY);
}

static void draw_spike_effect(const GameActor* actor) {
	draw_actor(
		actor, "missiles/spike_ball", FtFloat(VAL(actor, ARTILLERY_ANGLE)), ALPHA(VAL(actor, ARTILLERY_ALPHA)));
}

const GameActorTable TAB_SPIKE_BALL_EFFECT = {
	.load = load_spike_effect, .create = create_spike_effect, .tick = tick_spike_effect, .draw = draw_spike_effect};
