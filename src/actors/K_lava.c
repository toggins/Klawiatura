#include "actors/K_player.h"

// ====
// LAVA
// ====

static void load() {
	load_texture("enemies/lava");
	load_texture("enemies/lava2");
	load_texture("enemies/lava3");
	load_texture("enemies/lava4");
	load_texture("enemies/lava5");
	load_texture("enemies/lava6");
	load_texture("enemies/lava7");
	load_texture("enemies/lava8");
}

static void create(GameActor* actor) {
	actor->box.end.x = actor->box.end.y = FfInt(32L);
}

static void draw(const GameActor* actor) {
	const char* tex;
	switch ((int)((float)game_state.time / 9.090909090909091f) % 8L) {
	default:
		tex = "enemies/lava";
		break;
	case 1L:
		tex = "enemies/lava2";
		break;
	case 2L:
		tex = "enemies/lava3";
		break;
	case 3L:
		tex = "enemies/lava4";
		break;
	case 4L:
		tex = "enemies/lava5";
		break;
	case 5L:
		tex = "enemies/lava6";
		break;
	case 6L:
		tex = "enemies/lava7";
		break;
	case 7L:
		tex = "enemies/lava8";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type == ACT_PLAYER)
		kill_player(from);
}

const GameActorTable TAB_LAVA = {.load = load, .create = create, .draw = draw, .collide = collide};
