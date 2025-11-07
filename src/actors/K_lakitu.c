#include "actors/K_enemies.h" // IWYU pragma: keep

static void load() {
	load_texture_wild("enemies/lakitu??");
	load_texture_wild("enemies/lakitu_cloud?");
	load_texture_wild("enemies/lakitu_throw??");

	load_sound("lakitu");
	load_sound("lakitu2");
	load_sound("lakitu3");
	load_sound("stomp");
	load_sound("kick");

	load_actor(ACT_SPINY_EGG);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-48L);
	actor->box.end.x = FfInt(15L);
}
