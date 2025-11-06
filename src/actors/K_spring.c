#include "actors/K_player.h" // IWYU pragma: keep

enum {
	VAL_SPRING_FRAME = VAL_CUSTOM,
};

enum {
	FLG_SPRING_GREEN = CUSTOM_FLAG(0),
};

static void load() {
	load_texture("markers/spring");
	load_texture("markers/spring2");
	load_texture("markers/spring3");
	load_texture("markers/spring_green");
	load_texture("markers/spring_green2");
	load_texture("markers/spring_green3");

	load_sound("spring");
}

static void create(GameActor* actor) {
	actor->box.start.y = FfInt(16L);
	actor->box.end.x = FfInt(31L);
	actor->box.end.y = FfInt(64L);
}

static void tick(GameActor* actor) {
	if (VAL(actor, SPRING_FRAME) <= 0L)
		return;
	VAL(actor, SPRING_FRAME) += 25L;
	if (VAL(actor, SPRING_FRAME) >= 500L)
		VAL(actor, SPRING_FRAME) = 0L;
}

static void draw(const GameActor* actor) {
	const char* tex;
	switch (VAL(actor, SPRING_FRAME) / 100L) {
	default:
		tex = ANY_FLAG(actor, FLG_SPRING_GREEN) ? "markers/spring_green" : "markers/spring";
		break;
	case 1L:
	case 3L:
		tex = ANY_FLAG(actor, FLG_SPRING_GREEN) ? "markers/spring_green2" : "markers/spring2";
		break;
	case 2L:
		tex = ANY_FLAG(actor, FLG_SPRING_GREEN) ? "markers/spring_green3" : "markers/spring3";
		break;
	}

	draw_actor(actor, tex, 0.f, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || from->pos.y > (actor->pos.y + FfInt(63L)) || VAL(from, Y_SPEED) <= FxZero)
		return;

	VAL(from, Y_SPEED) = (VAL(from, PLAYER_SPRING) > 0L)
	                             ? (ANY_FLAG(actor, FLG_SPRING_GREEN) ? FfInt(-33L) : FfInt(-19L))
	                             : FfInt(-10L);
	VAL(actor, SPRING_FRAME) = 1L;
	play_actor_sound(actor, "spring");
}

const GameActorTable TAB_SPRING = {.load = load, .create = create, .tick = tick, .draw = draw, .collide = collide};
