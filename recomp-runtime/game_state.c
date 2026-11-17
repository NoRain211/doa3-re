/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_state.h"

#include "doa3_build.h"
#include "input_pulse_source.h"
#include "runtime.h"

/* Addresses from static analysis of both XBEs (docs/rollback-netcode.md). */
enum {
    GAME_STATE = DOA3(0x00480b70u, 0x00598e80u, 0x005998c0u),       /* 0 front end, 1 match, 2 title, 3 post-match */
    PAUSE_OPEN = DOA3(0x0048a470u, 0x005a27a0u, 0x005a31e0u),
    TASKS_SUSPENDED = DOA3(0x0047e795u, 0x00596aa5u, 0x005974e5u),
    ROUND_PHASE = DOA3(0x004a0474u, 0x005b1424u, 0x005b1e64u),      /* 1 live */
    ROUND_DECIDED = DOA3(0x00480960u, 0x00598c70u, 0x005996b0u),    /* KO, double KO or time up */
    TIME_UP = DOA3(0x0085ce9cu, 0x009b379cu, 0x0096e7fcu),
    FIGHTER1_LIFE = DOA3(0x00484c68u, 0x0059cf78u, 0x0059d9b8u),
    SCENE = DOA3(0x008612adu, 0x009b7b8du, 0x00972bedu),            /* 0 main menu, 2 battle type, 3 select */
    SCENE_STATES = DOA3(0x008611dcu, 0x009b7abcu, 0x00972b1cu),     /* per scene, stride 0x14: 1 settled */
    MENU_CURSOR = DOA3(0x008612bcu, 0x009b7b9cu, 0x00972bfcu),
    MENU_VERSUS = 5,
};

static uint8_t byte_at(uint32_t address)
{
    return *recomp_memory(address, 1u);
}

bool recomp_game_round_live(void)
{
    return byte_at(GAME_STATE) == 1u && byte_at(PAUSE_OPEN) == 0u && byte_at(TASKS_SUSPENDED) == 0u &&
        byte_at(ROUND_PHASE) == 1u && byte_at(ROUND_DECIDED) == 0u && byte_at(TIME_UP) == 0u;
}

bool recomp_game_fight_shown(void)
{
    const uint8_t state = byte_at(GAME_STATE);
    return state == 1u || state == 3u;
}

bool recomp_game_walk_to_versus(uint32_t frame, bool select, RecompInputGamepad *pad)
{
    const uint8_t state = byte_at(GAME_STATE), scene = byte_at(SCENE), cursor = byte_at(MENU_CURSOR);
    const bool settled = scene < 16u && byte_at(SCENE_STATES + scene * 0x14u) == 1u;
    const bool front_end = state == 0u;
    if (front_end && settled &&
        (select ? scene == 3u : scene == 0u && cursor == MENU_VERSUS))
        return true;
    const RecompInputGamepad none = {0};
    *pad = none;
    if (frame % 16u != 0u || (front_end && !settled)) return false;
    if (!front_end) pad->buttons = RECOMP_INPUT_BUTTON_START;
    else if (scene == 0u && cursor != MENU_VERSUS) pad->buttons = 8u; /* d-pad right */
    else pad->analog_buttons[RECOMP_INPUT_ANALOG_A] = RECOMP_INPUT_ANALOG_PRESSED;
    return false;
}

bool recomp_game_versus_chosen(void)
{
    return recomp_game_at_main_menu() && byte_at(MENU_CURSOR) == MENU_VERSUS;
}

bool recomp_game_at_main_menu(void)
{
    return byte_at(GAME_STATE) == 0u && byte_at(SCENE) == 0u && byte_at(SCENE_STATES) == 1u;
}

uint32_t recomp_game_life_address(void)
{
    return FIGHTER1_LIFE;
}
