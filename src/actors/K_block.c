#include "K_block.h"
#include "K_coin.h"
#include "K_game.h"
#include "K_powerup.h"

// ================
// HELPER FUNCTIONS
// ================

static void bump_block(GameActor* actor, GameActor* from, Bool strong) {
	if (actor == NULL)
		return;

	switch (actor->type) {
	default:
		return;
	case ACT_ITEM_BLOCK:
	case ACT_BRICK_BLOCK:
	case ACT_BRICK_BLOCK_COIN:
	case ACT_PSWITCH_BRICK:
	case ACT_NOTE_BLOCK:
		break;
	}

	if (ANY_FLAG(actor, FLG_BLOCK_EMPTY) || actor->values[VAL_BLOCK_BUMP] > 0L)
		return;

	GameActor* bump = create_actor(ACT_BLOCK_BUMP, actor->pos);
	if (bump != NULL)
		bump->values[VAL_BLOCK_PLAYER] = (ActorValue)get_owner_id(from);

	if ((actor->type == ACT_BRICK_BLOCK || actor->type == ACT_PSWITCH_BRICK)
		&& actor->values[VAL_BLOCK_ITEM] == ACT_NULL)
	{
		if (strong) {
			GameActor* shard = create_actor(ACT_BRICK_SHARD, POS_ADD(actor, FfInt(8L), FfInt(8L)));
			if (shard != NULL) {
				shard->values[VAL_X_SPEED] = FfInt(-2L), shard->values[VAL_Y_SPEED] = FfInt(-8L);
				FLAG_ON(shard, actor->flags & FLG_BLOCK_GRAY);
			}

			shard = create_actor(ACT_BRICK_SHARD, POS_ADD(actor, FfInt(16L), FfInt(8L)));
			if (shard != NULL) {
				shard->values[VAL_X_SPEED] = FfInt(2L), shard->values[VAL_Y_SPEED] = FfInt(-8L);
				FLAG_ON(shard, actor->flags & FLG_BLOCK_GRAY);
			}

			shard = create_actor(ACT_BRICK_SHARD, POS_ADD(actor, FfInt(8L), FfInt(16L)));
			if (shard != NULL) {
				shard->values[VAL_X_SPEED] = FfInt(-3L), shard->values[VAL_Y_SPEED] = FfInt(-6L);
				FLAG_ON(shard, actor->flags & FLG_BLOCK_GRAY);
			}

			shard = create_actor(ACT_BRICK_SHARD, POS_ADD(actor, FfInt(16L), FfInt(16L)));
			if (shard != NULL) {
				shard->values[VAL_X_SPEED] = FfInt(3L), shard->values[VAL_Y_SPEED] = FfInt(-6L);
				FLAG_ON(shard, actor->flags & FLG_BLOCK_GRAY);
			}

			GamePlayer* player = get_owner(from);
			if (player != NULL)
				player->score += 50L;

			play_actor_sound(actor, "break");
			FLAG_ON(actor, FLG_DESTROY);
			return;
		}

		play_actor_sound(actor, "bump");
	}

	Bool is_powerup = false;
	switch (actor->values[VAL_BLOCK_ITEM]) {
	default:
		break;

	case ACT_MUSHROOM:
	case ACT_FIRE_FLOWER:
	case ACT_BEETROOT:
	case ACT_LUI:
	case ACT_HAMMER_SUIT: {
		GamePlayer* player = get_owner(from);
		if (player != NULL && player->power == POW_SMALL)
			actor->values[VAL_BLOCK_ITEM] = ACT_MUSHROOM;
		else
			is_powerup = true;
		break;
	}
	}

	GameActor* item = (actor->values[VAL_BLOCK_ITEM] == ACT_NULL)
	                          ? NULL
	                          : create_actor(actor->values[VAL_BLOCK_ITEM], actor->pos);
	if (item != NULL) {
		move_actor(item, POS_ADD(actor,
					 Flerp(actor->box.start.x, actor->box.end.y, FxHalf)
						 - Flerp(item->box.start.x, item->box.end.x, FxHalf),
					 -item->box.end.y));
		skip_interp(item);

		if (item->type == ACT_COIN_POP) {
			if (from != NULL)
				item->values[VAL_COIN_POP_PLAYER] = (ActorValue)get_owner_id(from);
		} else {
			item->values[VAL_SPROUT] = 32L;
			switch (item->type) {
			default:
				break;

			case ACT_MUSHROOM:
			case ACT_MUSHROOM_1UP:
				item->values[VAL_X_SPEED] = FfInt(2L);
				break;

			case ACT_STARMAN:
				item->values[VAL_X_SPEED] = 163840L;
				break;
			}

			if (is_powerup)
				FLAG_OFF(item, FLG_POWERUP_FULL);
			play_actor_sound(item, "sprout");
		}
	}

	actor->values[VAL_BLOCK_BUMP] = 1L;
	if (actor->type == ACT_ITEM_BLOCK
		|| ((actor->type == ACT_BRICK_BLOCK || actor->type == ACT_PSWITCH_BRICK)
			&& actor->values[VAL_BLOCK_ITEM] != ACT_NULL)
		|| actor->values[VAL_BLOCK_TIME] > 300L)
		FLAG_ON(actor, FLG_BLOCK_EMPTY);
	else if (actor->values[VAL_BLOCK_TIME] <= 0L)
		actor->values[VAL_BLOCK_TIME] = 1L;
}

// ==========
// ITEM BLOCK
// ==========

static void load() {
	load_texture("items/block");
	load_texture("items/block2");
	load_texture("items/block3");
	load_texture("items/empty");

	load_sound("sprout");
}

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
	actor->depth = FfInt(20L);

	actor->values[VAL_BLOCK_ITEM] = ACT_NULL;
}

static void tick(GameActor* actor) {
	if (actor->values[VAL_BLOCK_BUMP] > 0L)
		if (++actor->values[VAL_BLOCK_BUMP] > 10L)
			actor->values[VAL_BLOCK_BUMP] = 0L;
}

static void draw(const GameActor* actor) {
	const char* tex;
	if (ANY_FLAG(actor, FLG_BLOCK_EMPTY))
		tex = "items/empty";
	else
		switch ((int)((float)(game_state.time) / 11.11111111111111f) % 4L) {
		default:
			tex = "items/block";
			break;
		case 1L:
		case 3L:
			tex = "items/block2";
			break;
		case 2L:
			tex = "items/block3";
			break;
		}

	int8_t bump;
	switch (actor->values[VAL_BLOCK_BUMP]) {
	default:
		bump = 0L;
		break;
	case 2L:
	case 9L:
		bump = 1L;
		break;
	case 3L:
	case 8L:
		bump = 2L;
		break;
	case 4L:
	case 7L:
		bump = 3L;
		break;
	case 5L:
	case 6L:
		bump = 4L;
		break;
	}

	batch_start(XYZ(FtInt(actor->pos.x), FtInt(actor->pos.y) - bump, FtFloat(actor->depth)), 0.f, WHITE);
	batch_sprite(tex, NO_FLIP);
}

static void on_bottom(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;

	GamePlayer* player = get_owner(from);
	bump_block(actor, from, player != NULL && player->power != POW_SMALL);
}

const GameActorTable TAB_ITEM_BLOCK = {
	.is_solid = always_solid, .load = load, .create = create, .tick = tick, .draw = draw, .on_bottom = on_bottom};

// ============
// HIDDEN BLOCK
// ============

static void load_hidden() {
	load_actor(ACT_ITEM_BLOCK);
}

static void collide_hidden(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER
		|| !(from->values[VAL_Y_SPEED] < FxZero
			&& ((from->pos.y + from->box.start.y) - from->values[VAL_Y_SPEED])
				   > (actor->pos.y + actor->box.end.y)))
		return;

	GameActor* block = create_actor(ACT_ITEM_BLOCK, actor->pos);
	if (block == NULL)
		return;

	move_actor(from, (fvec2){from->pos.x, (actor->pos.y + actor->box.end.y) - from->box.start.y});
	block->values[VAL_BLOCK_ITEM] = actor->values[VAL_BLOCK_ITEM];
	bump_block(block, from, false);
	FLAG_ON(actor, FLG_DESTROY);
}

const GameActorTable TAB_HIDDEN_BLOCK = {.load = load, .create = create, .collide = collide_hidden};

// ===========
// BRICK BLOCK
// ===========

static void load_brick() {
	load_texture("items/brick");
	load_texture("items/brick_gray");

	load_sound("bump");
	load_sound("break");

	load_actor(ACT_BRICK_SHARD);
}

static void draw_brick(const GameActor* actor) {
	int8_t bump;
	switch (actor->values[VAL_BLOCK_BUMP]) {
	default:
		bump = 0L;
		break;
	case 2L:
	case 10L:
		bump = 3L;
		break;
	case 3L:
	case 9L:
		bump = 6L;
		break;
	case 4L:
	case 8L:
		bump = 8L;
		break;
	case 5L:
	case 7L:
		bump = 10L;
		break;
	case 6L:
		bump = 11L;
		break;
	}
	batch_start(XYZ(FtInt(actor->pos.x), FtInt(actor->pos.y) - bump, FtFloat(actor->depth)), 0.f, WHITE);
	batch_sprite(ANY_FLAG(actor, FLG_BLOCK_GRAY) ? "items/brick_gray" : "items/brick", NO_FLIP);
}

const GameActorTable TAB_BRICK_BLOCK = {.is_solid = always_solid,
	.load = load_brick,
	.create = create,
	.tick = tick,
	.draw = draw_brick,
	.on_bottom = on_bottom};

// ==========
// COIN BLOCK
// ==========

static void load_coin_block() {
	load_texture("items/brick");
	load_texture("items/brick_gray");
	load_texture("items/empty");
}

static void tick_coin_block(GameActor* actor) {
	if (actor->values[VAL_BLOCK_TIME] > 0L && actor->values[VAL_BLOCK_TIME] <= 300L)
		++actor->values[VAL_BLOCK_TIME];
	tick(actor);
}

static void draw_coin_block(const GameActor* actor) {
	if (ANY_FLAG(actor, FLG_BLOCK_EMPTY))
		draw(actor);
	else
		draw_brick(actor);
}

const GameActorTable TAB_COIN_BLOCK = {.is_solid = always_solid,
	.load = load_coin_block,
	.create = create,
	.tick = tick_coin_block,
	.draw = draw_coin_block,
	.on_bottom = on_bottom};

// ==========
// BLOCK BUMP
// ==========

static void create_bump(GameActor* actor) {
	actor->box.end.x = FfInt(32L);
	actor->box.start.y = FfInt(-8L);

	actor->values[VAL_BLOCK_PLAYER] = (ActorValue)NULLPLAY;
}

static void tick_bump(GameActor* actor) {
	collide_actor(actor);
	if (++actor->values[VAL_BLOCK_BUMP] > 4L)
		FLAG_ON(actor, FLG_DESTROY);
}

static PlayerID bump_owner(const GameActor* actor) {
	return (PlayerID)actor->values[VAL_BLOCK_PLAYER];
}

const GameActorTable TAB_BLOCK_BUMP = {.create = create_bump, .tick = tick_bump, .owner = bump_owner};
