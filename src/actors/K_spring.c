#include "actors/K_player.h" // IWYU pragma: keep

enum {
	VAL_SPRING_FRAME = VAL_CUSTOM,
};

enum {
	FLG_SPRING_GREEN = CUSTOM_FLAG(0),
};

static void load() {
	load_texture_num("markers/spring%u", 3L, FALSE);
	load_texture_num("markers/spring_green%u", 3L, FALSE);

	load_sound("spring", FALSE);
}

static void create(GameActor* actor) {
	actor->box.start.y = FxFrom(16L);
	actor->box.end.x = FxFrom(31L);
	actor->box.end.y = FxFrom(64L);

	actor->depth = FxFrom(2L);
}

static void tick(GameActor* actor) {
	if (VAL(actor, SPRING_FRAME) <= 0L)
		return;
	VAL(actor, SPRING_FRAME) += 25L;
	if (VAL(actor, SPRING_FRAME) >= 500L)
		VAL(actor, SPRING_FRAME) = 0L;
}

static void draw(const GameActor* actor) {
	const char* tex = NULL;
	switch (VAL(actor, SPRING_FRAME) / 100L) {
	default:
	case 0L:
		tex = ANY_FLAG(actor, FLG_SPRING_GREEN) ? "markers/spring_green0" : "markers/spring0";
		break;
	case 1L:
	case 3L:
		tex = ANY_FLAG(actor, FLG_SPRING_GREEN) ? "markers/spring_green1" : "markers/spring1";
		break;
	case 2L:
		tex = ANY_FLAG(actor, FLG_SPRING_GREEN) ? "markers/spring_green2" : "markers/spring2";
		break;
	}
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || from->pos.y > (actor->pos.y + FxFrom(63L)) || VAL(from, Y_SPEED) <= FxZero)
		return;

	VAL(from, Y_SPEED) = (VAL(from, PLAYER_SPRING) > 0L)
	                             ? (ANY_FLAG(actor, FLG_SPRING_GREEN) ? FxFrom(-33L) : FxFrom(-19L))
	                             : FxFrom(-10L);
	VAL(actor, SPRING_FRAME) = 1L;
	play_state_sound("spring", PLAY_POS, 0L, A_ACTOR(actor));
}

const GameActorTable TAB_SPRING = {.load = load, .create = create, .tick = tick, .draw = draw, .collide = collide};
