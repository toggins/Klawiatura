#include "K_audio.h"
#include "K_cmd.h"
#include "K_menu.h"
#include "K_secrets.h"
#include "K_string.h"
#include "K_tick.h"

extern MenuType cur_menu;

Secret SECRETS[SECR_SIZE] = {
	[SECR_KEVIN] = {
		.name = "KEVIN",
		.inputs = {KB_SECRET_K, KB_SECRET_E, KB_SECRET_V, KB_SECRET_I, KB_SECRET_N, NULLBIND},
		.cmd = &CLIENT.game.kevin,
		.sound = "kevin_on", .track = "human_like_predator",
	},
	[SECR_FRED] = {
		.name = "FRED",
		.inputs = {KB_SECRET_F, KB_SECRET_R, KB_SECRET_E, KB_SECRET_D, NULLBIND},
		.cmd = &CLIENT.game.fred,
		.sound = "clone_dead2", .track = "bassbin_bangin",
	},
};

static int secret_state = 0;
static int last_secret = -1;

void load_secrets() {
	for (SecretType i = 0; i < SECR_SIZE; i++) {
		load_transient_sound(SECRETS[i].sound);
		load_transient_track(SECRETS[i].track);
	}

	load_transient_sound("type");
	load_transient_sound("thwomp");
	load_transient_track("it_makes_me_burn");
}

static void activate_secret(SecretType idx) {
	SECRETS[idx].active = TRUE;
	SECRETS[idx].state = SECRETS[idx].type_time = 0;
	*(SECRETS[idx].cmd) = TRUE;

	if (last_secret == idx)
		last_secret = -1;
	play_generic_sound(SECRETS[idx].sound);
	update_menu_track();
	push_lobby_data();
}

static void deactivate_secret(SecretType idx) {
	SECRETS[idx].active = FALSE;
	SECRETS[idx].state = SECRETS[idx].type_time = 0;
	*(SECRETS[idx].cmd) = FALSE;

	if (last_secret == idx)
		last_secret = -1;
	update_menu_track();
	push_lobby_data();
}

static const char* keve() {
	if (last_secret >= 0) {
		static char buf[MAX_SECRET_INPUTS + 1] = "";
		SDL_strlcpy(buf, SECRETS[last_secret].name, SECRETS[last_secret].state + 1);
		return buf;
	}

	if (CLIENT.game.kevin && CLIENT.game.fred)
		return "NOSTALIGA";

	const char* str = NULL;
	for (SecretType i = 0; i < SECR_SIZE; i++)
		if (SECRETS[i].active)
			str = SECRETS[i].name;
	return str;
}

void update_secrets() {
	const Bool slave = is_in_netgame() && is_client(),
		   forbinned = cur_menu >= MEN_INGAME || cur_menu == MEN_INTRO || cur_menu == MEN_JOINING_LOBBY,
		   handicapped = slave || forbinned;

	for (SecretType i = 0; i < SECR_SIZE; i++) {
		Secret* secret = &SECRETS[i];

		if (secret->active) {
			if (!*secret->cmd)
				deactivate_secret(i);
			continue;
		} else if (*secret->cmd) {
			activate_secret(i);
			continue;
		}

		if (secret->type_time > 0)
			if (--secret->type_time <= 0) {
				secret->state = 0;
				if (last_secret == i)
					last_secret = -1;
				continue;
			}

		if (handicapped || (last_secret >= 0 && last_secret != i) || !kb_pressed(secret->inputs[secret->state]))
			continue;
		if (secret->inputs[++secret->state] != NULLBIND) {
			last_secret = i;
			secret->type_time = TICKRATE;
			play_generic_sound("type");
			continue;
		}

		activate_secret(i);
	}

	if (handicapped || !kb_pressed(KB_SECRET_BAIL))
		return;

	int cancelled = 0;
	for (SecretType i = 0; i < SECR_SIZE; i++) {
		if (!SECRETS[i].active)
			continue;
		deactivate_secret(i);
		++cancelled;
	}
	if (cancelled > 0)
		play_generic_sound("thwomp");
}

float secret_lerp = 0.f;

void draw_secrets(const float fade_red) {
	batch_pos(B_XY(HALF_SCREEN_WIDTH + ((fade_red >= 255) ? (SDL_rand(3) - SDL_rand(3)) : 0.f),
		SCREEN_HEIGHT - 48.f - ((fade_red >= 255) ? SDL_rand(5) : 0.f)));
	batch_color(B_RGB(255, 96, 96));
	batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
	const float kscale = 1.f + ((0.84f + SDL_sinf(totalticks() / 32.f) * 0.16f) * secret_lerp);
	batch_scale(B_SCALE(kscale));
	batch_string("main", 24.f, keve());

	if (cur_menu != MEN_JOINING_LOBBY && cur_menu != MEN_LOBBY) {
		batch_pos(B_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 48.f)), batch_scale(B_SCALE(1.f));
		batch_color(B_ALPHA(128 * secret_lerp)), batch_align(B_ALIGN(FA_CENTER, FA_TOP));
		batch_string("main", 24, fmt("[%s] Deactivate", kb_label(KB_SECRET_BAIL)));
	}
}
