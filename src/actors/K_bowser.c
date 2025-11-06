#include "actors/K_autoscroll.h"
#include "actors/K_player.h"

// =====
// BOMZH
// =====

static void load() {
	load_texture("enemies/bomzh");

	load_track("smrpg_bowser");
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-60L);
	actor->box.start.y = FfInt(-218L);
	actor->box.end.y = FfInt(60L);
}

static void tick(GameActor* actor) {
	GameActor* nearest = nearest_pawn(actor->pos);
	if (nearest == NULL)
		return;

	if (nearest->pos.x < actor->pos.x)
		FLAG_ON(actor, FLG_X_FLIP);
	else if (nearest->pos.x > actor->pos.x)
		FLAG_OFF(actor, FLG_X_FLIP);

	GameActor* autoscroll = get_actor(game_state.autoscroll);
	if (autoscroll == NULL && nearest->pos.x > (game_state.size.x - FfInt(952L))) {
		autoscroll = create_actor(ACT_AUTOSCROLL, (fvec2){nearest->pos.x - F_HALF_SCREEN_WIDTH, FxZero});
		if (autoscroll != NULL) {
			VAL(autoscroll, X_SPEED) = FxOne;
			FLAG_ON(autoscroll, FLG_SCROLL_BOWSER);
			play_state_track(TS_MAIN, "smrpg_bowser", PLAY_LOOPING);
		}
	}
}

static void draw(const GameActor* actor) {
	draw_actor(actor, "enemies/bomzh", 0.f, WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER)
		return;

	GamePlayer* player = get_owner(from);
	if (player != NULL)
		player->lives = 0L;
	kill_player(from);
}

const GameActorTable TAB_BOWSER = {.load = load, .create = create, .tick = tick, .draw = draw, .collide = collide};
