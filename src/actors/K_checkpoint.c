#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_checkpoint.h"

/* ==========
   CHECKPOINT
   ========== */

static void load() {
    load_sprite_num("markers/checkpoint/%u", 3, AKL_NEVER);
    load_sound("sprout", AKL_NEVER);
    load_actor(ACT_CHECKPOINT_EFFECT);

    const GameContext* game_context = gamecontext();
    for (PlayerID i = 0; i < game_context->num_players; i++) {
        const PlayerCharacter character = game_context->players[i].character;
        load_sound(get_character_voice(character, PV_READY), AKL_NEVER);
        load_sound(get_character_voice(character, PV_CHECKPOINT1), AKL_NEVER);
        load_sound(get_character_voice(character, PV_CHECKPOINT2), AKL_NEVER);
        load_sound(get_character_voice(character, PV_CHECKPOINT3), AKL_NEVER);
    }
}

static void create(GameActor* actor) {
    actor->box.start.x = actor->box.start.y = Int2Fx(8);
    actor->box.end.x = Int2Fx(115);
    actor->box.end.y = Int2Fx(119);

    actor->depth = Int2Fx(24);

    if (gamestate()->checkpoint >= actor->id)
        VAL(actor, CHECKPOINT_ANIMATION) = 2;
}

static void cleanup(GameActor* actor) {
    GameState* game_state = gamestate();
    if (game_state->checkpoint == actor->id)
        game_state->checkpoint = NULL_ACTOR;
}

static void tick(GameActor* actor) {
    switch (VAL(actor, CHECKPOINT_ANIMATION)) {
    default:
        break;

    case 1: {
        VAL(actor, CHECKPOINT_FRAME) += FxHalf;
        if (VAL(actor, CHECKPOINT_FRAME) >= Int2Fx(20)) {
            VAL(actor, CHECKPOINT_ANIMATION) = 2;
            VAL(actor, CHECKPOINT_FRAME) = Fx0;
        }

        break;
    }

    case 2: {
        VAL(actor, CHECKPOINT_FRAME) = Fmod(VAL(actor, CHECKPOINT_FRAME) + 6554, Int2Fx(2));
        break;
    }
    }

    if (VAL(actor, CHECKPOINT_STATE) > 0 && VAL(actor, CHECKPOINT_STATE) <= 51) {
        switch (VAL(actor, CHECKPOINT_STATE)++) {
        default:
            break;

        case 1: {
            play_state_sound("sprout", 0, NULL);
            break;
        }

        case 51: {
            const GamePlayer* player = get_player(actor->player);
            if (player == NULL)
                break;

            const Sint32 r = rng(4);
            play_state_sound(get_character_voice(gamecontext()->players[player->id].character,
                                 (r > 0) ? (PV_CHECKPOINT1 + (r - 1)) : PV_READY),
                0, NULL);
            break;
        }
        }
    }
}

static void draw(const GameActor* actor) {
    batch_reset();
    batch_color(B_ALPHA((gamestate()->checkpoint <= actor->id) ? 255 : 128));

    const char* sprite = "markers/checkpoint/0";
    switch (VAL(actor, CHECKPOINT_ANIMATION)) {
    default:
        break;
    case 1:
        sprite = fmt("markers/checkpoint/%i", Fx2Int(VAL(actor, CHECKPOINT_FRAME)) % 2);
        break;
    case 2:
        sprite = fmt("markers/checkpoint/%i", 1 + Fx2Int(VAL(actor, CHECKPOINT_FRAME)));
        break;
    }

    draw_actor(actor, sprite, FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER)
        return;

    GameState* game_state = gamestate();
    if (game_state->checkpoint >= actor->id)
        return;

    VAL(actor, CHECKPOINT_ANIMATION) = 1;
    VAL(actor, CHECKPOINT_FRAME) = Fx0;
    ++VAL(actor, CHECKPOINT_STATE);

    GameActor* effect = create_actor(ACT_CHECKPOINT_EFFECT, Vadd(actor->pos, (FVec2){Int2Fx(54), Int2Fx(33)}));
    if (effect != NULL)
        VAL(effect, Y_SPEED) = Int2Fx(-10);

    game_state->checkpoint = actor->id;
}

const ActorTable TAB_CHECKPOINT = {
    .load = load,
    .create = create,
    .cleanup = cleanup,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};

/* =================
   CHECKPOINT EFFECT
   ================= */

static void load_effect() {
    load_sprite("markers/checkpoint/effect", AKL_NEVER);
}

static void create_effect(GameActor* actor) {
    actor->depth = Int2Fx(-1000);

    VAL(actor, CHECKPOINT_EFFECT_ALPHA) = Fx1;
}

static void tick_effect(GameActor* actor) {
    if (ANY_FLAG(actor, FLG_CHECKPOINT_EFFECT_TRAIL)) {
        ++actor->depth;

        VAL(actor, CHECKPOINT_EFFECT_ALPHA) -= 5120;
        if (VAL(actor, CHECKPOINT_EFFECT_ALPHA) <= Fx0)
            FLAG_ON(actor, FLG_DESTROY);

        return;
    }

    if (VAL(actor, Y_SPEED) < Fx0) {
        GameActor* effect = create_actor(ACT_CHECKPOINT_EFFECT, actor->pos);
        if (effect != NULL) {
            VAL(effect, CHECKPOINT_EFFECT_FRAME) = VAL(actor, CHECKPOINT_EFFECT_FRAME);
            VAL(effect, CHECKPOINT_EFFECT_ALPHA) = 40960;
            FLAG_ON(effect, FLG_CHECKPOINT_EFFECT_TRAIL);
            align_interp(effect, actor);
        }
    }

    if (!in_any_view(actor->pos, Int2Fx(-40), FALSE)) {
        FLAG_ON(actor, FLG_DESTROY);
        return;
    }

    move_actor(actor, POS_SPEED(actor));

    ++VAL(actor, CHECKPOINT_EFFECT_FRAME);
    if (VAL(actor, CHECKPOINT_EFFECT_FRAME) > 100) {
        VAL(actor, CHECKPOINT_EFFECT_ALPHA) -= 2560;
        if (VAL(actor, CHECKPOINT_EFFECT_ALPHA) <= Fx0) {
            FLAG_ON(actor, FLG_DESTROY);
            return;
        }
    }

    if (VAL(actor, Y_SPEED) < Fx0)
        VAL(actor, Y_SPEED) = Fmin(VAL(actor, Y_SPEED) + 26214, Fx0);
}

static void draw_effect(const GameActor* actor) {
    batch_reset();
    batch_angle(((float)SDL_min(VAL(actor, CHECKPOINT_EFFECT_FRAME), 20) / 20.f) * 2.f * SDL_PI_F);
    batch_color(B_ALPHA(Fx2Float(VAL(actor, CHECKPOINT_EFFECT_ALPHA)) * 255.f));
    draw_actor(actor, "markers/checkpoint/effect", FALSE);
}

const ActorTable TAB_CHECKPOINT_EFFECT = {
    .load = load_effect,
    .create = create_effect,
    .tick = tick_effect,
    .draw = draw_effect,
};
