#include "K_game.h"

#include "actors/K_block.h"
#include "actors/K_coin.h"   // IWYU pragma: keep
#include "actors/K_player.h" // IWYU pragma: keep
#include "actors/K_powerups.h"

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
	case ACT_COIN_BLOCK:
	case ACT_PSWITCH_BRICK:
	case ACT_NOTE_BLOCK:
		break;
	}

	if (ANY_FLAG(actor, FLG_BLOCK_EMPTY) && actor->type != ACT_NOTE_BLOCK)
		return;

	GamePlayer* player = get_owner(from);
	if (player != NULL) {
		GameActor* bump = create_actor(ACT_BLOCK_BUMP, actor->pos);
		if (bump != NULL)
			VAL(bump, BLOCK_PLAYER) = (ActorValue)player->id;
	}

	switch (actor->type) {
	default:
		break;

	case ACT_BRICK_BLOCK:
	case ACT_PSWITCH_BRICK: {
		if (!strong) {
			play_actor_sound(actor, "bump");
			break;
		}

		GameActor* shard = create_actor(ACT_BRICK_SHARD, POS_ADD(actor, FfInt(8L), FfInt(8L)));
		if (shard != NULL) {
			VAL(shard, X_SPEED) = FfInt(-2L), VAL(shard, Y_SPEED) = FfInt(-8L);
			FLAG_ON(shard, actor->flags & FLG_BLOCK_GRAY);
		}

		shard = create_actor(ACT_BRICK_SHARD, POS_ADD(actor, FfInt(16L), FfInt(8L)));
		if (shard != NULL) {
			VAL(shard, X_SPEED) = FfInt(2L), VAL(shard, Y_SPEED) = FfInt(-8L);
			FLAG_ON(shard, actor->flags & FLG_BLOCK_GRAY);
		}

		shard = create_actor(ACT_BRICK_SHARD, POS_ADD(actor, FfInt(8L), FfInt(16L)));
		if (shard != NULL) {
			VAL(shard, X_SPEED) = FfInt(-3L), VAL(shard, Y_SPEED) = FfInt(-6L);
			FLAG_ON(shard, actor->flags & FLG_BLOCK_GRAY);
		}

		shard = create_actor(ACT_BRICK_SHARD, POS_ADD(actor, FfInt(16L), FfInt(16L)));
		if (shard != NULL) {
			VAL(shard, X_SPEED) = FfInt(3L), VAL(shard, Y_SPEED) = FfInt(-6L);
			FLAG_ON(shard, actor->flags & FLG_BLOCK_GRAY);
		}

		if (player != NULL)
			player->score += 50L;

		play_actor_sound(actor, "break");
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	case ACT_NOTE_BLOCK: {
		VAL(actor, X_TOUCH) = 0L, VAL(actor, Y_TOUCH) = -1L;
		play_actor_sound(actor, "bump");
		break;
	}
	}

	Bool is_powerup = false;
	switch (VAL(actor, BLOCK_ITEM)) {
	default:
		break;

	case ACT_MUSHROOM:
	case ACT_FIRE_FLOWER:
	case ACT_BEETROOT:
	case ACT_LUI:
	case ACT_HAMMER_SUIT: {
		if (player != NULL && player->power == POW_SMALL)
			VAL(actor, BLOCK_ITEM) = ACT_MUSHROOM;
		else
			is_powerup = true;
		break;
	}
	}

	GameActor* item = (VAL(actor, BLOCK_ITEM) == ACT_NULL || ANY_FLAG(actor, FLG_BLOCK_EMPTY))
	                          ? NULL
	                          : create_actor(VAL(actor, BLOCK_ITEM), actor->pos);
	if (item != NULL) {
		move_actor(item, POS_ADD(actor,
					 Flerp(actor->box.start.x, actor->box.end.y, FxHalf)
						 - Flerp(item->box.start.x, item->box.end.x, FxHalf),
					 -item->box.end.y));

		if (item->type == ACT_COIN_POP) {
			if (from != NULL)
				VAL(item, COIN_POP_PLAYER) = (ActorValue)get_owner_id(from);
		} else {
			VAL(item, SPROUT) = 32L;
			switch (item->type) {
			default:
				break;

			case ACT_MUSHROOM:
			case ACT_MUSHROOM_1UP:
				VAL(item, X_SPEED) = FfInt(2L);
				break;

			case ACT_STARMAN:
				VAL(item, X_SPEED) = 163840L;
				break;
			}

			if (is_powerup)
				FLAG_OFF(item, FLG_POWERUP_FULL);
			play_actor_sound(item, "sprout");
		}
	}

	VAL(actor, BLOCK_BUMP) = 1L;
	if (actor->type == ACT_ITEM_BLOCK
		|| ((actor->type == ACT_BRICK_BLOCK || actor->type == ACT_PSWITCH_BRICK
			    || actor->type == ACT_NOTE_BLOCK)
			&& VAL(actor, BLOCK_ITEM) != ACT_NULL)
		|| VAL(actor, BLOCK_TIME) > 300L)
		FLAG_ON(actor, FLG_BLOCK_EMPTY);
	else if (VAL(actor, BLOCK_TIME) <= 0L)
		VAL(actor, BLOCK_TIME) = 1L;
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

	load_actor(ACT_COIN_POP);
}

static void create(GameActor* actor) {
	actor->box.start.x = actor->box.start.y = FxZero;
	actor->box.end.x = actor->box.end.y = FfInt(32L);
	actor->depth = FfInt(20L);

	VAL(actor, BLOCK_ITEM) = ACT_NULL;
}

static void tick(GameActor* actor) {
	if (VAL(actor, BLOCK_BUMP) > 0L)
		if (++VAL(actor, BLOCK_BUMP) > 10L)
			VAL(actor, BLOCK_BUMP) = 0L;
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
	switch (VAL(actor, BLOCK_BUMP)) {
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
	switch (from->type) {
	default:
		break;

	case ACT_MISSILE_BEETROOT:
	case ACT_MISSILE_SILVER_HAMMER:
		bump_block(actor, from, true);
		break;

	case ACT_PLAYER: {
		GamePlayer* player = get_owner(from);
		bump_block(actor, from, player != NULL && player->power != POW_SMALL);
		break;
	}
	}
}

static void on_any_other_side(GameActor* actor, GameActor* from) {
	if (from->type == ACT_MISSILE_BEETROOT || from->type == ACT_MISSILE_SILVER_HAMMER)
		bump_block(actor, from, true);
}

const GameActorTable TAB_ITEM_BLOCK = {
	.is_solid = always_solid,
	.load = load,
	.create = create,
	.tick = tick,
	.draw = draw,
	.on_top = on_any_other_side,
	.on_bottom = on_bottom,
	.on_left = on_any_other_side,
	.on_right = on_any_other_side,
};

// ============
// HIDDEN BLOCK
// ============

static void load_hidden() {
	load_actor(ACT_ITEM_BLOCK);
}

static void collide_hidden(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER
		|| !(VAL(from, Y_SPEED) < FxZero
			&& ((from->pos.y + from->box.start.y) - VAL(from, Y_SPEED) + FxOne)
				   > (actor->pos.y + actor->box.end.y)))
		return;

	GameActor* block = create_actor(ACT_ITEM_BLOCK, actor->pos);
	if (block == NULL)
		return;

	move_actor(from, (fvec2){from->pos.x, (actor->pos.y + actor->box.end.y) - from->box.start.y});
	VAL(block, BLOCK_ITEM) = VAL(actor, BLOCK_ITEM);
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
	switch (VAL(actor, BLOCK_BUMP)) {
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

const GameActorTable TAB_BRICK_BLOCK = {
	.is_solid = always_solid,
	.load = load_brick,
	.create = create,
	.tick = tick,
	.draw = draw_brick,
	.on_top = on_any_other_side,
	.on_bottom = on_bottom,
	.on_left = on_any_other_side,
	.on_right = on_any_other_side,
}, TAB_PSWITCH_BRICK = {
	.is_solid = always_solid,
	.load = load_brick,
	.create = create,
	.tick = tick,
	.draw = draw_brick,
	.on_top = on_any_other_side,
	.on_bottom = on_bottom,
	.on_left = on_any_other_side,
	.on_right = on_any_other_side,
};

// ==========
// COIN BLOCK
// ==========

static void load_coin_block() {
	load_texture("items/brick");
	load_texture("items/brick_gray");
	load_texture("items/empty");
}

static void tick_coin_block(GameActor* actor) {
	if (VAL(actor, BLOCK_TIME) > 0L && VAL(actor, BLOCK_TIME) <= 300L)
		++VAL(actor, BLOCK_TIME);
	tick(actor);
}

static void draw_coin_block(const GameActor* actor) {
	if (ANY_FLAG(actor, FLG_BLOCK_EMPTY))
		draw(actor);
	else
		draw_brick(actor);
}

const GameActorTable TAB_COIN_BLOCK = {
	.is_solid = always_solid,
	.load = load_coin_block,
	.create = create,
	.tick = tick_coin_block,
	.draw = draw_coin_block,
	.on_top = on_any_other_side,
	.on_bottom = on_bottom,
	.on_left = on_any_other_side,
	.on_right = on_any_other_side,
};

// ==========
// NOTE BLOCK
// ==========

static Bool note_solid(const GameActor* actor) {
	return !ANY_FLAG(actor, FLG_BLOCK_HIDDEN) * SOL_SOLID;
}

static void load_note() {
	load_texture("items/note");
	load_texture("items/note2");
	load_texture("items/note3");

	load_sound("bump");
	load_sound("spring");
	load_sound("sprout");
}

static void draw_note(const GameActor* actor) {
	if (ANY_FLAG(actor, FLG_BLOCK_HIDDEN))
		return;

	const char* tex;
	switch ((int)((float)(game_state.time) / 7.69230769231f) % 4L) {
	default:
		tex = "items/note2";
		break;
	case 1L:
	case 3L:
		tex = "items/note";
		break;
	case 2L:
		tex = "items/note3";
		break;
	}

	int8_t bump;
	switch (VAL(actor, BLOCK_BUMP)) {
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

	const int8_t bx = (int8_t)(VAL(actor, X_TOUCH) * bump), by = (int8_t)(VAL(actor, Y_TOUCH) * bump);
	batch_start(XYZ(FtInt(actor->pos.x) + bx, FtInt(actor->pos.y) + by, FtFloat(actor->depth)), 0.f, WHITE);
	batch_sprite(tex, NO_FLIP);
}

static void note_top(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER:
	case ACT_MISSILE_BEETROOT:
	case ACT_MISSILE_SILVER_HAMMER: {
		VAL(from, Y_SPEED) = (VAL(from, PLAYER_SPRING) > 0L) ? FfInt(-19L) : FfInt(-10L);
		VAL(actor, BLOCK_BUMP) = 1L;
		VAL(actor, X_TOUCH) = 0L, VAL(actor, Y_TOUCH) = 1L;
		play_actor_sound(actor, "spring");
		break;
	}
	}
}

static void note_left(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER:
	case ACT_MISSILE_BEETROOT:
	case ACT_MISSILE_SILVER_HAMMER: {
		VAL(from, X_SPEED) = FfInt(-5L);
		VAL(actor, BLOCK_BUMP) = 1L;
		VAL(actor, X_TOUCH) = 1L, VAL(actor, Y_TOUCH) = 0L;
		play_actor_sound(actor, "bump");
		break;
	}
	}
}

static void note_right(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER:
	case ACT_MISSILE_BEETROOT:
	case ACT_MISSILE_SILVER_HAMMER: {
		VAL(from, X_SPEED) = FfInt(5L);
		VAL(actor, BLOCK_BUMP) = 1L;
		VAL(actor, X_TOUCH) = -1L, VAL(actor, Y_TOUCH) = 0L;
		play_actor_sound(actor, "bump");
		break;
	}
	}
}

static void collide_note(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;

	if ((from->pos.x + from->box.end.x - Fmax(FxZero, VAL(from, X_SPEED)) - FxOne)
		<= (actor->pos.x + actor->box.start.x))
	{
		move_actor(from, (fvec2){actor->pos.x + actor->box.start.x - from->box.end.x, from->pos.y});
		FLAG_OFF(actor, FLG_BLOCK_HIDDEN);
	} else if ((from->pos.x + from->box.start.x - Fmin(FxZero, VAL(from, X_SPEED)) + FxOne)
		   >= (actor->pos.x + actor->box.end.x))
	{
		move_actor(from, (fvec2){actor->pos.x + actor->box.end.x - from->box.start.x, from->pos.y});
		FLAG_OFF(actor, FLG_BLOCK_HIDDEN);
	}

	if ((from->pos.y + from->box.end.y - Fmax(FxZero, VAL(from, Y_SPEED)) - FxOne)
		<= (actor->pos.y + actor->box.start.y))
	{
		move_actor(from, (fvec2){from->pos.x, actor->pos.y + actor->box.start.y - from->box.end.y});
		FLAG_OFF(actor, FLG_BLOCK_HIDDEN);
	} else if ((from->pos.y + from->box.start.y - Fmin(FxZero, VAL(from, Y_SPEED)) + FxOne)
		   >= (actor->pos.y + actor->box.end.y))
	{
		move_actor(from, (fvec2){from->pos.x, actor->pos.y + actor->box.end.y - from->box.start.y});
		FLAG_OFF(actor, FLG_BLOCK_HIDDEN);
	}
}

const GameActorTable TAB_NOTE_BLOCK = {
	.is_solid = note_solid,
	.load = load_note,
	.create = create,
	.tick = tick,
	.draw = draw_note,
	.collide = collide_note,
	.on_top = note_top,
	.on_bottom = on_bottom,
	.on_left = note_left,
	.on_right = note_right,
};

// ==========
// BLOCK BUMP
// ==========

static void create_bump(GameActor* actor) {
	actor->box.end.x = FfInt(32L);
	actor->box.start.y = FfInt(-8L);

	VAL(actor, BLOCK_PLAYER) = (ActorValue)NULLPLAY;
}

static void tick_bump(GameActor* actor) {
	collide_actor(actor);
	if (++VAL(actor, BLOCK_BUMP) > 4L)
		FLAG_ON(actor, FLG_DESTROY);
}

static PlayerID bump_owner(const GameActor* actor) {
	return (PlayerID)VAL(actor, BLOCK_PLAYER);
}

const GameActorTable TAB_BLOCK_BUMP = {
	.create = create_bump,
	.tick = tick_bump,
	.owner = bump_owner,
};
