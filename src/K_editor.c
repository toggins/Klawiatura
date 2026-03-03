#include "K_editor.h"
#include "K_input.h"
#include "K_video.h"

static Bool editing = FALSE, noclipping = FALSE;

Bool is_editing_level() {
	return editing && !is_in_netgame();
}

void set_editing_level(Bool value) {
	editing = value;
	if (!value)
		noclipping = FALSE;
}

Bool is_noclipping() {
	return is_editing_level() || noclipping;
}

void set_noclipping(Bool value) {
	noclipping = value;
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

Bool editor_baton_pass(GamePlayer* _player, GameActor* _actor) {
	player = _player, actor = _actor;
	get_cursor_pos(&mx, &my);

	if (is_noclipping())
		noclip();

	if (!is_editing_level())
		return is_noclipping(); // prevent further updates in a noclipping player even if we aren't utilizing
		                        // the full editor mode

	return TRUE;
}

void draw_editor() {
	float mx = 0.f, my = 0.f;
	get_cursor_pos(&mx, &my);

	batch_reset();
	batch_pos(B_XY(mx, my));
	batch_sprite("tiles/tank");
}
