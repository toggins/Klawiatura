#include "K_audio.h"
#include "K_string.h"
#include "K_video.h"

#include "actors/K_blocks.h"
#include "actors/K_player.h"
#include "actors/K_pswitch.h"

static void load() {
    load_sprite_num("markers/pswitch/%u", 3, AKL_NEVER);
    load_sprite("markers/pswitch/flat", AKL_NEVER);
    load_sound("ui/toggle", AKL_NEVER);
    load_track("smw/pswitch", AKL_NEVER);
}

static void create(GameActor* actor) {
    actor->box.start.x = Int2Fx(-15);
    actor->box.start.y = Int2Fx(-31);
    actor->box.end.x = Int2Fx(16);
    actor->box.end.y = Fx1;

    actor->depth = 131073;
}

static void tick(GameActor* actor) {
    ++VAL(actor, PSWITCH_FRAME);
}

static void draw(const GameActor* actor) {
    batch_reset();
    draw_actor(actor,
        ANY_FLAG(actor, FLG_PSWITCH_FLAT) ? "markers/pswitch/flat"
                                          : fmt("markers/pswitch/%i", (VAL(actor, PSWITCH_FRAME) / 2) % 3),
        FALSE);
}

static void collide(GameActor* actor, GameActor* from) {
    if (from->type != ACT_PLAYER || ANY_FLAG(actor, FLG_PSWITCH_FLAT) || from->pos.y >= (actor->pos.y - Int2Fx(16))
        || (from->vel.y < Fx0 && !ANY_FLAG(from, FLG_PLAYER_STOMP)))
    {
        return;
    }

    GameActor* replacee = NULL;
    FOR_EACH_ACTOR (replacee) {
        switch (replacee->type) {
        default:
            break;

        case ACT_BLOCK: {
            if (VAL(replacee, BLOCK_TYPE) != BLOCK_BRICK || VAL(replacee, BLOCK_ITEM) != ACT_NULL
                || ANY_FLAG(replacee, FLG_BLOCK_EMPTY | FLG_BLOCK_HIDDEN))
            {
                break;
            }

            const ActorFlags flags = replacee->flags & FLG_BLOCK_GRAY;
            replace_actor(replacee, ACT_PSWITCH_COIN);
            FLAG_ON(replacee, flags);

            break;
        }

        case ACT_COIN: {
            replace_actor(replacee, ACT_PSWITCH_BLOCK);
            VAL(replacee, BLOCK_TYPE) = BLOCK_BRICK;

            break;
        }
        }
    }

    from->vel.y = Int2Fx(-3);
    FLAG_ON(actor, FLG_PSWITCH_FLAT);

    gamestate()->pswitch = 500;
    for (PlayerID i = 0, n = gamecontext()->num_players; i < n; i++)
        update_player_track(get_player(i));

    play_state_sound("ui/toggle", PLAY_POS, A_ACTOR(actor));
}

const ActorTable TAB_PSWITCH = {
    .load = load,
    .create = create,
    .tick = tick,
    .draw = draw,
    .collide = collide,
};
