/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_quit.h"
#include "d3d_texture_model.h"
#include "kernel_abi.h"
#include "quit_icon.h"
#include "quit_prompt.h"
#include "xbox_memory_layout.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The main menu (scene 0, update 0x000d0060) is a carousel over a table of ten
   12-byte entries at 0x00368480: mode, then the label sprite and the picture
   sprite as two 16-bit ids, then the number of its help line. 0x000c8010 lists
   the unlocked ones' table indices at 0x00860d70 and their count at
   0x0036847c; 0x000c83a0 answers for a position, 0x000c7fc0 for a mode.

   Quit Game is one more position after the listed ones: the count grows by
   one and the two readers answer for that position from an entry of the
   runner's. Its confirm never reaches the game: the update opens the quit
   prompt instead, so no transition starts and keeping on needs no undoing.

   The game draws a sprite by id through a record table: 0x002fd668 holds a
   12-byte record per id (rectangle, then the index of its texture slot in the
   table at 0x0048f288, two words each: texture, palette), and an entry can
   name only 16-bit ids, all inside the table. So the entry names two ids the
   main menu never draws, and the draw wrapper points their records at the
   runner's textures for the length of the call: the queued command keeps the
   slot's address, and the record is put back before anything else reads it. */

enum {
    MENU_BUILD = 0x000c8010u, MENU_ENTRY = 0x000c83a0u, MENU_MODE = 0x000c7fc0u,
    MENU_HELP = 0x000559f0u, MENU_DRAW = 0x0006d140u, MENU_UPDATE = 0x000d0060u,
    PLAY_SOUND = 0x0009fd00u,        /* (sound, 0) */
    COUNT = 0x0036847cu,             /* u32 */
    GAME_STATE = 0x00480b70u,        /* 0 front end */
    SCENE = 0x008612adu,             /* 0 main menu */
    MENU_STATE = 0x008610e0u,        /* +0xfa, +0xfc: 0 and 1 once settled; +0x1dc cursor; +0x224 */
    PAD_EDGES = 0x0086132au,         /* per pad, stride 8: the buttons pressed this frame; +2: d-pad */
    SPRITE_RECORDS = 0x002fd668u,    /* 12 bytes per id: l, t, r, b (u16); texture slot (u32) */
    SPRITE_SLOTS = 0x0048f288u,      /* 8 bytes per slot: texture, palette */
    HELP_TEXT = 0x00491ae0u, HELP_COLOUR = 0x00491af0u,

    /* The confirm's table of available modes covers 0-0xe, all 2 but Versus when one
       pad is connected. Mode 9 is in it, no entry's, and the mode switch ignores it. */
    QUIT_MODE = 9u,
    HELP_NUMBER = 0x7200u,           /* past the 0x4a the game's tables hold */
    SOUND_CONFIRM = 0x24u,
    CONFIRM_BUTTONS = 0x110u,        /* what the update's confirm tests: A and Start */
    LEFT_RIGHT = 0x0cu,

    /* Two sprites the main menu never draws: a 128x128 picture of the select
       screens' set and a label's row from the HUD's text. The install checks
       their sizes. */
    PICTURE_ID = 0x2ffu, LABEL_ID = 0x4ffu,
    LABEL_HEIGHT = 24, PICTURE_SIZE = 128,
};

typedef struct Sprite {
    uint32_t record, entry;  /* the game's record of its id; the runner's slot */
    uint32_t slot;           /* the slot's index in the game's table */
    uint32_t pixels, mask_x, mask_y;
    int left, top, width, height;
} Sprite;

static bool ready, label_made;
static int quit_position = -1;
static uint32_t entry;       /* the 12-byte menu entry */
static uint32_t help;        /* the help line */
static Sprite picture, label;

static uint32_t u32(uint32_t address) { return *recomp_memory_u32(address); }
static uint16_t u16(uint32_t address) { uint16_t v; memcpy(&v, recomp_memory(address, 2u), 2u); return v; }
static uint8_t u8(uint32_t address) { return *recomp_memory(address, 1u); }
static void set_u32(uint32_t address, uint32_t v) { *recomp_memory_u32(address) = v; }
static void set_u16(uint32_t address, uint16_t v) { memcpy(recomp_memory(address, 2u), &v, 2u); }

/* ---- The pictures ---------------------------------------------------- */

/* A palette of 0 clear; 8-63 eight alphas by eight greys for edges and the
   shadow; 64-255 opaque greys. */
static uint8_t palette_index(uint8_t grey, uint8_t alpha)
{
    if (alpha >= 250u) return (uint8_t)(64u + (grey * 191u + 127u) / 255u);
    const unsigned a = (alpha * 7u + 127u) / 255u;
    return a == 0u ? 0u : (uint8_t)(a * 8u + (grey * 7u + 127u) / 255u);
}

/* The label is cut from the main menu's own words (sprites 0x1fd-0x206, all
   on one 256x256 P8 texture): letter bodies only, packed as tightly as the
   game packs them, then outlined and shadowed as the words are. The words
   slant 0.3 px per row and brighten from grey at their left end to white by
   x 56; a letter that moves keeps the brightness of its new place. All fitted
   to the game's art; the art itself stays on the disc. */
enum {
    WORDS_ID = 0x1fdu,                 /* STORY MODE, at the top of the words' texture */
    WORDS_SIZE = 256, CLEAR = 255,     /* the words' palette: 255 is clear */
    BODY_GREY = 0x45, MIN_PIECE = 8,   /* lighter than the outline; specks are a neighbour's */
    GAP = 3, SPACE = 5,                /* clear columns between letters, and more between words */
    SHADOW_DROP = 3, SHADOW_RADIUS = 2,
};
#define LEAN 0.3f
#define SHADOW_GAIN 0.435f

/* Each letter: the row of its word in the texture and a slanted column range
   that holds most of it. Q is the O of MODE with the leg of SURVIVAL's R. */
static const struct { char letter; uint8_t top, from, to; } LETTERS[] = {
    {'Q', 0, 84, 97}, {'U', 48, 14, 27}, {'I', 168, 59, 67}, {'T', 144, 32, 46}, {' ', 0, 0, 0},
    {'G', 168, 79, 93}, {'A', 24, 53, 67}, {'M', 0, 64, 86}, {'E', 0, 107, 121},
};
enum { TAIL_TOP = 48, TAIL_FROM = 33, TAIL_TO = 40, TAIL_ROW = 10, TAIL_ROWS = 5, TAIL_DX = -4, TAIL_DY = 2 };

typedef struct Words {
    uint32_t pixels, colours, mask_x, mask_y;
} Words;

static uint8_t word_index(const Words *w, int x, int y)
{
    return u8(w->pixels + recomp_d3d_texture_swizzle_offset((uint32_t)x, (uint32_t)y, w->mask_x, w->mask_y));
}

static uint32_t colour(const Words *w, uint8_t index) { return u32(w->colours + 4u * index); }

static bool is_body(const Words *w, uint8_t index)
{
    const uint32_t c = colour(w, index);
    return (c >> 24) == 255u && ((c >> 16) & 0xffu) >= BODY_GREY && (c & 0xffffffu) != 0u;
}

static float slanted(int x, int y) { return (float)x - LEAN * (float)(14 - y); }

static float brightness(float x)
{
    static const float RAMP[][2] = {{0, .64f}, {8, .66f}, {16, .68f}, {24, .71f}, {32, .80f}, {44, .90f}, {52, .94f}, {56, 1}};
    if (x <= RAMP[0][0]) return RAMP[0][1];
    for (unsigned i = 1u; i < sizeof RAMP / sizeof RAMP[0]; ++i)
        if (x <= RAMP[i][0])
            return RAMP[i - 1][1] + (RAMP[i][1] - RAMP[i - 1][1]) * (x - RAMP[i - 1][0]) / (RAMP[i][0] - RAMP[i - 1][0]);
    return 1.0f;
}

/* The opaque grey of the palette closest to grey; the original if none. */
static uint8_t grey_index(const Words *w, uint8_t original, unsigned grey)
{
    unsigned best = original, distance = 1000u;
    for (unsigned i = 0u; i < 256u; ++i) {
        const uint32_t c = colour(w, (uint8_t)i);
        const unsigned r = (c >> 16) & 0xffu;
        if ((c >> 24) != 255u || r != ((c >> 8) & 0xffu) || r != (c & 0xffu)) continue;
        const unsigned d = r > grey ? r - grey : grey - r;
        if (d < distance) { distance = d; best = i; }
    }
    return (uint8_t)best;
}

/* The bodies of one word's letters that lie mostly in [from, to). */
typedef struct Pixel { int16_t x, y; } Pixel;
static int letter_pixels(const Words *w, int top, float from, float to, Pixel *out, int capacity)
{
    static uint8_t seen[LABEL_HEIGHT][WORDS_SIZE];
    static Pixel piece[LABEL_HEIGHT * WORDS_SIZE];
    memset(seen, 0, sizeof seen);
    int count = 0;
    for (int y = 0; y < LABEL_HEIGHT; ++y)
        for (int x = 0; x < WORDS_SIZE; ++x) {
            if (seen[y][x] || !is_body(w, word_index(w, x, top + y))) continue;
            int n = 0, inside = 0, next = 0;
            seen[y][x] = 1u;
            piece[n++] = (Pixel){(int16_t)x, (int16_t)y};
            while (next < n) {
                const Pixel p = piece[next++];
                const float u = slanted(p.x, p.y);
                if (u >= from && u < to) ++inside;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = p.x + dx, ny = p.y + dy;
                        if (nx < 0 || nx >= WORDS_SIZE || ny < 0 || ny >= LABEL_HEIGHT || seen[ny][nx] ||
                            !is_body(w, word_index(w, nx, top + ny)))
                            continue;
                        seen[ny][nx] = 1u;
                        piece[n++] = (Pixel){(int16_t)nx, (int16_t)ny};
                    }
            }
            if (n < MIN_PIECE || inside * 2 <= n) continue;
            for (int i = 0; i < n && count < capacity; ++i) out[count++] = piece[i];
        }
    return count;
}

/* Lays QUIT GAME out in a WORDS_SIZE-wide row, centred in width; false if
   the words' art is not as measured. */
static bool cut_label(const Words *w, int width, uint8_t *label)
{
    static uint8_t row[LABEL_HEIGHT][WORDS_SIZE];
    static Pixel pixels[LABEL_HEIGHT * WORDS_SIZE];
    static float cover[LABEL_HEIGHT][WORDS_SIZE], pass[LABEL_HEIGHT][WORDS_SIZE];
    int right[LABEL_HEIGHT];
    memset(row, CLEAR, sizeof row);
    for (int y = 0; y < LABEL_HEIGHT; ++y) right[y] = -99;
    for (size_t l = 0u; l < sizeof LETTERS / sizeof LETTERS[0]; ++l) {
        if (LETTERS[l].letter == ' ') {
            for (int y = 0; y < LABEL_HEIGHT; ++y) if (right[y] > -99) right[y] += SPACE;
            continue;
        }
        const int top = LETTERS[l].top;
        const int n = letter_pixels(w, top, LETTERS[l].from, LETTERS[l].to, pixels, LABEL_HEIGHT * WORDS_SIZE);
        if (n == 0) return false;
        int left[LABEL_HEIGHT], end[LABEL_HEIGHT], min_x = WORDS_SIZE;
        float u_min = 1e9f, u_max = -1e9f;
        for (int y = 0; y < LABEL_HEIGHT; ++y) { left[y] = WORDS_SIZE; end[y] = -1; }
        for (int i = 0; i < n; ++i) {
            const Pixel p = pixels[i];
            if (p.x < left[p.y]) left[p.y] = p.x;
            if (p.x > end[p.y]) end[p.y] = p.x;
            if (p.x < min_x) min_x = p.x;
            const float u = slanted(p.x, p.y);
            if (u < u_min) u_min = u;
            if (u > u_max) u_max = u;
        }
        int shift = 4 - min_x;
        for (bool clash = true; clash;) {
            clash = false;
            for (int y = 0; y < LABEL_HEIGHT; ++y)
                if (end[y] >= 0 && right[y] + GAP >= left[y] + shift) { clash = true; ++shift; break; }
        }
        for (int i = 0; i <= (LETTERS[l].letter == 'Q' ? 1 : 0); ++i) {
            const int source_top = i == 0 ? top : TAIL_TOP;
            const int count = i == 0 ? n : 0;
            for (int k = 0; k < count; ++k) {
                const Pixel p = pixels[k];
                const int x = p.x + shift;
                if (x < 0 || x >= WORDS_SIZE) return false;
                uint8_t index = word_index(w, p.x, source_top + p.y);
                const unsigned red = (colour(w, index) >> 16) & 0xffu;
                if (red > 110u && shift != 0) {
                    const float grey = (float)red * brightness((float)x) / brightness((float)p.x);
                    index = grey_index(w, index, grey > 255.0f ? 255u : (unsigned)(grey + 0.5f));
                }
                row[p.y][x] = index;
            }
            if (i == 1) {  /* the tail, at the foot of the O's right side */
                const int tail_shift = (int)lroundf((float)(min_x + shift) + (u_max - u_min) + TAIL_DX - TAIL_FROM);
                for (int y = TAIL_ROW; y < TAIL_ROW + TAIL_ROWS; ++y)
                    for (int x = 0; x < WORDS_SIZE; ++x) {
                        const float u = slanted(x, y);
                        const uint8_t index = word_index(w, x, TAIL_TOP + y);
                        const int tx = x + tail_shift, ty = y + TAIL_DY;
                        if (u < TAIL_FROM || u >= TAIL_TO || !is_body(w, index)) continue;
                        if (tx >= 0 && tx < WORDS_SIZE && ty < LABEL_HEIGHT) row[ty][tx] = index;
                    }
            }
        }
        for (int y = 0; y < LABEL_HEIGHT; ++y)
            if (end[y] >= 0 && end[y] + shift > right[y]) right[y] = end[y] + shift;
    }
    /* The outline: two pixels of the words' outline grey round every body. */
    uint8_t outline = CLEAR;
    for (unsigned i = 0u; i < 256u; ++i) if (colour(w, (uint8_t)i) == 0xff333333u) { outline = (uint8_t)i; break; }
    if (outline == CLEAR) return false;
    static uint8_t body[LABEL_HEIGHT][WORDS_SIZE];
    for (int y = 0; y < LABEL_HEIGHT; ++y)
        for (int x = 0; x < WORDS_SIZE; ++x) body[y][x] = row[y][x] != CLEAR;
    int last = 0;
    for (int y = 0; y < LABEL_HEIGHT; ++y)
        for (int x = 0; x < WORDS_SIZE; ++x) {
            if (body[y][x]) { if (x > last) last = x; continue; }
            for (int dy = -2; dy <= 2 && row[y][x] == CLEAR; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    const int yy = y + dy, xx = x + dx;
                    if (dx * dx + dy * dy <= 4 && yy >= 0 && yy < LABEL_HEIGHT && xx >= 0 && xx < WORDS_SIZE &&
                        body[yy][xx]) { row[y][x] = outline; break; }
                }
        }
    /* The shadow: coverage dropped 3 px and box-blurred twice. */
    for (int y = 0; y < LABEL_HEIGHT; ++y)
        for (int x = 0; x < WORDS_SIZE; ++x)
            cover[y][x] = y >= SHADOW_DROP && row[y - SHADOW_DROP][x] != CLEAR
                ? (float)(colour(w, row[y - SHADOW_DROP][x]) >> 24) / 255.0f : 0.0f;
    for (int n = 0; n < 2; ++n) {
        for (int y = 0; y < LABEL_HEIGHT; ++y)
            for (int x = 0; x < WORDS_SIZE; ++x) {
                float sum = 0.0f;
                for (int d = -SHADOW_RADIUS; d <= SHADOW_RADIUS; ++d)
                    if (x + d >= 0 && x + d < WORDS_SIZE) sum += cover[y][x + d];
                pass[y][x] = sum / (2 * SHADOW_RADIUS + 1);
            }
        for (int y = 0; y < LABEL_HEIGHT; ++y)
            for (int x = 0; x < WORDS_SIZE; ++x) {
                float sum = 0.0f;
                for (int d = -SHADOW_RADIUS; d <= SHADOW_RADIUS; ++d)
                    if (y + d >= 0 && y + d < LABEL_HEIGHT) sum += pass[y + d][x];
                cover[y][x] = sum / (2 * SHADOW_RADIUS + 1);
            }
    }
    for (int y = 0; y < LABEL_HEIGHT; ++y)
        for (int x = 0; x < WORDS_SIZE; ++x) {
            if (row[y][x] != CLEAR) continue;
            const int alpha = (int)lroundf(255.0f * SHADOW_GAIN * cover[y][x]);
            if (alpha <= 0) continue;
            unsigned best = CLEAR, distance = 1000u;
            for (unsigned i = 0u; i < 256u; ++i) {
                const uint32_t c = colour(w, (uint8_t)i);
                if ((c & 0xffffffu) != 0u) continue;
                const unsigned a = c >> 24, d = (unsigned)abs((int)a - alpha);
                if (d < distance) { distance = d; best = i; }
            }
            row[y][x] = (uint8_t)best;
        }
    /* Centred in the label's rectangle, as the game centres it. */
    const int used = last + 1 + 6, offset = (width - used) / 2;
    for (int y = 0; y < LABEL_HEIGHT; ++y)
        for (int x = 0; x < width; ++x) {
            const int sx = x - offset;
            label[y * width + x] = sx >= 0 && sx < WORDS_SIZE ? row[y][sx] : (uint8_t)CLEAR;
        }
    return true;
}

/* ---- Guest memory ------------------------------------------------------ */

static uint32_t guest_alloc(uint32_t size, uint32_t alignment)
{
    const uint32_t address = xbox_HeapAlloc(size, alignment);
    if (address == 0u) fprintf(stderr, "recomp menu quit: no guest memory for %u bytes\n", size);
    return address;
}

static uint32_t power_of_two_log(uint32_t n)
{
    uint32_t log = 0u;
    while ((1u << log) < n) ++log;
    return log;
}

/* Makes the slot for a sprite id: a swizzled P8 texture over its rectangle,
   drawn with the shared palette. pixels is row-major, width x height, at the
   rectangle's corner. False if the id's record is not the size asked for. */
/* Row-major texels over the sprite's rectangle. */
static void write_pixels(const Sprite *sprite, const uint8_t *pixels)
{
    for (int y = 0; y < sprite->height; ++y)
        for (int x = 0; x < sprite->width; ++x)
            *recomp_memory(sprite->pixels + recomp_d3d_texture_swizzle_offset((uint32_t)(sprite->left + x),
                (uint32_t)(sprite->top + y), sprite->mask_x, sprite->mask_y), 1u) = pixels[y * sprite->width + x];
}

static bool make_sprite(Sprite *sprite, uint32_t id, int width, int height, uint32_t palette,
    const uint8_t *pixels)
{
    sprite->record = SPRITE_RECORDS + 12u * id;
    const uint32_t left = u16(sprite->record), top = u16(sprite->record + 2u),
                   right = u16(sprite->record + 4u), bottom = u16(sprite->record + 6u);
    if ((int)(right - left) != width || (int)(bottom - top) != height) {
        fprintf(stderr, "recomp menu quit: sprite %#x is %ux%u, not %dx%d\n", id, right - left, bottom - top,
            width, height);
        return false;
    }
    const uint32_t log_width = power_of_two_log(right), log_height = power_of_two_log(bottom);
    const uint32_t texture_width = 1u << log_width, texture_height = 1u << log_height;
    const uint32_t header = guest_alloc(20u, 4u), pixel_bytes = guest_alloc(texture_width * texture_height, 0x1000u);
    sprite->entry = guest_alloc(8u, 8u);
    if (header == 0u || pixel_bytes == 0u || sprite->entry == 0u) return false;
    sprite->slot = (sprite->entry - SPRITE_SLOTS) / 8u;
    set_u32(header, 0x00040001u);
    set_u32(header + 4u, pixel_bytes);
    set_u32(header + 8u, 0u);
    set_u32(header + 12u, log_height << 24 | log_width << 20 | 1u << 16 | 0x0bu << 8 | 0x2au);
    set_u32(header + 16u, 0u);
    set_u32(sprite->entry, header);
    set_u32(sprite->entry + 4u, palette);
    recomp_d3d_texture_swizzle_masks(texture_width, texture_height, &sprite->mask_x, &sprite->mask_y);
    for (uint32_t y = 0u; y < texture_height; ++y)
        for (uint32_t x = 0u; x < texture_width; ++x)
            *recomp_memory(pixel_bytes + recomp_d3d_texture_swizzle_offset(x, y, sprite->mask_x, sprite->mask_y), 1u) = 0u;
    sprite->pixels = pixel_bytes;
    sprite->left = (int)left;
    sprite->top = (int)top;
    sprite->width = width;
    sprite->height = height;
    if (pixels != NULL) write_pixels(sprite, pixels);
    return true;
}

void recomp_menu_quit_install(void)
{
    ready = label_made = false;
    quit_position = -1;
#if !defined(RECOMP_DOAXBV_BINDINGS) && !defined(RECOMP_DOA3_JP_BINDINGS)
    /* The game's record of a sprite must be readable before it is borrowed. */
    const uint32_t colours_size = 1024u;
    const uint32_t palette = guest_alloc(12u, 4u), colours = guest_alloc(colours_size, 0x80u);
    entry = guest_alloc(12u, 4u);
    static const char HELP[] = "@-5Quit the game and return to the desktop";
    help = guest_alloc((uint32_t)sizeof HELP, 4u);
    if (palette == 0u || colours == 0u || entry == 0u || help == 0u) return;
    for (unsigned i = 0u; i < 256u; ++i) {
        uint32_t a, g;
        if (i >= 64u) { a = 255u; g = (i - 64u) * 255u / 191u; }
        else { a = (i / 8u) * 255u / 7u; g = (i % 8u) * 255u / 7u; }
        set_u32(colours + 4u * i, a << 24 | g << 16 | g << 8 | g);
    }
    set_u32(palette, 0x00830001u);
    set_u32(palette + 4u, colours);
    memcpy(recomp_memory(help, (size_t)sizeof HELP), HELP, sizeof HELP);

    static uint8_t picture_pixels[PICTURE_SIZE * PICTURE_SIZE];
    static uint8_t grey[PICTURE_SIZE * PICTURE_SIZE], alpha[PICTURE_SIZE * PICTURE_SIZE];
    recomp_quit_icon_draw(1.0f, grey, alpha);
    for (uint32_t i = 0u; i < PICTURE_SIZE * PICTURE_SIZE; ++i) picture_pixels[i] = palette_index(grey[i], alpha[i]);
    const uint32_t label_width = (uint32_t)u16(SPRITE_RECORDS + 12u * LABEL_ID + 4u) -
        u16(SPRITE_RECORDS + 12u * LABEL_ID);
    if (label_width == 0u || label_width > 256u) {
        fprintf(stderr, "recomp menu quit: label sprite %#x is %u wide\n", LABEL_ID, label_width);
        return;
    }
    /* The label stays clear until the menu's words are loaded (make_label). */
    if (!make_sprite(&picture, PICTURE_ID, PICTURE_SIZE, PICTURE_SIZE, palette, picture_pixels) ||
        !make_sprite(&label, LABEL_ID, (int)label_width, LABEL_HEIGHT, palette, NULL))
        return;
    set_u32(entry, QUIT_MODE);
    set_u16(entry + 4u, (uint16_t)LABEL_ID);
    set_u16(entry + 6u, (uint16_t)PICTURE_ID);
    set_u32(entry + 8u, HELP_NUMBER);
    ready = true;
#endif
}

/* ---- The carousel ------------------------------------------------------ */

void recomp_menu_quit_build(RecompFunction original)
{
    original();
    quit_position = -1;
    if (!ready) return;
    const uint32_t count = u32(COUNT);
    if (count >= 0xffu) return;
    quit_position = (int)count;
    set_u32(COUNT, count + 1u);
}

/* The reader clamps a position to the last entry, as the game's does. */
static bool is_quit_position(uint32_t position)
{
    if (quit_position < 0) return false;
    const uint32_t last = u32(COUNT) - 1u, p = position & 0xffu;
    return (int)(p > last ? last : p) == quit_position;
}

void recomp_menu_quit_entry(RecompFunction original)
{
    if (is_quit_position(kernel_arg(1u))) {
        kernel_return_caller_cleanup(entry);
        return;
    }
    original();
}

void recomp_menu_quit_mode(RecompFunction original)
{
    if (quit_position >= 0 && (kernel_arg(1u) & 0xffu) == QUIT_MODE) {
        kernel_return_caller_cleanup(entry);
        return;
    }
    original();
}

/* The entry's help number is no index into the game's tables. */
void recomp_menu_quit_help(RecompFunction original)
{
    if (ready && kernel_arg(1u) == HELP_NUMBER) {
        set_u32(HELP_COLOUR, kernel_arg(2u));
        set_u32(HELP_TEXT, help);
        kernel_return_caller_cleanup(0u);
        return;
    }
    original();
}

/* ---- The art ----------------------------------------------------------- */

static bool at_main_menu(void)
{
    return u8(GAME_STATE) == 0u && u8(SCENE) == 0u;
}

/* The menu's words, once loaded: their texture and palette, from the slot
   of the first one's record. Each load may put them somewhere new. */
static bool find_words(Words *w, uint32_t *palette)
{
    const uint32_t slot = u32(SPRITE_RECORDS + 12u * WORDS_ID + 8u);
    const uint32_t texture = u32(SPRITE_SLOTS + 8u * slot);
    *palette = u32(SPRITE_SLOTS + 8u * slot + 4u);
    if (texture == 0u || *palette == 0u) return false;
    const uint32_t format = u32(texture + 12u);
    if (((format >> 8) & 0xffu) != 0x0bu || ((format >> 20) & 0xfu) != 8u || ((format >> 24) & 0xfu) != 8u) return false;
    w->pixels = u32(texture + 4u) & 0x03ffffffu;
    w->colours = u32(*palette + 4u) & 0x03ffffffu;
    recomp_d3d_texture_swizzle_masks(WORDS_SIZE, WORDS_SIZE, &w->mask_x, &w->mask_y);
    return true;
}

/* Cuts the label once the words are loaded, and draws it in their palette. */
static void make_label(void)
{
    Words words;
    uint32_t palette;
    if (!find_words(&words, &palette)) return;
    set_u32(label.entry + 4u, palette);
    if (label_made) return;
    label_made = true;
    static uint8_t pixels[WORDS_SIZE * LABEL_HEIGHT];
    if (!cut_label(&words, label.width, pixels)) {
        fprintf(stderr, "recomp menu quit: the menu's words are not as measured; the label stays clear\n");
        return;
    }
    write_pixels(&label, pixels);
    fprintf(stderr, "recomp menu quit: label cut from the menu's words\n");
}

void recomp_menu_quit_draw(RecompFunction original)
{
    Sprite *sprite = NULL;
    uint32_t saved = 0u;
    if (ready && at_main_menu()) {
        const uint32_t id = u32(kernel_arg(1u));
        sprite = id == PICTURE_ID ? &picture : id == LABEL_ID ? &label : NULL;
        if (sprite == &label) make_label();
    }
    if (sprite != NULL) {
        saved = u32(sprite->record + 8u);
        set_u32(sprite->record + 8u, sprite->slot);
    }
    original();
    if (sprite != NULL) set_u32(sprite->record + 8u, saved);
}

/* ---- The confirm ------------------------------------------------------- */

/* The update's own test for a confirm on the entry under the cursor: settled,
   on Quit Game, and a pad that pressed A or Start without a d-pad edge. */
static bool confirmed(void)
{
    if (u16(MENU_STATE + 0xfau) != 0u || u16(MENU_STATE + 0xfcu) != 1u || u8(MENU_STATE + 0x224u) != 0u) return false;
    if ((int8_t)u8(MENU_STATE + 0x1dcu) != quit_position) return false;
    for (unsigned pad = 0u; pad < 4u; ++pad)
        if ((u16(PAD_EDGES + 8u * pad) & CONFIRM_BUTTONS) != 0u && (u16(PAD_EDGES + 2u + 8u * pad) & LEFT_RIGHT) == 0u)
            return true;
    return false;
}

void recomp_menu_quit_update(RecompFunction original)
{
    if (ready && quit_position >= 0 && !recomp_quit_prompt_open() && confirmed()) {
        const uint32_t eax = recomp_runtime.registers.eax, arguments[2] = {SOUND_CONFIRM, 0u};
        kernel_call_guest(PLAY_SOUND, arguments, 2u);
        recomp_quit_prompt_request();
        fprintf(stderr, "recomp menu quit: asking\n");
        kernel_return_caller_cleanup(eax);
        return;
    }
    original();
}
