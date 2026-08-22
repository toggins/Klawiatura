#include "K_audio.h"
#include "K_locale.h"
#include "K_video.h"

#include "actors/K_player.h"
#include "actors/K_powerups.h"
#include "actors/K_warp.h"

static void load() {
    load_sound("warp", AKL_NEVER);
}

static void load_special(const GameActor* actor) {
    if (ANY_FLAG(actor, FLG_WARP_CALAMITY | FLG_WARP_SECRET | FLG_WARP_WORLD | FLG_WARP_LEVEL)) {
        const char* secret = get_game_secret(VAL(actor, WARP_SECRET));
        if (secret != NULL && secret[0] != '$')
            load_sprite(LFMT(secret), AKL_NEVER);
    }
    if (ANY_FLAG(actor, FLG_WARP_CALAMITY))
        load_sound("vo/clone/a_dead", AKL_NEVER);
    if (ANY_FLAG(actor, FLG_WARP_DEVASTATOR))
        load_sound("vo/thwomp", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-8);
    actor->box.start.y = Int2Fx(-13);
    actor->box.end.x = Int2Fx(7);
    actor->box.end.y = Fx1;

    VAL(actor, WARP_X) = actor->pos.x;
    VAL(actor, WARP_Y) = actor->pos.y;
    VAL(actor, WARP_ANGLE) = 3;
    VAL(actor, WARP_OUT_ANGLE) = 1;

    FLAG_OFF(actor, FLG_VISIBLE);
}

static void tick(GameActor* actor) {
    if (VAL(actor, WARP_STATE) > 0) {
        ++VAL(actor, WARP_STATE);
        if (VAL(actor, WARP_STATE) > 200 && !ANY_FLAG(actor, FLG_WARP_WORLD | FLG_WARP_LEVEL))
            FLAG_ON(actor, FLG_DESTROY);
    }
}

static void draw_hud(const GameActor* actor) {
    if (VAL(actor, WARP_STATE) <= 0)
        return;

    if (ANY_FLAG(actor, FLG_WARP_SECRET)) {
        const char* secret = get_game_secret(VAL(actor, WARP_SECRET));
        if (secret != NULL) {
            batch_reset();
            batch_pos(B_F3_XY(320.f, -37.f + (int)(((float)SDL_min(VAL(actor, WARP_STATE), 41) / 41.f) * 257.f)));
            if (secret[0] == '$')
                batch_string("main", 24.f, LFMT(secret + 1));
            else
                batch_sprite(LFMT(secret));
        }
    } else if (ANY_FLAG(actor, FLG_WARP_CALAMITY)) {
        const char* secret = get_game_secret(VAL(actor, WARP_SECRET));
        if (secret != NULL) {
            batch_reset();
            batch_pos(B_F3_XY(320.f, 220.f));
            if (VAL(actor, WARP_STATE) >= 101)
                batch_color(B_U4_ALPHA((1.f - ((float)(VAL(actor, WARP_STATE) - 101) / 101.f)) * 255.f));
            if (secret[0] == '$')
                batch_string("main", 24.f, LFMT(secret + 1));
            else
                batch_sprite(LFMT(secret));
        }
    }
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || get_actor(VAL(from, PLAYER_WARP)) != NULL || ANY_FLAG(from, FLG_PLAYER_WARP_OUT)
        || VAL(actor, WARP_STATE) > 0)
    {
        return;
    }

    GamePlayer* player = get_player(from->player);
    if (player == NULL)
        return;

    switch (VAL(actor, WARP_ANGLE)) {
    default: {
        if (!TOUCHING(from, TOUCH_RIGHT) || !ANY_INPUT(player, GI_RIGHT) || from->pos.y > (actor->pos.y + Fx1))
            return;

        move_actor(from, Vadd(actor->pos, (FVec2){actor->box.end.x - from->box.end.x, Fx0}));
        break;
    }

    case 1: {
        if (!TOUCHING(from, TOUCH_TOP) || !ANY_INPUT(player, GI_UP))
            return;

        move_actor(from, Vadd(actor->pos, (FVec2){Fx0, actor->box.start.y - from->box.start.y}));
        break;
    }

    case 2: {
        if (!TOUCHING(from, TOUCH_LEFT) || !ANY_INPUT(player, GI_LEFT) || from->pos.y > (actor->pos.y + Fx1))
            return;

        move_actor(from, Vadd(actor->pos, (FVec2){actor->box.start.x - from->box.start.x, Fx0}));
        break;
    }

    case 3: {
        if (!TOUCHING(from, TOUCH_BOTTOM) || !ANY_INPUT(player, GI_DOWN))
            return;

        move_actor(from, Vadd(actor->pos, (FVec2){Fx0, actor->box.end.y - from->box.end.y}));
        break;
    }
    }

    VAL(from, PLAYER_WARP) = actor->id;
    VAL(from, PLAYER_WARP_STATE) = 0;

    if (ANY_FLAG(actor, FLG_WARP_CALAMITY)) {
        GameActor* item = NULL;
        FOR_EACH_ACTOR (item) {
            switch (item->type) {
            default:
                break;

            case ACT_SUPER_MUSHROOM:
            case ACT_1UP_MUSHROOM:
            case ACT_POISON_MUSHROOM:
            case ACT_FIRE_FLOWER:
            case ACT_BEETROOT:
            case ACT_GREEN_LUI:
            case ACT_STARMAN: {
                if (ANY_FLAG(item, FLG_POWERUP_CALAMITY)) {
                    item->sprout = 32;
                    FLAG_OFF(item, FLG_POWERUP_CALAMITY);
                }

                break;
            }
            }
        }

        play_state_sound("vo/clone/a_dead", 0, NULL);
    } else if (ANY_FLAG(actor, FLG_WARP_DEVASTATOR)) {
        play_state_sound("vo/thwomp", 0, NULL);
        stop_state_track(from->player);
    } else {
        play_state_sound("warp", PLAY_POS, A_ACTOR(from));
    }

    if (ANY_FLAG(actor, FLG_WARP_CALAMITY | FLG_WARP_SECRET | FLG_WARP_WORLD | FLG_WARP_LEVEL | FLG_WARP_DEVASTATOR))
        ++VAL(actor, WARP_STATE);

    if (ANY_FLAG(actor, FLG_WARP_WORLD | FLG_WARP_LEVEL)) {
        for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
            GamePlayer* oplayer = get_player(i);
            if (oplayer == NULL || oplayer->id == player->id)
                continue;

            GameActor* opawn = get_actor(oplayer->actor);
            if (opawn == NULL || opawn->id == from->id)
                continue;

            FLAG_ON(opawn, FLG_DESTROY);
        }

        set_sequence(GS_WARP, player, ANY_FLAG(actor, FLG_WARP_SECRET | FLG_WARP_DEVASTATOR));
        set_view_player(player);
    }
}

const ActorTable TAB_WARP = {
    .load = load,
    .load_special = load_special,
    .create = create,
    .tick = tick,
    .draw_hud = draw_hud,
    .collide = collide,
};
