#include "K_game.h"
#include "K_log.h"

static void load() {
	load_texture("items/coin");
	load_texture("items/coin2");
	load_texture("items/coin3");

	load_sound("coin");
}

static void create(GameActor* actor) {
	actor->depth = FxOne;

	actor->box.start.x = FfInt(6L);
	actor->box.start.y = FfInt(2L);
	actor->box.end.x = FfInt(25L);
	actor->box.end.y = FfInt(30L);
}

static void draw(const GameActor* actor) {
	const char* tex;
	switch ((game_state.time / 5) % 4) {
	default:
		tex = "items/coin";
		break;
	case 1:
	case 3:
		tex = "items/coin2";
		break;
	case 2:
		tex = "items/coin3";
		break;
	}
	draw_actor(actor, tex, 0, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;
	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	++player->coins;
	while (player->coins >= 100L) {
		++player->lives;
		player->coins -= 100L;
	}
	player->score += 200L;

	FLAG_ON(actor, FLG_DESTROY);
	INFO("Bye %i", actor->id);
}

const GameActorTable TAB_COIN = {.load = load, .create = create, .draw = draw, .collide = collide};
