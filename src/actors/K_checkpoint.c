#include "K_string.h"

#include "actors/K_checkpoint.h" // IWYU pragma: keep???
#include "actors/K_points.h"

static void load() {
	load_texture_num("markers/checkpoint%u", 3L);

	load_sound("sprout");

	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-19L);
	actor->box.start.y = FfInt(-110L);
	actor->box.end.x = FfInt(21L);
	actor->box.end.y = FxOne;

	actor->depth = FfInt(3L);
}

static void draw(const GameActor* actor) {
	const char* tex = "markers/checkpoint0";
	if (game_state.checkpoint == actor->id)
		tex = fmt("markers/checkpoint%u", 1L + ((game_state.time / 10L) % 2L));
	draw_actor(actor, tex, 0.f, B_ALPHA((actor->id >= game_state.checkpoint) ? 255L : 128L));
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || game_state.checkpoint >= actor->id)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	give_points(actor, player, 1000L);
	play_actor_sound(actor, "sprout");
	game_state.checkpoint = actor->id;

	if (numplayers() > 1L)
		hud_message(fmt("%s reached a checkpoint!", get_player_name(player->id)));

	if (VAL(actor, CHECKPOINT_BOUNDS_X1) == VAL(actor, CHECKPOINT_BOUNDS_X2)
		|| VAL(actor, CHECKPOINT_BOUNDS_Y1) == VAL(actor, CHECKPOINT_BOUNDS_Y2))
		return;
	if (player->bounds.start.x != VAL(actor, CHECKPOINT_BOUNDS_X1)
		|| player->bounds.start.y != VAL(actor, CHECKPOINT_BOUNDS_Y1)
		|| player->bounds.end.x != VAL(actor, CHECKPOINT_BOUNDS_X2)
		|| player->bounds.end.y != VAL(actor, CHECKPOINT_BOUNDS_Y2))
	{
		player->bounds.start.x = VAL(actor, CHECKPOINT_BOUNDS_X1),
		player->bounds.start.y = VAL(actor, CHECKPOINT_BOUNDS_Y1),
		player->bounds.end.x = VAL(actor, CHECKPOINT_BOUNDS_X2),
		player->bounds.end.y = VAL(actor, CHECKPOINT_BOUNDS_Y2);

		// !!! CLIENT-SIDE !!!
		if (viewplayer() == player->id)
			lerp_camera(25.f);
		// !!! CLIENT-SIDE !!!
	}
}

const GameActorTable TAB_CHECKPOINT = {.load = load, .create = create, .draw = draw, .collide = collide};
