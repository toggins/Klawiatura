#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_bowser.h"
#include "actors/K_lava.h"
#include "actors/K_player.h"
#include "actors/K_podoboo.h"
#include "actors/K_warp.h"

/* ====
   LAVA
   ==== */

static void load() {
    load_sprite_num("enemies/lava/%u", 7, AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.y = Int2Fx(-17);
    actor->box.end.x = Int2Fx(32);
    actor->box.end.y = Int2Fx(31);

    actor->depth = Int2Fx(20);
}

static void tick(GameActor* actor) {
    if (!ANY_FLAG(actor, FLG_LAVA_WAVE))
        return;

    VAL_TICK(actor, LAVA_OVERLAP);

    move_actor(
        actor, (FVec2){actor->pos.x, VAL(actor, LAVA_Y) + Fmul(VAL(actor, LAVA_WAVE), Fcos(VAL(actor, LAVA_ANGLE)))});
    VAL(actor, LAVA_ANGLE) = Fmod(VAL(actor, LAVA_ANGLE) + 5719, Fx2Pi);

    if (VAL(actor, LAVA_WAVE) > Fx0) {
        VAL(actor, LAVA_WAVE) -= 6554;
        if (VAL(actor, LAVA_WAVE) < Fx0)
            VAL(actor, LAVA_WAVE) = Fx0;
    }
    if (VAL(actor, LAVA_WAVE) <= Fx0)
        VAL(actor, LAVA_ANGLE) = Fx0;
}

static void draw(const GameActor* actor) {
    batch_reset();
    const char* sprite = fmt("enemies/lava/%i", ((gamestate()->time * 11) / 100) % 7);
    for (Sint32 i = 0, n = Fx2Int(actor->box.end.x - actor->box.start.x); i < n; i += 32) {
        batch_offset(B_F3_XY(-i, 0.f));
        draw_actor(actor, sprite, FALSE);
    }
}

static void collide(GameActor* actor, GameActor* from) {
    switch (from->type) {
    default:
        break;

    case ACT_PLAYER: {
        kill_player(from);
        break;
    }

    case ACT_BOWSER_DEAD: {
        const Fixed ly = actor->pos.y + Int2Fx(10);
        if ((from->pos.y + from->box.end.y) <= ly || VAL(from, BOWSER_DEAD_LAVA) > 0)
            break;

        create_actor(ACT_LAVA_SPLASH, Vadd(from->pos, (FVec2){Fx0, Int2Fx(-9)}));

        GameActor* waver = create_actor(ACT_LAVA_WAVER, from->pos);
        if (waver != NULL)
            waver->vel.x = 122880;
        waver = create_actor(ACT_LAVA_WAVER, from->pos);
        if (waver != NULL)
            waver->vel.x = -122880;

        from->depth = actor->depth + 1;
        ++VAL(from, BOWSER_DEAD_LAVA);
        VAL(from, BOWSER_DEAD_Y) = ly;

        play_state_sound("bowser/lava", PLAY_POS, A_ACTOR(from));
        break;
    }

    case ACT_LAVA_WAVER: {
        if (VAL(actor, LAVA_OVERLAP) > 0) {
            VAL(actor, LAVA_OVERLAP) = 2;
            break;
        }

        if ((from->vel.x > Fx0 && (from->pos.x + from->box.start.x) > (actor->pos.x + actor->box.start.x))
            || (from->vel.x < Fx0 && (from->pos.x + from->box.end.x) < (actor->pos.x + actor->box.end.x)))
        {
            break;
        }

        actor->depth -= 1;
        VAL(actor, LAVA_Y) = actor->pos.y;
        VAL(actor, LAVA_WAVE) = VAL(from, LAVA_WAVE);
        VAL(actor, LAVA_OVERLAP) = 2;
        FLAG_ON(actor, FLG_LAVA_WAVE);

        VAL(from, LAVA_WAVE) -= Int2Fx(4);
        break;
    }

    case ACT_PODOBOO: {
        if (VAL(from, PODOBOO_OVERLAP) > 0) {
            VAL(from, PODOBOO_OVERLAP) = 2;
            break;
        }

        VAL(from, PODOBOO_OVERLAP) = 2;
        if (from->vel.y >= Fmul(VAL(from, PODOBOO_JUMP), 43691))
            FLAG_OFF(from, FLG_VISIBLE);

        break;
    }
    }
}

const ActorTable TAB_LAVA = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};

/* ==========
   LAVA WAVER
   ========== */

static void create_waver(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-17);
    actor->box.end.x = Int2Fx(17);
    actor->box.end.y = Int2Fx(15);

    VAL(actor, LAVA_WAVE) = Int2Fx(16);
    FLAG_OFF(actor, FLG_VISIBLE);
}

static void tick_waver(GameActor* actor) {
    move_actor(actor, Vadd(actor->pos, actor->vel));

    if (VAL(actor, LAVA_WAVE) <= Fx0) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    collide_actor(actor);
}

const ActorTable TAB_LAVA_WAVER = {
    .create = create_waver,
    .tick = tick_waver,
};

/* ===========
   RISING LAVA
   =========== */

static void load_rising() {
    load_sprite_num("enemies/lava/%u", 7, AKL_NEVER);
    load_sprite("enemies/lava/bottom", AKL_NEVER);
    load_sprite_num("ui/lava/%u", 7, AKL_NEVER);
}

static void create_rising(GameActor* actor) {
    actor->depth = Int2Fx(-10);

    VAL(actor, RISING_LIMIT) = Int2Fx(2000);
    VAL(actor, RISING_TIME) = 900;

    GameState* game_state = gamestate();
    GameActor* hazard = get_actor(game_state->hazard);
    if (hazard != NULL)
        FLAG_ON(hazard, FLG_DESTROY);
    game_state->hazard = actor->id;
}

static void cleanup_rising(GameActor* actor) {
    GameState* game_state = gamestate();
    if (game_state->hazard == actor->id)
        game_state->hazard = NULL_ACTOR;
}

static void tick_rising(GameActor* actor) {
    // !!! CLIENT-SIDE !!!
    const GamePlayer* view_player = get_player(viewplayer());
    if (view_player != NULL) {
        Bool show_hud = FALSE;

        const Fixed limit = VAL(actor, RISING_LIMIT) + Int2Fx(300);
        if (view_player->pos.y >= limit) {
            const GameActor* pawn = get_actor(view_player->actor);
            if (pawn != NULL && pawn->type == ACT_PLAYER) {
                const GameActor* warp = get_actor(VAL(pawn, PLAYER_WARP));
                if (warp == NULL || VAL(warp, WARP_Y) >= limit)
                    show_hud = TRUE;
            } else {
                show_hud = TRUE;
            }
        }

        VideoState* video_state = videostate();
        if (show_hud)
            video_state->hazard_fade = 1.f;
        else if (video_state->hazard_fade > 0.f)
            video_state->hazard_fade -= 0.01f;
    }
    // !!! CLIENT-SIDE !!!

    if (actor->pos.y > VAL(actor, RISING_LIMIT) && get_sequence()->type != GS_LOSE) {
        const GameState* game_state = gamestate();
        if (game_state->time > 0 && (VAL(actor, RISING_TIME) <= 1 || (game_state->time % VAL(actor, RISING_TIME)) == 0)
            && actor->vel.y > Int2Fx(-3))
        {
            actor->vel.y = Fmax(actor->vel.y - Fx1, Int2Fx(-3));
        }

        move_actor(actor, Vadd(actor->pos, actor->vel));
    }

    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++) {
        const GamePlayer* player = get_player(i);
        if (player == NULL)
            continue;

        GameActor* pawn = get_actor(player->actor);
        if (pawn != NULL && pawn->type == ACT_PLAYER && (pawn->pos.y + pawn->box.end.y) > (actor->pos.y - Int2Fx(17)))
            kill_player(pawn);
    }
}

static void draw_rising(const GameActor* actor) {
    batch_reset();

    const LevelInfo* level_info = levelinfo();
    const Sint32 w = Fx2Int(level_info->size.x), y = Fx2Int(get_interp(actor).y);
    const char* sprite = fmt("enemies/lava/%i", ((gamestate()->time * 11) / 100) % 7);

    for (Sint32 i = 0; i < w; i++) {
        batch_pos(B_F3_XY(i * 32.f, y));
        batch_sprite(sprite);
    }

    const Sint32 h = Fx2Int(level_info->size.y);
    if (y < (h - 18)) {
        batch_pos(B_F3_XY(0.f, y + 18.f));
        batch_scale(B_F2((float)w / 32.f, ((float)Fx2Int(level_info->size.y) - (float)y - 18.f) / 32.f));
        batch_sprite("enemies/lava/bottom");
    }
}

static void draw_rising_hud(const GameActor* actor) {
    const VideoState* video_state = videostate();
    if (video_state->hazard_fade <= 0.f)
        return;

    const GamePlayer* view_player = get_player(viewplayer());
    if (view_player == NULL)
        return;

    batch_reset();
    batch_pos(B_F3_XY(23.f, 70.f));
    batch_color(B_U4_ALPHA(video_state->hazard_fade * 255.f));
    batch_sprite(get_character_sprite(gamecontext()->players[view_player->id].character, POW_NONE, PF_DEAD));

    batch_pos(B_F3_XY(8.f, 70.f + SDL_truncf((Fx2Float(actor->pos.y) - Fx2Float(view_player->pos.y)) / 20.f)));
    batch_sprite(fmt("ui/lava/%i", ((gamestate()->time * 11) / 100) % 7));
}

const ActorTable TAB_RISING_LAVA = {
    .load = load_rising,
    .create = create_rising,
    .cleanup = cleanup_rising,
    .tick = tick_rising,
    .draw = draw_rising,
    .draw_hud = draw_rising_hud,
};
