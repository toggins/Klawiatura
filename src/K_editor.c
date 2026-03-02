#include "K_editor.h"
#include "K_input.h"
#include "K_video.h"

static Bool editing = FALSE;

Bool is_editing_level() {
	return editing && !is_in_netgame();
}

void set_editing_level(Bool value) {
	editing = value;
}

void load_editor() {
	load_texture("tiles/tank");
}

static GamePlayer* player = NULL;
static GameActor* actor = NULL;
static float mx = 0.f, my = 0.f;

static void noclip() {
	const Fixed dx = FxFrom(ANY_INPUT(player, GI_RIGHT) - ANY_INPUT(player, GI_LEFT)),
		    dy = FxFrom(ANY_INPUT(player, GI_DOWN) - ANY_INPUT(player, GI_UP));
	const Fixed angle = FxAtan2(dy, dx);

	Fixed speed = ANY_INPUT(player, GI_RUN) ? FxFrom(20L) : FxFrom(8L);
	if (!dx && !dy)
		speed = FxZero;

	VAL(actor, X_SPEED) = FxMul(speed, FxCos(angle)), VAL(actor, Y_SPEED) = FxMul(speed, FxSin(angle));
	move_actor(actor, POS_SPEED(actor));
}

void editor_baton_pass(GamePlayer* _player, GameActor* _actor) {
	player = _player, actor = _actor;
	get_cursor_pos(&mx, &my);

	noclip();
}

void draw_editor() {
	float mx = 0.f, my = 0.f;
	get_cursor_pos(&mx, &my);

	batch_reset();
	batch_pos(B_XY(mx, my));
	batch_sprite("tiles/tank");
}
