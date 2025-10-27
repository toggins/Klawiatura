#include "K_checkpoint.h"
#include "K_points.h"

static void load() {
	load_texture("markers/checkpoint");
	load_texture("markers/checkpoint2");
	load_texture("markers/checkpoint3");

	load_sound("sprout");

	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-19L);
	actor->box.start.y = FfInt(-110L);
	actor->box.end.x = FfInt(21L);
	actor->box.end.y = FxOne;

	actor->depth = FfInt(2L);
}

static void draw(const GameActor* actor) {
	draw_actor(actor,
		(game_state.checkpoint == actor->id)
			? (((game_state.time / 10L) % 2L) ? "markers/checkpoint3" : "markers/checkpoint2")
			: "markers/checkpoint",
		0.f, ALPHA(actor->id >= game_state.checkpoint ? 255 : 128));
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
		if (viewplayer() == player->id) {
			// lerp_camera(500);
		}
		// !!! CLIENT-SIDE !!!
	}
}

const GameActorTable TAB_CHECKPOINT = {.load = load, .create = create, .draw = draw, .collide = collide};
