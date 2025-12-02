#include "actors/K_player.h"
#include "actors/K_powerups.h"
#include "actors/K_warp.h"

static void load_special(const GameActor* actor) {
	if (ANY_FLAG(actor, FLG_WARP_CALAMITY))
		load_sound("clone_dead2", false);

	if (ANY_FLAG(actor, FLG_WARP_SECRET))
		load_track("warp", false);
}

static void create(GameActor* actor) {
	actor->box.start.x = FfInt(-8L);
	actor->box.start.y = FfInt(-16L);
	actor->box.end.x = FfInt(8L);
}

static void collide(GameActor* actor, GameActor* from) {
	if (from->type != ACT_PLAYER || get_actor(VAL(from, PLAYER_WARP)) != NULL)
		return;

	GamePlayer* player = get_owner(from);
	if (player == NULL)
		return;

	Bool warped = false;
	switch (VAL(actor, WARP_ANGLE)) {
	case 0L: {
		if (VAL(from, X_TOUCH) > 0L && ANY_INPUT(player, GI_RIGHT)) {
			move_actor(from, POS_ADD(actor, actor->box.end.x - from->box.end.x, FxZero));
			warped = true;
		}
		break;
	}

	case 1L: {
		if (VAL(from, Y_TOUCH) < 0L && ANY_INPUT(player, GI_UP)) {
			move_actor(from, POS_ADD(actor, FxZero, actor->box.start.y - from->box.start.y));
			warped = true;
		}
		break;
	}

	case 2L: {
		if (VAL(from, X_TOUCH) < 0L && ANY_INPUT(player, GI_LEFT)) {
			move_actor(from, POS_ADD(actor, actor->box.start.x - from->box.start.x, FxZero));
			warped = true;
		}
		break;
	}

	default: {
		if (VAL(from, PLAYER_GROUND) > 0L && ANY_INPUT(player, GI_DOWN)) {
			move_actor(from, actor->pos);
			warped = true;
		}
		break;
	}
	}

	if (warped) {
		from->depth = FfInt(21L);
		VAL(from, X_SPEED) = VAL(from, Y_SPEED) = FxZero;
		VAL(from, PLAYER_WARP) = actor->id;
		VAL(from, PLAYER_WARP_STATE) = 0L;
		FLAG_OFF(from, FLG_PLAYER_WARP_OUT);
		play_actor_sound(actor, "warp");

		if (VAL(actor, WARP_STRING) != 0L) {
			int8_t lvl[ACTOR_STRING_MAX + 1] = "";
			decode_actor_string(actor, VAL_WARP_STRING, lvl);
			SDL_strlcpy((char*)game_state.next, (char*)lvl, sizeof(game_state.next));

			GamePlayer* warper = get_owner(from);
			for (PlayerID i = 0; i < numplayers(); i++) {
				GamePlayer* player = get_player(i);
				if (player == NULL || warper == player)
					continue;

				GameActor* pawn = get_actor(player->actor);
				if (pawn != NULL)
					FLAG_ON(pawn, FLG_DESTROY);
			}

			// !!! CLIENT-SIDE !!!
			set_view_player(warper);
			// !!! CLIENT-SIDE !!!
		}

		if (ANY_FLAG(actor, FLG_WARP_CALAMITY)) {
			for (GameActor* act = get_actor(game_state.live_actors); act != NULL;
				act = get_actor(act->previous))
			{
				if ((act->type != ACT_MUSHROOM && act->type != ACT_MUSHROOM_1UP
					    && act->type != ACT_MUSHROOM_POISON)
					|| !ANY_FLAG(act, FLG_POWERUP_CALAMITY))
					continue;
				VAL(act, X_SPEED) = FfInt(2L);
				VAL(act, SPROUT) = FfInt(32L);
				FLAG_OFF(act, FLG_POWERUP_CALAMITY);
			}
			FLAG_OFF(actor, FLG_WARP_CALAMITY);

			hud_message("MUSHROOM CALAMITY ACTIVATED!!!");
			play_state_sound("clone_dead2");
		}
	}
}

const GameActorTable TAB_WARP = {.load_special = load_special, .create = create, .collide = collide};
