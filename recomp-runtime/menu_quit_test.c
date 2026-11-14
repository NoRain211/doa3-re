/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "d3d_texture_model.h"
#include "kernel_abi.h"
#include "menu_quit.h"
#include "quit_prompt.h"
#include "xbox_memory_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Guest memory in 64 KiB pages, made on first touch. */
RecompRuntime recomp_runtime;
uint8_t *recomp_fast_ram;
static uint8_t *pages[0x4000000u >> 16];
uint8_t *recomp_memory(uint32_t address, size_t width)
{
    if (address >= 0x4000000u || width > 0x10000u || ((address + width - 1u) >> 16) != (address >> 16)) {
        fprintf(stderr, "bad access %08x+%zu\n", address, width);
        abort();
    }
    uint8_t **page = &pages[address >> 16];
    if (*page == NULL) *page = calloc(1u, 0x10000u);
    return *page + (address & 0xffffu);
}
uint32_t *recomp_memory_u32_checked(uint32_t address) { return (uint32_t *)(void *)recomp_memory(address, 4u); }
static uint32_t rd32(uint32_t a) { return *recomp_memory_u32(a); }
static void wr32(uint32_t a, uint32_t v) { *recomp_memory_u32(a) = v; }
static uint16_t rd16(uint32_t a) { uint16_t v; memcpy(&v, recomp_memory(a, 2u), 2u); return v; }
static void wr16(uint32_t a, uint16_t v) { memcpy(recomp_memory(a, 2u), &v, 2u); }
static uint8_t rd8(uint32_t a) { return *recomp_memory(a, 1u); }
static void wr8(uint32_t a, uint8_t v) { *recomp_memory(a, 1u) = v; }

/* The guest heap: a bump allocator from the real base. */
static uint32_t heap = XBOX_HEAP_BASE;
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment)
{
    heap = (heap + alignment - 1u) & ~(alignment - 1u);
    const uint32_t address = heap;
    heap += size;
    return address;
}

/* kernel_call_guest dispatches here: the game's play_sound is the only call. */
static unsigned sounds, last_sound;
void recomp_dispatch_indirect_site(uint32_t address, uint32_t saved_esp, const char *member, int line)
{
    (void)member; (void)line;
    if (address != 0x0009fd00u) { fprintf(stderr, "unexpected call %08x\n", address); abort(); }
    last_sound = rd32(saved_esp - 8u);
    ++sounds;
}

enum {
    COUNT = 0x0036847cu, GAME_STATE = 0x00480b70u, SCENE = 0x008612adu, STATE = 0x008610e0u,
    EDGES = 0x0086132au, RECORDS = 0x002fd668u, SLOTS = 0x0048f288u,
    HELP_TEXT = 0x00491ae0u, HELP_COLOUR = 0x00491af0u,
    PICTURE = 0x2ffu, LABEL = 0x4ffu,
};

static int failures;
static void expect(const char *what, int ok)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

/* Calls a wrapper with the arguments on the guest stack and a fake original. */
static unsigned originals;
static void original(void) { ++originals; recomp_runtime.registers.esp += 4u; recomp_runtime.registers.eax = 0x1111u; }
static uint32_t call(void (*wrapper)(RecompFunction), const uint32_t *args, unsigned count)
{
    const uint32_t esp = 0x00100f00u;
    for (unsigned i = 0; i < count; ++i) wr32(esp + 4u + 4u * i, args[i]);
    wr32(esp, 0u);
    recomp_runtime.registers.esp = esp;
    wrapper(original);
    expect("the wrapper pops its return address", recomp_runtime.registers.esp == esp + 4u);
    return recomp_runtime.registers.eax;
}

/* The game's draw, as far as the wrapper cares: it reads the record while it runs. */
static uint32_t slot_seen;
static void game_draw(void)
{
    slot_seen = rd32(RECORDS + 12u * rd32(rd32(recomp_runtime.registers.esp + 4u)) + 8u);
    recomp_runtime.registers.esp += 4u;
}

static void make_game(void)
{
    wr16(RECORDS + 12u * PICTURE + 4u, 128u);
    wr16(RECORDS + 12u * PICTURE + 6u, 128u);
    wr32(RECORDS + 12u * PICTURE + 8u, 210u);
    wr16(RECORDS + 12u * LABEL + 2u, 168u);
    wr16(RECORDS + 12u * LABEL + 4u, 160u);
    wr16(RECORDS + 12u * LABEL + 6u, 192u);
    wr32(RECORDS + 12u * LABEL + 8u, 307u);
    wr32(COUNT, 10u);
}

static void press(unsigned pad, uint16_t buttons, uint16_t dpad)
{
    for (unsigned i = 0; i < 4u; ++i) { wr16(EDGES + 8u * i, 0u); wr16(EDGES + 2u + 8u * i, 0u); }
    wr16(EDGES + 8u * pad, buttons);
    wr16(EDGES + 2u + 8u * pad, dpad);
}

/* The menu's words: a P8 256x256 texture with blocks for letters where the
   label looks for them. Palette: 1 white body, 152 the outline grey, black at
   rising alphas in 2-41, 255 clear. */
enum { WORDS_PIXELS = 0x00300000u, WORDS_COLOURS = 0x00320000u, WORDS_TEXTURE = 0x00321000u,
    WORDS_PALETTE = 0x00321100u, WORDS_SLOT = 112u };
static uint32_t make_words(void)
{
    for (unsigned i = 0u; i < 256u; ++i) wr32(WORDS_COLOURS + 4u * i, 0u);
    wr32(WORDS_COLOURS + 4u * 1u, 0xffffffffu);
    wr32(WORDS_COLOURS + 4u * 152u, 0xff333333u);
    for (unsigned i = 2u; i < 42u; ++i) wr32(WORDS_COLOURS + 4u * i, (i * 3u) << 24);
    uint32_t mask_x, mask_y;
    recomp_d3d_texture_swizzle_masks(256u, 256u, &mask_x, &mask_y);
    for (uint32_t y = 0u; y < 256u; ++y)
        for (uint32_t x = 0u; x < 256u; ++x)
            wr8(WORDS_PIXELS + recomp_d3d_texture_swizzle_offset(x, y, mask_x, mask_y), 255u);
    static const struct { uint32_t top; float from, to; } blocks[] = {
        {0, 86, 95}, {48, 16, 25}, {48, 30, 39}, {168, 61, 65}, {144, 34, 44}, {168, 81, 91}, {24, 55, 65},
        {0, 66, 84}, {0, 109, 119},
    };
    for (unsigned b = 0u; b < sizeof blocks / sizeof blocks[0]; ++b)
        for (uint32_t y = 4u; y < 14u; ++y)
            for (uint32_t x = 0u; x < 256u; ++x) {
                const float u = (float)x - 0.3f * (float)(14u - y);
                if (u >= blocks[b].from && u < blocks[b].to)
                    wr8(WORDS_PIXELS + recomp_d3d_texture_swizzle_offset(x, blocks[b].top + y, mask_x, mask_y), 1u);
            }
    wr32(WORDS_TEXTURE + 4u, WORDS_PIXELS);
    wr32(WORDS_TEXTURE + 12u, 8u << 24 | 8u << 20 | 1u << 16 | 0x0bu << 8 | 0x2au);
    wr32(WORDS_PALETTE, 0x00830001u);
    wr32(WORDS_PALETTE + 4u, WORDS_COLOURS);
    wr32(RECORDS + 12u * 0x1fdu + 8u, WORDS_SLOT);
    wr32(SLOTS + 8u * WORDS_SLOT, WORDS_TEXTURE);
    wr32(SLOTS + 8u * WORDS_SLOT + 4u, WORDS_PALETTE);
    return WORDS_PALETTE;
}

static void draw_label_now(uint32_t label_struct)
{
    const uint32_t esp = 0x00100f00u;
    wr32(esp + 4u, label_struct); wr32(esp, 0u);
    recomp_runtime.registers.esp = esp;
    recomp_menu_quit_draw(game_draw);
}

/* Texels of the label's texture that are not clear (0 before the words, 255
   after), inside and outside its rectangle, and how many are white body. */
static void count_label(uint32_t texture, unsigned *inside, unsigned *outside, unsigned *body)
{
    uint32_t mask_x, mask_y;
    recomp_d3d_texture_swizzle_masks(256u, 256u, &mask_x, &mask_y);
    *inside = *outside = *body = 0u;
    for (uint32_t y = 0u; y < 256u; ++y)
        for (uint32_t x = 0u; x < 256u; ++x) {
            const uint8_t texel = rd8(rd32(texture + 4u) + recomp_d3d_texture_swizzle_offset(x, y, mask_x, mask_y));
            const bool in = x < 160u && y >= 168u && y < 192u;
            if (in && texel == 1u) ++*body;
            if (texel == 0u || (in && texel == 255u)) continue;
            if (in) ++*inside; else ++*outside;
        }
}

int main(void)
{
    make_game();
    recomp_menu_quit_install();
    uint32_t zero = 0u, position = 10u, mode = 9u, help = 0x7200u;

    call(recomp_menu_quit_build, &zero, 0u);
    expect("the count grows by one", rd32(COUNT) == 11u && originals == 1u);
    originals = 0u;

    const uint32_t entry = call(recomp_menu_quit_entry, &position, 1u);
    expect("the last position answers with Quit Game's entry", entry != 0u && originals == 0u &&
        rd32(entry) == 9u && rd16(entry + 4u) == LABEL && rd16(entry + 6u) == PICTURE && rd32(entry + 8u) == 0x7200u);
    position = 3u;
    call(recomp_menu_quit_entry, &position, 1u);
    expect("another position is the game's", originals == 1u);
    position = 200u;
    expect("a position past the end clamps to Quit Game", call(recomp_menu_quit_entry, &position, 1u) == entry);
    expect("its mode finds the entry", call(recomp_menu_quit_mode, &mode, 1u) == entry && originals == 1u);
    mode = 5u;
    call(recomp_menu_quit_mode, &mode, 1u);
    expect("another mode is the game's", originals == 2u);

    const uint32_t help_args[2] = {0x7200u, 0xffffu};
    wr32(HELP_TEXT, 0u);
    call(recomp_menu_quit_help, help_args, 2u);
    expect("the help line is set", rd32(HELP_COLOUR) == 0xffffu && rd32(HELP_TEXT) != 0u && originals == 2u &&
        memcmp(recomp_memory(rd32(HELP_TEXT), 4u), "@-5Q", 4u) == 0);
    help = 0x48u;
    call(recomp_menu_quit_help, &help, 1u);
    expect("another help line is the game's", originals == 3u);

    /* The draw: the records point at the runner's slots only while the game draws. */
    const uint32_t record = RECORDS + 12u * LABEL;
    const uint32_t label_struct = 0x00101000u, picture_struct = 0x00101010u, other_struct = 0x00101020u;
    wr32(label_struct, LABEL); wr32(picture_struct, PICTURE); wr32(other_struct, 0x1fdu);
    wr32(RECORDS + 12u * 0x1fdu + 8u, 112u);
    wr8(GAME_STATE, 0u); wr8(SCENE, 0u);
    {   /* the fake draw is the original: run the wrapper with it */
        const uint32_t esp = 0x00100f00u;
        wr32(esp + 4u, label_struct); wr32(esp, 0u);
        recomp_runtime.registers.esp = esp;
        recomp_menu_quit_draw(game_draw);
    }
    expect("the label's record names the runner's slot during the draw",
        slot_seen != 307u && rd32(SLOTS + 8u * slot_seen) != 0u);
    expect("the record is put back", rd32(record + 8u) == 307u);
    {
        const uint32_t esp = 0x00100f00u;
        wr32(esp + 4u, picture_struct); wr32(esp, 0u);
        recomp_runtime.registers.esp = esp;
        recomp_menu_quit_draw(game_draw);
    }
    expect("so is the picture's", slot_seen != 210u && rd32(RECORDS + 12u * PICTURE + 8u) == 210u);
    {
        const uint32_t esp = 0x00100f00u;
        wr32(esp + 4u, other_struct); wr32(esp, 0u);
        recomp_runtime.registers.esp = esp;
        recomp_menu_quit_draw(game_draw);
    }
    expect("another sprite is untouched", slot_seen == 112u);
    wr8(SCENE, 4u);
    {
        const uint32_t esp = 0x00100f00u;
        wr32(esp + 4u, label_struct); wr32(esp, 0u);
        recomp_runtime.registers.esp = esp;
        recomp_menu_quit_draw(game_draw);
    }
    expect("outside the main menu the game's art is untouched", slot_seen == 307u);
    wr8(SCENE, 0u);

    /* The slots: texture and palette. The label stays clear until the menu's words load. */
    wr32(label_struct, LABEL);
    draw_label_now(label_struct);
    const uint32_t slot = SLOTS + 8u * slot_seen, texture = rd32(slot), palette = rd32(slot + 4u);
    expect("the slot has a texture and a palette", texture != 0u && palette != 0u);
    expect("the texture is P8, 256x256", ((rd32(texture + 12u) >> 8) & 0xffu) == 0x0bu &&
        ((rd32(texture + 12u) >> 20) & 0xfu) == 8u && ((rd32(texture + 12u) >> 24) & 0xfu) == 8u);
    expect("the palette is a palette", (rd32(palette) & 0x70000u) == 0x30000u && rd32(palette + 4u) != 0u);
    unsigned inside, outside, body;
    count_label(texture, &inside, &outside, &body);
    expect("before the words load the label is clear", inside == 0u && outside == 0u);

    const uint32_t words_palette = make_words();
    draw_label_now(label_struct);
    expect("the label draws in the words' palette", rd32(slot + 4u) == words_palette);
    count_label(texture, &inside, &outside, &body);
    expect("the label is cut from the words, inside its rectangle only", body > 200u && inside > body && outside == 0u);

    /* The confirm. */
    wr16(STATE + 0xfau, 0u); wr16(STATE + 0xfcu, 1u); wr8(STATE + 0x224u, 0u); wr8(STATE + 0x1dcu, 10u);
    press(1u, 0x10u, 0u);
    originals = 0u;
    call(recomp_menu_quit_update, &zero, 0u);
    expect("A on Quit Game asks instead of confirming", originals == 0u && recomp_quit_prompt_open() &&
        sounds == 1u && last_sound == 0x24u);
    call(recomp_menu_quit_update, &zero, 0u);
    expect("while it asks the game runs on", originals == 1u && sounds == 1u);
    /* Open the prompt for real and cancel it, to start afresh. */
    RecompInputGamepad pad = {0};
    expect("the filter opens the requested prompt", recomp_quit_prompt_filter(0u, &pad, 100u) == RECOMP_QUIT_OPENED);
    pad.analog_buttons[1] = 0x20u;
    pad.buttons = 0u;
    for (unsigned i = 0u; i < 3u && recomp_quit_prompt_open(); ++i) recomp_quit_prompt_filter(0u, &pad, 200u + i);
    memset(&pad, 0, sizeof pad);
    for (unsigned i = 0u; i < 3u; ++i) recomp_quit_prompt_filter(0u, &pad, 300u + i);
    expect("B keeps playing", !recomp_quit_prompt_open());

    originals = 0u;
    press(0u, 0x10u, 0x4u);
    call(recomp_menu_quit_update, &zero, 0u);
    expect("a d-pad edge moves the cursor instead", originals == 1u && !recomp_quit_prompt_open());
    press(0u, 0x10u, 0u);
    wr8(STATE + 0x1dcu, 4u);
    call(recomp_menu_quit_update, &zero, 0u);
    expect("A on another entry is the game's", originals == 2u && !recomp_quit_prompt_open());
    wr8(STATE + 0x1dcu, 10u);
    wr16(STATE + 0xfcu, 2u);
    call(recomp_menu_quit_update, &zero, 0u);
    expect("an unsettled menu is the game's", originals == 3u && !recomp_quit_prompt_open());

    /* A game with a full list or a locked mode: the position follows the count. */
    wr32(COUNT, 9u);
    call(recomp_menu_quit_build, &zero, 0u);
    expect("a shorter list puts Quit Game at its end", rd32(COUNT) == 10u);
    position = 9u;
    expect("the last position of the shorter list", call(recomp_menu_quit_entry, &position, 1u) == entry);

    if (failures == 0) puts("menu quit: ok");
    return failures == 0 ? 0 : 1;
}
