#include "K_audio.h"
#include "K_cmd.h"
#include "K_input.h"
#include "K_interface.h"
#include "K_levels.h"
#include "K_net.h"
#include "K_string.h"
#include "K_video.h"
#include "K_worlds.h"

static void yes_option() {
    load_world_save(get_world_key(((WorldSaveInfo*)topui()->userdata)->world));
}

static void no_option() {
    if (is_client())
        return;

    const World* world = get_world(CLIENT.world);
    if (world == NULL)
        return;

    if (is_connected())
        peers_to_players();

    WorldContext ctx = init_world_context(world);
    jump_to_world(&ctx, TRUE);
}

static Option SAVE_OPTIONS[MAX_OPTIONS] = {
    {.name = "option.yes", .callback = yes_option},
    {.name = "option.no",  .callback = no_option },
};
static size_t save_option = 0;
static Uint32 character_frame = 0;

static void create_save(UI* ui) {
    UI_ALLOC_DATA(ui, WorldSaveInfo);

    save_option = 0;
    character_frame = 0;
}

static void tick_save(UI* ui) {
    if (kb_pressed(KB_UI_BACK)) {
        ui->flags |= UIF_DESTROY;
        play_generic_sound("ui/select", 0);

        return;
    }

    character_frame += 3;
    tick_options(NULL, SAVE_OPTIONS, &save_option, 0);
}

static void draw_save(const UI* ui) {
    batch_reset();
    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, 16.f));
    batch_colors(B_U4X4_YELLOW);
    batch_align(B_ALIGN(FA_CENTER, FA_TOP));
    batch_string("header", 32.f, LFMT("message.continue_from_save"));

    const WorldSaveInfo* save = ui->userdata;
    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT - 32.f));
    batch_color(B_U4_WHITE);
    batch_sprite(get_character_sprite(save->character, save->powerup, PF_WALK1 + ((character_frame / 25) % 3)));
    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT - 24.f));
    batch_string("footer", 16.f, fmt("%s x %i", get_character_name(save->character), save->lives));

    const Level* level = get_level_key(save->level);
    if (level != NULL) {
        batch_pos(B_F3_HALF_SCREEN);
        batch_string_wrap("header", 32.f, LFMT(fmt("level.%s", level->name)), SCREEN_HEIGHT - 32.f);
    }

    draw_options(SAVE_OPTIONS, save_option, SCREEN_HEIGHT - 112.f);

    batch_pos(B_F3_XY(HALF_SCREEN_WIDTH, SCREEN_HEIGHT - 16.f));
    batch_color(B_U4_WHITE);
    batch_align(B_ALIGN(FA_CENTER, FA_BOTTOM));
    batch_string("footer", 16.f,
        fmt("[%s] %s   [%s] %s", kb_label(KB_UI_ENTER), LFMT("menu.select"), kb_label(KB_UI_BACK), LFMT("menu.back")));
}

const UITable TAB_SAVE = {
    .create = create_save,
    .tick = tick_save,
    .draw = draw_save,
};
