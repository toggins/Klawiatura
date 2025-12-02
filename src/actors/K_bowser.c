#include "K_string.h"
#include "K_tick.h"

#include "actors/K_autoscroll.h"
#include "actors/K_bowser.h"
#include "actors/K_missiles.h"
#include "actors/K_player.h"

// ======
// BOWSER
// ======

static void load() {
	load_texture_num("enemies/bowser%u", 3L, false);
	load_texture_num("enemies/bowser_jump%u", 2L, false);
	load_texture_num("enemies/bowser_fire%u", 2L, false);
	load_texture_num("ui/bowser%u", 3L, false);

	load_sound("bowser_fire", false);
	load_sound("bowser_hurt", false);
	load_sound("kick", false);

	load_actor(ACT_MISSILE_NAPALM);
	load_actor(ACT_BOWSER_DEAD);
}

static void load_special(const GameActor* actor) {
	if (ANY_FLAG(actor, FLG_BOWSER_CHARGE))
		load_texture_num("enemies/bowser_charge%u", 8L, false);

	if (ANY_FLAG(actor, FLG_BOWSER_GUN)) {
		load_texture("enemies/bowser_gun", false);

		load_sound("bang", false);
		load_sound("bang4", false);

		load_actor(ACT_BULLET_BILL);
		load_actor(ACT_EXPLODE);
	}

	if (ANY_FLAG(actor, FLG_BOWSER_GIGA))
		load_track("smrpg_smithy", false);
	else if (ANY_FLAG(actor, FLG_BOWSER_DEVASTATOR)
		 || ALL_FLAG(actor, FLG_BOWSER_CHARGE | FLG_BOWSER_VOMIT | FLG_BOWSER_HAMMER))
		load_track("yi_bowser", false);
	else if (game_state.flags & GF_LOST)
		load_track("smb3_bowser", false);
	else
		load_track("smrpg_bowser", false);

	if (game_state.flags & GF_LOST)
		load_track("win3", false);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-30L);
	actor->box.start.y = FfInt(-70L);
	actor->box.end.x = FfInt(30L);
	actor->depth = FxOne;

	VAL(actor, BOWSER_SLIPPER) = NULLACT;
}

static void tick(GameActor* actor) {
	// Animation
	if (!ANY_FLAG(actor, FLG_BOWSER_ANIMS)) {
		VAL(actor, BOWSER_FRAME) += 30L;
		if (VAL(actor, BOWSER_GROUND) <= 0L) {
			VAL(actor, BOWSER_FRAME) = 0L;
			FLAG_OFF(actor, FLG_BOWSER_ANIMS);
			FLAG_ON(actor, FLG_BOWSER_ANIM_JUMP);
		}
	} else if (ANY_FLAG(actor, FLG_BOWSER_ANIM_JUMP)) {
		if (VAL(actor, BOWSER_FRAME) < 200L)
			VAL(actor, BOWSER_FRAME) += 50L;
		if (VAL(actor, BOWSER_GROUND) > 0L) {
			VAL(actor, BOWSER_FRAME) = 0L;
			FLAG_OFF(actor, FLG_BOWSER_ANIMS);
		}
	} else if (ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE_START))
		VAL(actor, BOWSER_FRAME) += 14L;
	else if (ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE_START2))
		VAL(actor, BOWSER_FRAME) += 100L;
	else if (ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE)) {
		VAL(actor, BOWSER_FRAME) += 20L;
		if (VAL(actor, BOWSER_FRAME) >= 200L) {
			VAL(actor, BOWSER_FRAME) = 0L;
			FLAG_OFF(actor, FLG_BOWSER_ANIMS);
		}
	}

	// Target, Trigger
	GameActor* nearest = nearest_pawn(actor->pos);
	if (nearest != NULL) {
		if (nearest->pos.x < actor->pos.x)
			FLAG_ON(actor, FLG_X_FLIP);
		else if (nearest->pos.x > actor->pos.x)
			FLAG_OFF(actor, FLG_X_FLIP);

		GameActor* autoscroll = get_actor(game_state.autoscroll);
		if (autoscroll == NULL && nearest->pos.x > (game_state.size.x - FfInt(952L))) {
			autoscroll
				= create_actor(ACT_AUTOSCROLL, (fvec2){nearest->pos.x - F_HALF_SCREEN_WIDTH, FxZero});
			if (autoscroll != NULL) {
				VAL(autoscroll, X_SPEED) = FxOne;
				FLAG_ON(autoscroll, FLG_SCROLL_BOWSER);

				const char* track = "smrpg_bowser";
				if (ANY_FLAG(actor, FLG_BOWSER_GIGA))
					track = "smrpg_smithy";
				else if (ANY_FLAG(actor, FLG_BOWSER_DEVASTATOR)
					 || ALL_FLAG(actor, FLG_BOWSER_CHARGE | FLG_BOWSER_VOMIT | FLG_BOWSER_HAMMER))
					track = "yi_bowser";
				else if (game_state.flags & GF_LOST)
					track = "smb3_bowser";
				play_state_track(TS_MAIN, track, PLAY_LOOPING);
			}
		}
	}

	//
	// Events from "Hardcore 1-4"
	//

	// 886
	if ((game_state.time % 50L) == 0L && ANY_FLAG(actor, FLG_BOWSER_ACTIVE))
		VAL(actor, BOWSER_C) = rng(64L);

	// 887
	if (!ANY_FLAG(actor, FLG_BOWSER_ACTIVE) && in_any_view(actor, FxZero, false)) {
		FLAG_ON(actor, FLG_BOWSER_ACTIVE);
		VAL(actor, BOWSER_HEIGHT) = actor->pos.y;
	}

	// 888
	if (ANY_FLAG(actor, FLG_BOWSER_ACTIVE) && !ANY_FLAG(actor, FLG_BOWSER_1) && (game_state.time % 50L) == 0L)
		VAL(actor, BOWSER_C) = rng(128L);

	// 889, 890
	if (VAL(actor, BOWSER_C) > 0L) {
		VAL(actor, X_SPEED) = ANY_FLAG(actor, FLG_BOWSER_1) ? FfInt(2L) : FfInt(-2L);
		--VAL(actor, BOWSER_C);
	} else
		VAL(actor, X_SPEED) = FxZero;

	// 891, 892
	if ((!ANY_FLAG(actor, FLG_BOWSER_1) && (actor->pos.x + actor->box.start.x) < (game_state.size.x - FfInt(560L)))
		|| (ANY_FLAG(actor, FLG_BOWSER_1)
			&& (actor->pos.x + actor->box.end.x) > (game_state.size.x - FfInt(80L))))
		TOGGLE_FLAG(actor, FLG_BOWSER_1);

	// 893
	if (((game_state.time * 2L) % 15L) == 0L && ANY_FLAG(actor, FLG_BOWSER_ACTIVE))
		VAL(actor, BOWSER_D) = rng(20L);

	// 894
	if (VAL(actor, BOWSER_D) == 10L && VAL(actor, BOWSER_GROUND) > 0L)
		VAL(actor, Y_SPEED) = FfInt(-6L);

	// 905
	if (ANY_FLAG(actor, FLG_BOWSER_ACTIVE)
		&& !ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE_START | FLG_BOWSER_ANIM_FIRE_START2))
		++VAL(actor, BOWSER_F);

	// 906, 907
	if (VAL(actor, BOWSER_F) > 150L) {
		VAL(actor, BOWSER_F) = 0L;
		VAL(actor, BOWSER_FRAME) = 0L;
		FLAG_OFF(actor, FLG_BOWSER_ANIMS);
		FLAG_ON(actor, FLG_BOWSER_10
				       | ((VAL(actor, BOWSER_K) < 5L) ? FLG_BOWSER_ANIM_FIRE_START
								      : FLG_BOWSER_ANIM_FIRE_START2));
	}

	// 908, 909, 912, 915
	if (ANY_FLAG(actor, FLG_BOWSER_10)
		&& ((ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE_START) && VAL(actor, BOWSER_FRAME) >= 600L)
			|| (ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE_START2) && VAL(actor, BOWSER_FRAME) >= 6800L)))
	{
		VAL(actor, BOWSER_FRAME) = 0L;
		FLAG_OFF(actor, FLG_BOWSER_10 | FLG_BOWSER_ANIMS);
		FLAG_ON(actor, FLG_BOWSER_ANIM_FIRE);
		if (VAL(actor, BOWSER_K) < 5L) {
			GameActor* napalm = create_actor(ACT_MISSILE_NAPALM,
				POS_ADD(actor, ANY_FLAG(actor, FLG_X_FLIP) ? FfInt(-18L) : FfInt(18L), FfInt(-32L)));
			if (napalm != NULL) {
				FLAG_ON(napalm, (actor->flags & FLG_X_FLIP) | FLG_MISSILE_SHIMMY);
				VAL(napalm, X_SPEED) = Fmul(ANY_FLAG(napalm, FLG_X_FLIP) ? FfInt(-4L) : FfInt(4L),
					VAL(actor, BOWSER_MISSILE_SPEED));
				VAL(napalm, MISSILE_HEIGHT) = VAL(actor, BOWSER_HEIGHT) - FfInt(28L + (rng(3L) * 36L));
			}

			++VAL(actor, BOWSER_K);
		} else {
			const ActorFlag mflags = (actor->flags & FLG_X_FLIP) | FLG_MISSILE_SHIMMY;
			const fvec2 mpos
				= POS_ADD(actor, (mflags & FLG_X_FLIP) ? FfInt(-18L) : FfInt(18L), FfInt(-32L));
			const fixed mspd = Fmul(
				(mflags & FLG_X_FLIP) ? FfInt(-4L) : FfInt(4L), VAL(actor, BOWSER_MISSILE_SPEED));
			for (fixed i = 0L; i < 3L; i++) {
				GameActor* napalm = create_actor(ACT_MISSILE_NAPALM, mpos);
				if (napalm != NULL) {
					FLAG_ON(napalm, mflags);
					VAL(napalm, X_SPEED) = mspd;
					VAL(napalm, MISSILE_HEIGHT)
						= VAL(actor, BOWSER_HEIGHT) - FfInt(28L + (i * 36L));
				}
			}

			VAL(actor, BOWSER_K) = 0L;
		}

		play_actor_sound(actor, "bowser_fire");
	}

	if (ALL_FLAG(actor, FLG_BOWSER_ACTIVE | FLG_BOWSER_GUN) && VAL(actor, BOWSER_GUN)++ > 100L) {
		GameActor* bullet = create_actor(ACT_BULLET_BILL,
			POS_ADD(actor, ANY_FLAG(actor, FLG_X_FLIP) ? FfInt(-39L) : FfInt(39L), FfInt(-23L)));
		if (bullet != NULL) {
			bullet->depth = FxHalf;
			VAL(bullet, X_SPEED) = ANY_FLAG(actor, FLG_X_FLIP) ? -212992L : 212992L;
			FLAG_ON(bullet, actor->flags & FLG_X_FLIP);

			create_actor(ACT_EXPLODE, bullet->pos);
			play_actor_sound(bullet, "bang");
		}

		VAL(actor, BOWSER_GUN) = 0L;
	}

	VAL_TICK(actor, BOWSER_HURT);

	GameActor* slipper = get_actor(VAL(actor, BOWSER_SLIPPER));
	if (slipper == NULL)
		VAL(actor, BOWSER_SLIPPER) = NULLACT;
	else if (VAL(actor, BOWSER_SLIP) != 0L) {
		VAL(actor, BOWSER_SLIP) -= (VAL(actor, BOWSER_SLIP) > 0L) ? 1L : -1L;
		if (VAL(actor, BOWSER_SLIP) == 0L)
			VAL(actor, BOWSER_SLIPPER) = NULLACT;
		else {
			const fixed slip = FfInt(VAL(actor, BOWSER_SLIP));
			if (!touching_solid(HITBOX_ADD(slipper, slip, FxZero), SOL_SOLID))
				move_actor(slipper, POS_ADD(slipper, slip, FxZero));
		}
	}

	VAL_TICK(actor, BOWSER_GROUND);
	VAL(actor, Y_SPEED) += 8738L;
	displace_actor(actor, FfInt(10L), false);
	if (VAL(actor, Y_TOUCH) > 0L)
		VAL(actor, BOWSER_GROUND) = 2L;
}

static void draw(const GameActor* actor) {
	const char* tex = NULL;
	const uint32_t frame = VAL(actor, BOWSER_FRAME) / 100L;
	if (!ANY_FLAG(actor, FLG_BOWSER_ANIMS))
		switch (frame % 4L) {
		default:
		case 2L:
			tex = "enemies/bowser0";
			break;
		case 1L:
			tex = "enemies/bowser1";
			break;
		case 3L:
			tex = "enemies/bowser2";
			break;
		}
	else if (ANY_FLAG(actor, FLG_BOWSER_ANIM_JUMP))
		switch (frame) {
		default:
			tex = "enemies/bowser1";
			break;
		case 1L:
			tex = "enemies/bowser_jump0";
			break;
		case 2L:
			tex = "enemies/bowser_jump1";
			break;
		}
	else if (ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE_START))
		switch (frame) {
		case 0L:
			tex = "enemies/bowser1";
			break;
		case 1L:
			tex = "enemies/bowser_fire0";
			break;
		default:
			tex = "enemies/bowser_fire1";
			break;
		}
	else if (ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE_START2))
		tex = fmt("enemies/bowser_charge%u", (frame >= 8L) ? (5L + ((frame - 5L) % 3L)) : frame);
	else if (ANY_FLAG(actor, FLG_BOWSER_ANIM_FIRE))
		tex = "enemies/bowser_fire0";

	GLubyte a = 255L;
	if (VAL(actor, BOWSER_HURT) > 0L) {
		const float time = (float)game_state.time * 0.05f;
		const float flash = SDL_fmodf(time, 1.f);
		a = (GLubyte)(((SDL_fmodf(time, 2.f) >= 1.f) ? flash : (1.f - flash)) * 255.f);

		const float hurt = VAL(actor, BOWSER_HURT), hurt_time = VAL(actor, BOWSER_HURT_TIME);
		if (hurt > (hurt_time - 6.f))
			a = (GLubyte)glm_lerp(255.f, (float)a, (hurt_time - hurt) / 6.f);
		else if (hurt < 10.f)
			a = (GLubyte)glm_lerp(255.f, (float)a, hurt / 10.f);
	}
	draw_actor(actor, tex, 0.f, B_ALPHA(a));

	if (ANY_FLAG(actor, FLG_BOWSER_GUN)) {
		const InterpActor* iactor = get_interp(actor);
		batch_pos(B_XYZ(FtInt(iactor->pos.x) + (ANY_FLAG(actor, FLG_X_FLIP) ? -23L : 23L),
			FtInt(iactor->pos.y) - 23L, FtFloat(actor->depth)));
		batch_flip(B_NO_FLIP), batch_sprite("enemies/bowser_gun");
	}
}

static void draw_hud(const GameActor* actor) {
	if (!ANY_FLAG(actor, FLG_BOWSER_ACTIVE))
		return;

	if (video_state.boss < 64.f) {
		video_state.boss += dt() * 4.f;
		if (video_state.boss > 64.f)
			video_state.boss = 64.f;
	}
	if (video_state.boss < -47.f)
		return;

	batch_reset();
	GLfloat pos[3] = {SCREEN_WIDTH - 16.f, video_state.boss, -10000.f};
	batch_pos(pos), batch_sprite("ui/bowser0");

	pos[0] -= 64.f;
	for (ActorValue i = 0; i < VAL(actor, BOWSER_HEALTH); i++) {
		batch_pos(pos), batch_sprite("ui/bowser1");
		pos[0] -= 9.f;
	}
}

static void hit_bowser(GameActor* actor, GameActor* from) {
	if (actor == NULL || actor->type != ACT_BOWSER || VAL(actor, BOWSER_HURT) > 0L)
		return;

	play_actor_sound(actor, "bowser_hurt");
	if (--VAL(actor, BOWSER_HEALTH) <= 0L) {
		GameActor* dead = create_actor(ACT_BOWSER_DEAD, actor->pos);
		if (dead != NULL) {
			FLAG_ON(dead, actor->flags & FLG_X_FLIP);
			align_interp(dead, actor);

			GamePlayer* player = get_owner(from);
			game_state.sequence.type = SEQ_BOWSER_END;
			game_state.sequence.activator = (PlayerID)((player == NULL) ? NULLPLAY : player->id);

			if (numplayers() > 1L && player != NULL)
				hud_message(fmt("%s has defeated Bowser!", get_player_name(player->id)));
			stop_state_track(TS_MAIN);

			FLAG_ON(actor, FLG_DESTROY);
			return;
		}
	}

	VAL(actor, BOWSER_HITS) = 0L;
	VAL(actor, BOWSER_HURT) = VAL(actor, BOWSER_HURT_TIME);
}

static Bool slap_bowser(GameActor* actor, GameActor* from) {
	if (actor == NULL || actor->type != ACT_BOWSER || VAL(actor, BOWSER_HURT) > 0L)
		return false;

	play_actor_sound(actor, "kick");
	if (++VAL(actor, BOWSER_HITS) > 4L)
		hit_bowser(actor, from);
	return true;
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;

	case ACT_PLAYER: {
		if (VAL(actor, BOWSER_HURT) >= (VAL(actor, BOWSER_HURT_TIME) - 5L)
			&& from->pos.y < (actor->pos.y - FfInt(45L)))
			break;

		if (VAL(actor, BOWSER_HURT) > 0L || from->pos.y >= (actor->pos.y - FfInt(45L))
			|| (VAL(from, Y_SPEED) < FxZero && !ANY_FLAG(from, FLG_PLAYER_STOMP)))
		{
			hit_player(from);
			break;
		}

		GamePlayer* player = get_owner(from);
		if (player != NULL)
			player->score += 100L;
		VAL(from, Y_SPEED) = FfInt(-8L);
		FLAG_ON(from, FLG_PLAYER_STOMP);

		VAL(actor, BOWSER_SLIPPER) = from->id;
		VAL(actor, BOWSER_SLIP) = ANY_FLAG(from, FLG_X_FLIP) ? -8L : 8L;
		hit_bowser(actor, from);
		break;
	}

	case ACT_MISSILE_FIREBALL: {
		GamePlayer* player = get_owner(from);
		if (player != NULL && slap_bowser(actor, from))
			FLAG_ON(from, FLG_DESTROY);
		break;
	}

	case ACT_MISSILE_BEETROOT: {
		GamePlayer* player = get_owner(from);
		if (player != NULL && slap_bowser(actor, from))
			FLAG_ON(from, FLG_MISSILE_HIT);
		break;
	}

	case ACT_MISSILE_HAMMER: {
		GamePlayer* player = get_owner(from);
		if (player != NULL)
			hit_bowser(actor, from);
		break;
	}
	}
}

const GameActorTable TAB_BOWSER = {
	.load = load,
	.load_special = load_special,
	.create = create,
	.tick = tick,
	.draw = draw,
	.draw_hud = draw_hud,
	.collide = collide,
};

// ============
// BOWSER DYING
// ============

static void load_corpse() {
	load_texture_num("enemies/bowser_dead%u", 2L, false);

	load_sound("bowser_dead", false);
	load_sound("bowser_fall", false);
	load_sound("bowser_lava", false);

	load_actor(ACT_LAVA_BUBBLE);
}

static void create_corpse(GameActor* actor) {
	actor->box.start.x = FfInt(-30L);
	actor->box.start.y = FfInt(-70L);
	actor->box.end.x = FfInt(30L);
	actor->depth = FxOne;

	play_actor_sound(actor, "bowser_dead");
}

static void tick_corpse(GameActor* actor) {
	if (actor->pos.y > (game_state.bounds.end.y + FfInt(100L)) && game_state.sequence.type == SEQ_BOWSER_END) {
		game_state.sequence.type = SEQ_AMBUSH_END;
		game_state.sequence.time = 999L;
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	const fixed state = ++VAL(actor, KUPPA_BE_HAPPY);
	if (state > 100L) {
		if (state == 101L)
			play_actor_sound(actor, "bowser_fall");
		VAL(actor, Y_SPEED) += 8738L;
	}

	if (ANY_FLAG(actor, FLG_KUPPA_LAVA_LOVE)) {
		if (VAL(actor, Y_SPEED) > FxOne)
			VAL(actor, Y_SPEED) = Fmax(VAL(actor, Y_SPEED) - FxOne, FxOne);
		if (VAL(actor, KUPPA_PFRBRBR) < 50L) {
			GameActor* bubble
				= create_actor(ACT_LAVA_BUBBLE, POS_ADD(actor, FfInt(-23L + rng(47L)), FfInt(-16L)));
			if (bubble != NULL) {
				VAL(bubble, X_SPEED) = FfInt(-2L + rng(5L));
				VAL(bubble, Y_SPEED) = FfInt(-2L - rng(4L));
			}

			++VAL(actor, KUPPA_PFRBRBR);
		}
	}

	move_actor(actor, POS_SPEED(actor));
	collide_actor(actor);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, fmt("enemies/bowser_dead%u", (game_state.time / 2L) % 2L), 0.f, B_WHITE);
}

static void draw_corpse_hud(const GameActor* actor) {
	if (video_state.boss > -48.f) {
		video_state.boss -= dt() * 4.f;
		if (video_state.boss < -48.f)
			video_state.boss = -48.f;
	}
	if (video_state.boss < -47.f)
		return;

	batch_reset();
	batch_pos(B_XYZ(SCREEN_WIDTH - 16.f, video_state.boss, -10000.f)), batch_sprite("ui/bowser0");
}

const GameActorTable TAB_BOWSER_DEAD = {
	.load = load_corpse,
	.create = create_corpse,
	.tick = tick_corpse,
	.draw = draw_corpse,
	.draw_hud = draw_corpse_hud,
};

// =============
// LAVA SPLASHER
// =============

static void create_laver(GameActor* actor) {
	actor->box.start.x = FfInt(-15L);
	actor->box.start.y = FfInt(-17L);
	actor->box.end.x = FfInt(17L);
	actor->box.end.y = FfInt(15L);

	VAL(actor, KUPPA_LAVER) = FfInt(16L);
}

static void tick_laver(GameActor* actor) {
	if (VAL(actor, KUPPA_LAVER) <= FxZero) {
		FLAG_ON(actor, FLG_DESTROY);
		return;
	}

	move_actor(actor, POS_ADD(actor, ANY_FLAG(actor, FLG_X_FLIP) ? FfInt(-4L) : FfInt(4L), FxZero));
	collide_actor(actor);
}

const GameActorTable TAB_BOWSER_LAVA = {.create = create_laver, .tick = tick_laver};
