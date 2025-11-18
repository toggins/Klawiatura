#include "K_string.h"

#include "actors/K_player.h" // IWYU pragma: keep

enum {
	VAL_SPRING_FRAME = VAL_CUSTOM,
};

enum {
	FLG_SPRING_GREEN = CUSTOM_FLAG(0),
};

static void load() {
	load_texture_num("markers/spring%u", 3L);
	load_texture_num("markers/spring_green%u", 3L);

	load_sound("spring");
}

static void create(GameActor* actor) {
	actor->box.start.y = FfInt(16L);
	actor->box.end.x = FfInt(31L);
	actor->box.end.y = FfInt(64L);

	actor->depth = FfInt(2L);
}

static void tick(GameActor* actor) {
	if (VAL(actor, SPRING_FRAME) <= 0L)
		return;
	VAL(actor, SPRING_FRAME) += 25L;
	if (VAL(actor, SPRING_FRAME) >= 500L)
		VAL(actor, SPRING_FRAME) = 0L;
}

static void draw(const GameActor* actor) {
	const char* tex = fmt(ANY_FLAG(actor, FLG_SPRING_GREEN) ? "markers/spring_green%u" : "markers/spring%u",
		VAL(actor, SPRING_FRAME) / 100L);
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
