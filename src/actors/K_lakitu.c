#include "K_string.h"

#include "actors/K_lakitu.h"
#include "actors/K_player.h"
#include "actors/K_spiny.h"

static void load() {
	load_texture_num("enemies/lakitu%u", 10L, FALSE);
	load_texture_num("enemies/lakitu_cloud%u", 7L, FALSE);
	load_texture_num("enemies/lakitu_throw%u", 11L, FALSE);

	load_sound("lakitu0", FALSE);
	load_sound("lakitu1", FALSE);
	load_sound("lakitu2", FALSE);
	load_sound("stomp", FALSE);
	load_sound("kick", FALSE);

	load_actor(ACT_SPINY_EGG);
	load_actor(ACT_POINTS);
}

static void create(GameActor* actor) {
	actor->box.start.x = FxFrom(-15L);
	actor->box.start.y = FxFrom(-48L);
	actor->box.end.x = FxFrom(15L);
}

static void tick(GameActor* actor) {
	if (ANY_FLAG(actor, FLG_LAKITU_BLINK)) {
		if (++VAL(actor, LAKITU_FRAME) >= 16L) {
			FLAG_OFF(actor, FLG_LAKITU_BLINK);
			VAL(actor, LAKITU_FRAME) = 0L;
		}
	} else if (ANY_FLAG(actor, FLG_LAKITU_CLOUD)) {
		if (++VAL(actor, LAKITU_FRAME) >= 12L) {
			FLAG_OFF(actor, FLG_LAKITU_CLOUD);
			VAL(actor, LAKITU_FRAME) = 0L;
		}
	} else if (ANY_FLAG(actor, FLG_LAKITU_IN)) {
		if (VAL(actor, LAKITU_FRAME) < 10L)
			++VAL(actor, LAKITU_FRAME);
	} else if (ANY_FLAG(actor, FLG_LAKITU_OUT)) {
		if (++VAL(actor, LAKITU_FRAME) >= 11L) {
			FLAG_OFF(actor, FLG_LAKITU_OUT);
			VAL(actor, LAKITU_FRAME) = 0L;
		}
	}

	GameActor* nearest = nearest_pawn(actor->pos);
	if (nearest != NULL) {
		if (nearest->pos.x < (game_state.size.x - FxFrom(1000L))) {
			if (actor->pos.x > (nearest->pos.x + FxFrom(50L))
				&& VAL(actor, X_SPEED)
					   > ((game_state.flags & GF_HARDCORE) ? FxFrom(-20L) : FxFrom(-9L)))
				VAL(actor, X_SPEED) -= 13107L;
			else if (actor->pos.x < (nearest->pos.x - FxFrom(50L))
				 && VAL(actor, X_SPEED) < ((game_state.flags & GF_HARDCORE) ? FxFrom(20L) : FxFrom(9L)))
				VAL(actor, X_SPEED) += 13107L;

			if (actor->pos.x < (nearest->pos.x + FxFrom(100L))
				&& actor->pos.x > (nearest->pos.x - FxFrom(100L)))
			{
				if (actor->pos.x > nearest->pos.x && VAL(actor, X_SPEED) < FxFrom(-2L))
					VAL(actor, X_SPEED) += FxOne;
				else if (actor->pos.x < nearest->pos.x && VAL(actor, X_SPEED) > FxFrom(4L))
					VAL(actor, X_SPEED) -= FxOne;
			}

			if (in_any_view(actor, FxFrom(-32L), FALSE))
				VAL(actor, LAKITU_THROW) += (game_state.flags & GF_HARDCORE) ? 5L : 1L;
		} else if (VAL(actor, X_SPEED) > FxFrom(-2L))
			VAL(actor, X_SPEED) -= FxOne;
	}

	if ((game_state.time % 5L) == 0L && !ANY_FLAG(actor, FLG_LAKITU_ANIMS))
		switch (rng(20L)) {
		default:
			break;

		case 10L: {
			FLAG_OFF(actor, FLG_LAKITU_ANIMS);
			FLAG_ON(actor, FLG_LAKITU_BLINK);
			VAL(actor, LAKITU_FRAME) = 0L;
			break;
		}

		case 15L: {
			FLAG_OFF(actor, FLG_LAKITU_ANIMS);
			FLAG_ON(actor, FLG_LAKITU_CLOUD);
			VAL(actor, LAKITU_FRAME) = 0L;
			break;
		}
		}

	if (VAL(actor, LAKITU_THROW) > (200L + VAL(actor, LAKITU_STALL))) {
		VAL(actor, LAKITU_THROW) = 0L;
		VAL(actor, LAKITU_STALL) = rng(100L);
		const Sint32 wee = rng(3L);
		play_state_sound(fmt("lakitu%i", wee), PLAY_POS, 0L, A_ACTOR(actor));

		GameActor* egg = create_actor(ACT_SPINY_EGG, POS_ADD(actor, FxZero, FxFrom(-39L)));
		if (egg != NULL) {
			if (ANY_FLAG(actor, FLG_LAKITU_GREEN)) {
				VAL(egg, X_SPEED)
					= (nearest != NULL
						  && (nearest->pos.x + Fdouble(VAL(nearest, X_SPEED))) < egg->pos.x)
				                  ? FxFrom(-4L)
				                  : FxFrom(4L);
				VAL(egg, Y_SPEED) = FxFrom(-5L);
				FLAG_ON(egg, FLG_SPINY_GREEN);
			} else
				VAL(egg, Y_SPEED) = FxFrom(-3L);
		}
	}

	if (VAL(actor, LAKITU_THROW) > (VAL(actor, LAKITU_STALL) + 160L) && !ANY_FLAG(actor, FLG_LAKITU_IN)) {
		FLAG_OFF(actor, FLG_LAKITU_ANIMS);
		FLAG_ON(actor, FLG_LAKITU_IN);
		VAL(actor, LAKITU_FRAME) = 0L;
	}

	if (VAL(actor, LAKITU_THROW) > (VAL(actor, LAKITU_STALL) + 190L) && !ANY_FLAG(actor, FLG_LAKITU_OUT)) {
		FLAG_OFF(actor, FLG_LAKITU_ANIMS);
		FLAG_ON(actor, FLG_LAKITU_OUT);
		VAL(actor, LAKITU_FRAME) = 0L;
	}

	move_actor(actor, POS_SPEED(actor));
}

static void draw(const GameActor* actor) {
	const char* tex = "enemies/lakitu0";
	if (ANY_FLAG(actor, FLG_LAKITU_BLINK))
		switch (VAL(actor, LAKITU_FRAME)) {
		default:
			tex = "enemies/lakitu1";
			break;
		case 1L:
		case 15L:
			tex = "enemies/lakitu2";
			break;
		case 2L:
		case 14L:
			tex = "enemies/lakitu3";
			break;
		case 3L:
		case 13L:
			tex = "enemies/lakitu4";
			break;
		case 4L:
		case 12L:
			tex = "enemies/lakitu5";
			break;
		case 5L:
		case 11L:
			tex = "enemies/lakitu6";
			break;
		case 6L:
		case 10L:
			tex = "enemies/lakitu7";
			break;
		case 7L:
		case 9L:
			tex = "enemies/lakitu8";
			break;
		case 8L:
			tex = "enemies/lakitu9";
			break;
		}
	else if (ANY_FLAG(actor, FLG_LAKITU_CLOUD))
		switch (VAL(actor, LAKITU_FRAME)) {
		default:
			tex = "enemies/lakitu_cloud0";
			break;
		case 1L:
		case 11L:
			tex = "enemies/lakitu_cloud1";
			break;
		case 2L:
		case 10L:
			tex = "enemies/lakitu_cloud2";
			break;
		case 3L:
		case 9L:
			tex = "enemies/lakitu_cloud3";
			break;
		case 4L:
		case 8L:
			tex = "enemies/lakitu_cloud4";
			break;
		case 5L:
		case 7L:
			tex = "enemies/lakitu_cloud5";
			break;
		case 6L:
			tex = "enemies/lakitu_cloud6";
			break;
		}
	else if (ANY_FLAG(actor, FLG_LAKITU_IN))
		tex = fmt("enemies/lakitu_throw%u", VAL(actor, LAKITU_FRAME));
	else if (ANY_FLAG(actor, FLG_LAKITU_OUT))
		tex = fmt("enemies/lakitu_throw%u", 10L - VAL(actor, LAKITU_FRAME));
	draw_actor(actor, tex, 0.f, B_WHITE);
}

static void draw_corpse(const GameActor* actor) {
	draw_actor(actor, "enemies/lakitu0", 0.f, B_WHITE);
}

static void collide(GameActor* actor, GameActor* from) {
	switch (from->type) {
	default:
		break;
	case ACT_PLAYER:
		if (VAL(actor, PLAYER_STARMAN) > 0L)
			player_starman(from, actor);
		else if (check_stomp(actor, from, FxFrom(-16L), 100L))
			kill_enemy(actor, from, FALSE);
		break;

	case ACT_KOOPA_SHELL:
	case ACT_BUZZY_SHELL:
		hit_shell(actor, from);
		break;
	case ACT_BLOCK_BUMP:
		hit_bump(actor, from, 100L);
		break;
	case ACT_MISSILE_FIREBALL:
		hit_fireball(actor, from, 100L);
		break;
	case ACT_MISSILE_BEETROOT:
		hit_beetroot(actor, from, 100L);
		break;
	case ACT_MISSILE_HAMMER:
		hit_hammer(actor, from, 100L);
		break;
	}
}

const GameActorTable TAB_LAKITU = {
	.load = load,
	.create = create,
	.tick = tick,
	.draw = draw,
	.draw_dead = draw_corpse,
	.collide = collide,
};
