#include "actors/K_missiles.h"

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

const GameActorTable TAB_MISSILE_FIREBALL = {.load = load_fireball,
	.create = create_fireball,
	.tick = tick_fireball,
	.draw = draw_fireball,
	.cleanup = cleanup,
	.owner = owner};
