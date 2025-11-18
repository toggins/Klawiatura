#include "K_string.h"

#include "actors/K_enemies.h"
#include "actors/K_missiles.h"
#include "actors/K_player.h"

static void create(GameActor* actor) {
	VAL(actor, MISSILE_PLAYER) = (ActorValue)NULLPLAY;
}

static void cleanup(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if (player == NULL)
		return;

	for (ActorID i = 0; i < MAX_MISSILES; i++)
		if (player->missiles[i] == actor->id) {
			player->missiles[i] = NULLACT;
			break;
		}
	for (ActorID i = 0; i < MAX_SINK; i++)
		if (player->sink[i] == actor->id) {
			player->sink[i] = NULLACT;
			break;
		}
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type == ACT_PLAYER && get_owner(actor) == NULL)
		hit_player(from);
}

static PlayerID owner(const GameActor* actor) {
	return VAL(actor, MISSILE_PLAYER);
}

// ========
// FIREBALL
// ========

static void load_fireball() {
	load_texture("missiles/fireball");

	load_sound("bump");
	load_sound("kick");

	load_actor(ACT_EXPLODE);
}

static void create_fireball(GameActor* actor) {
	create(actor);

	actor->box.start.x = actor->box.start.y = FfInt(-7L);
	actor->box.end.x = actor->box.end.y = FfInt(8L);
}

static void tick_fireball(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if ((player != NULL && !in_player_view(actor, player, FxZero, true))
		|| (player == NULL && !in_any_view(actor, FxZero, true)))
	{
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, MISSILE_ANGLE) += (VAL(actor, X_SPEED) < FxZero) ? -12868L : 12868L;
	VAL(actor, Y_SPEED) += 26214L;

	displace_actor(actor, FfInt(10L), false);
	if (VAL(actor, X_TOUCH) != 0L)
		FLAG_ON(actor, FLG_DESTROY);
	else
		collide_actor(actor);
	if (ANY_FLAG(actor, FLG_DESTROY)) {
		align_interp(create_actor(ACT_EXPLODE, actor->pos), actor);
		return;
	}

	if (VAL(actor, Y_TOUCH) > 0L)
		VAL(actor, Y_SPEED) = FfInt(-5L);
}

static void draw_fireball(const GameActor* actor) {
	draw_actor(actor, "missiles/fireball", FtFloat(VAL(actor, MISSILE_ANGLE)), WHITE);
}

const GameActorTable TAB_MISSILE_FIREBALL = {
	.load = load_fireball,
	.create = create_fireball,
	.tick = tick_fireball,
	.draw = draw_fireball,
	.cleanup = cleanup,
	.collide = collide,
	.owner = owner,
};

// ========
// BEETROOT
// ========

static void load_beetroot() {
	load_texture("missiles/beetroot");

	load_sound("hurt");
	load_sound("bump");
	load_sound("kick");

	load_actor(ACT_EXPLODE);
	load_actor(ACT_POINTS);
}

static void create_beetroot(GameActor* actor) {
	create(actor);
	VAL(actor, MISSILE_HITS) = 3L;

	actor->box.start.x = FfInt(-11L);
	actor->box.start.y = FfInt(-32L);
	actor->box.end.x = FfInt(12L);
}

static void tick_beetroot(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if ((player != NULL && !in_player_view(actor, player, FxZero, true))
		|| (player == NULL && !in_any_view(actor, FxZero, true)))
	{
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if ((actor->pos.y + Fmin(VAL(actor, Y_SPEED), FxZero) - FxOne) > game_state.water) {
		if (!ANY_FLAG(actor, FLG_MISSILE_SINK)) {
			GamePlayer* player = get_owner(actor);
			if (player != NULL) {
				for (ActorID i = 0; i < MAX_MISSILES; i++)
					if (player->missiles[i] == actor->id) {
						player->missiles[i] = NULLACT;
						break;
					}
				for (ActorID i = 0; i < MAX_SINK; i++)
					if (get_actor(player->sink[i]) == NULL) {
						player->sink[i] = actor->id;
						break;
					}
			}

			VAL(actor, X_SPEED) = VAL(actor, Y_SPEED) = FxZero;
		}
		FLAG_ON(actor, FLG_MISSILE_SINK);
	} else {
		if (ANY_FLAG(actor, FLG_MISSILE_SINK)) {
			GamePlayer* player = get_owner(actor);
			if (player != NULL) {
				for (ActorID i = 0; i < MAX_SINK; i++)
					if (player->sink[i] == actor->id) {
						player->sink[i] = NULLACT;
						break;
					}
				for (ActorID i = 0; i < MAX_MISSILES; i++)
					if (get_actor(player->missiles[i]) == NULL) {
						player->missiles[i] = actor->id;
						break;
					}
			}
		}
		FLAG_OFF(actor, FLG_MISSILE_SINK);
	}

	if (ANY_FLAG(actor, FLG_MISSILE_SINK)) {
		move_actor(actor, (fvec2){actor->pos.x, actor->pos.y + FfInt(rng(3L))});

		if (VAL(actor, MISSILE_BUBBLE) < 20L && (game_state.time % 10L) == 0L
			&& in_any_view(actor, FxZero, false))
		{
			create_actor(ACT_BUBBLE, (fvec2){actor->pos.x, actor->pos.y - FfInt(3L)});
			++VAL(actor, MISSILE_BUBBLE);
		}
		return;
	}

	const fixed vx = VAL(actor, X_SPEED);
	VAL(actor, Y_SPEED) += 26214L;
	if (VAL(actor, MISSILE_HITS) > 0L) {
		if (VAL(actor, MISSILE_COOLDOWN) > 0L) {
			if (!touching_solid(HITBOX(actor), SOL_SOLID))
				--VAL(actor, MISSILE_COOLDOWN);
			VAL(actor, X_TOUCH) = VAL(actor, Y_TOUCH) = 0L;
			move_actor(actor, POS_SPEED(actor));
		} else
			displace_actor_soft(actor);

		collide_actor(actor);
	} else {
		move_actor(actor, POS_SPEED(actor));
		return;
	}

	if (!ANY_FLAG(actor, FLG_MISSILE_HIT)) {
		if (VAL(actor, X_TOUCH) != 0L || VAL(actor, Y_TOUCH) != 0L)
			play_actor_sound(actor, "hurt");
		else
			return;
	}
	FLAG_ON(actor, FLG_MISSILE_HIT);
	create_actor(ACT_EXPLODE, actor->pos);

	if (vx == VAL(actor, X_SPEED))
		VAL(actor, X_SPEED) = -VAL(actor, X_SPEED);
	VAL(actor, Y_SPEED) = FfInt(-8L);

	--VAL(actor, MISSILE_HITS);
	VAL(actor, MISSILE_COOLDOWN) = 2L;

	FLAG_OFF(actor, FLG_MISSILE_HIT);
}

static void draw_beetroot(const GameActor* actor) {
	draw_actor(actor, "missiles/beetroot", 0.f, WHITE);
}

const GameActorTable TAB_MISSILE_BEETROOT = {
	.load = load_beetroot,
	.create = create_beetroot,
	.tick = tick_beetroot,
	.draw = draw_beetroot,
	.cleanup = cleanup,
	.collide = collide,
	.owner = owner,
};

// ======
// HAMMER
// ======

static void load_hammer() {
	load_texture("missiles/hammer");

	load_sound("kick");

	load_actor(ACT_POINTS);
}

static void create_hammer(GameActor* actor) {
	create(actor);

	actor->box.start.x = FfInt(-13L);
	actor->box.start.y = FfInt(-18L);
	actor->box.end.x = FfInt(11L);
	actor->box.end.y = FfInt(15L);
}

static void tick_hammer(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if ((player != NULL && !in_player_view(actor, player, FxZero, true))
		|| (player == NULL && !in_any_view(actor, FfInt(128L), true)))
	{
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, MISSILE_ANGLE) += (VAL(actor, X_SPEED) < FxZero) ? -6434L : 6434L;
	VAL(actor, Y_SPEED) += 18725L;
	move_actor(actor, POS_SPEED(actor));
	collide_actor(actor);
}

static void draw_hammer(const GameActor* actor) {
	draw_actor(actor, "missiles/hammer", FtFloat(VAL(actor, MISSILE_ANGLE)), WHITE);
}

const GameActorTable TAB_MISSILE_HAMMER = {
	.load = load_hammer,
	.create = create_hammer,
	.tick = tick_hammer,
	.draw = draw_hammer,
	.cleanup = cleanup,
	.collide = collide,
	.owner = owner,
};

// =============
// SILVER HAMMER
// =============

static void load_silver() {
	load_texture("missiles/hammer_silver");

	load_sound("kick");

	load_actor(ACT_EXPLODE);
	load_actor(ACT_POINTS);
}

static void create_silver(GameActor* actor) {
	create_hammer(actor);
	VAL(actor, MISSILE_HITS) = 2L;
}

static void tick_silver(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if ((player != NULL && !in_player_view(actor, player, FxZero, true))
		|| (player == NULL && !in_any_view(actor, FfInt(128L), true)))
	{
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	VAL(actor, MISSILE_ANGLE) += (VAL(actor, X_SPEED) < FxZero) ? -45753L : 45753L;

	const fixed vx = VAL(actor, X_SPEED);
	VAL(actor, Y_SPEED) += 26214L;
	if (VAL(actor, MISSILE_HITS) > 0L) {
		if (VAL(actor, MISSILE_COOLDOWN) > 0L) {
			if (!touching_solid(HITBOX(actor), SOL_SOLID))
				--VAL(actor, MISSILE_COOLDOWN);
			VAL(actor, X_TOUCH) = VAL(actor, Y_TOUCH) = 0L;
			move_actor(actor, POS_SPEED(actor));
		} else
			displace_actor_soft(actor);

		collide_actor(actor);
	} else {
		move_actor(actor, POS_SPEED(actor));
		return;
	}

	if (!ANY_FLAG(actor, FLG_MISSILE_HIT)) {
		if (VAL(actor, X_TOUCH) != 0L || VAL(actor, Y_TOUCH) != 0L)
			play_actor_sound(actor, "kick");
		else
			return;
	}
	FLAG_ON(actor, FLG_MISSILE_HIT);
	create_actor(ACT_EXPLODE, actor->pos);

	if (vx == VAL(actor, X_SPEED))
		VAL(actor, X_SPEED) = -VAL(actor, X_SPEED);
	VAL(actor, Y_SPEED) = FfInt(-5L);

	--VAL(actor, MISSILE_HITS);
	VAL(actor, MISSILE_COOLDOWN) = 2L;

	FLAG_OFF(actor, FLG_MISSILE_HIT);
}

static void draw_silver(const GameActor* actor) {
	draw_actor(actor, "missiles/hammer_silver", FtFloat(VAL(actor, MISSILE_ANGLE)), WHITE);
}

const GameActorTable TAB_MISSILE_SILVER_HAMMER = {
	.load = load_silver,
	.create = create_silver,
	.tick = tick_silver,
	.draw = draw_silver,
	.cleanup = cleanup,
	.collide = collide,
	.owner = owner,
};

// ======
// NAPALM
// ======

static void load_napalm() {
	load_texture_num("missiles/napalm%u", 3L);
}

static void create_napalm(GameActor* actor) {
	create(actor);

	actor->box.start.x = FfInt(-16L);
	actor->box.start.y = FfInt(-15L);
	actor->box.end.x = FfInt(16L);
	actor->box.end.y = FfInt(15L);
}

static void tick_napalm(GameActor* actor) {
	GamePlayer* player = get_owner(actor);
	if ((player != NULL && !in_player_view(actor, player, FxZero, true))
		|| (player == NULL && !in_any_view(actor, FxZero, true)))
	{
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	if (ANY_FLAG(actor, FLG_MISSILE_SHIMMY)) {
		if (actor->pos.y > VAL(actor, MISSILE_HEIGHT))
			move_actor(actor,
				(fvec2){actor->pos.x, Fmax(actor->pos.y - FfInt(4L), VAL(actor, MISSILE_HEIGHT))});
		else if (actor->pos.y < VAL(actor, MISSILE_HEIGHT))
			move_actor(actor,
				(fvec2){actor->pos.x, Fmin(actor->pos.y + FfInt(4L), VAL(actor, MISSILE_HEIGHT))});
	}

	move_actor(actor, POS_SPEED(actor));
}

static void draw_napalm(const GameActor* actor) {
	draw_actor(actor, fmt("missiles/napalm%u", (game_state.time / 2L) % 3L), 0.f, WHITE);
}

static void collide_napalm(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		collide(actor, from);
		break;
	case ACT_MISSILE_FIREBALL:
		block_fireball(from);
		break;
	case ACT_MISSILE_BEETROOT:
		block_beetroot(from);
		break;
	}
}

const GameActorTable TAB_MISSILE_NAPALM = {
	.load = load_napalm,
	.create = create_napalm,
	.tick = tick_napalm,
	.draw = draw_napalm,
	.collide = collide_napalm,
	.owner = owner,
};
