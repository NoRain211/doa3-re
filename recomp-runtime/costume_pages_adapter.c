/* Hand-written save-costume scan, select menu and model loading: up to 64
   save costumes per character, shown 9 rows to a page, Left/Right flipping
   pages (docs/doa3-costumes.md). The game's own row, pick and lookup code
   stays lifted; it reads the per-character counts this scan writes.
   USA 3.0 passes arguments on the stack; Japan 3.1, built by a newer
   compiler, passes most in registers, so each has its own entry code. */
#include "costume_pages_adapter.h"
#include "costume_pages.h"
#include "kernel_abi.h"

#include <string.h>

static CostumeTables tables;

typedef struct {
    uint32_t ebx, esi, edi, ebp;
} SavedRegisters;

/* Guest helpers clobber registers the caller expects kept. */
static SavedRegisters save_registers(void)
{
    return (SavedRegisters){recomp_runtime.registers.ebx, recomp_runtime.registers.esi,
                            recomp_runtime.registers.edi, recomp_runtime.registers.ebp};
}

static void restore_registers(SavedRegisters saved)
{
    recomp_runtime.registers.ebx = saved.ebx;
    recomp_runtime.registers.esi = saved.esi;
    recomp_runtime.registers.edi = saved.edi;
    recomp_runtime.registers.ebp = saved.ebp;
}

static uint32_t call(uint32_t address, const uint32_t *args, unsigned count)
{
    kernel_call_guest(address, args, count);
    return recomp_runtime.registers.eax;
}

static uint8_t u8(uint32_t address) { return *recomp_memory(address, 1); }
static void set_u8(uint32_t address, uint8_t value) { *recomp_memory(address, 1) = value; }
static uint16_t u16(uint32_t address) { return *recomp_memory_u16(address); }
static void set_u16(uint32_t address, uint16_t value) { *recomp_memory_u16(address) = value; }
static uint32_t u32(uint32_t address) { return *recomp_memory_u32(address); }
static void set_u32(uint32_t address, uint32_t value) { *recomp_memory_u32(address) = value; }

static float guest_float(uint32_t address)
{
    float value;
    memcpy(&value, recomp_memory(address, sizeof value), sizeof value);
    return value;
}

static void guest_string(uint32_t address, char *out, size_t size)
{
    size_t i = 0;
    for (; i + 1 < size; ++i) {
        char c = (char)u8(address + i);
        if (c == '\0') break;
        out[i] = c;
    }
    out[i] = '\0';
}

#ifdef RECOMP_DOA3_JP_BINDINGS
enum {
    /* The scan (0x86ad0) and its tables. */
    PREFIXES = 0x00302c58, SCAN_FORMAT = 0x00235580, SLOT_TABLE = 0x005b1e58,
    NAME_TABLE = 0x005b2140, NAME_COUNT = 0x005b212c, COUNTS = 0x005b3174,
    SPRINTF = 0x001b7150, FIND_FIRST = 0x0018b090, FIND_NEXT = 0x0018b19d,
    FIND_CLOSE = 0x0018b726,
    /* The menu total (0xcd7c0). */
    SELECT_TO_PREFIX = 0x002ff868, DISC_ROWS = 0x0029c70c, UNLOCKED = 0x0059fb16,
    /* costume_model_load (0x86060). */
    DISC_COSTUMES = 0x00301640, MODEL_BUFFERS = 0x005a0cf0, LOADER = 0x005b2130,
    PENDING = 0x00598b7c, MODEL_REGISTER = 0x00086f90, MODEL_RELOCATE = 0x00049560,
    /* The costume list draw (0xd18a0). */
    CURSOR_TO_SELECT = 0x009b76b0, SELECT_TABLE = 0x0029ca60,
    DRAW_BEGIN = 0x0006e2a0, PLAYER_SIDE = 0x000ccef0, PULSE = 0x00054c70,
    SPRITE_PLACE = 0x0006e3d0, SPRITE_BOX = 0x0006e5a0, SPRITE_DRAW = 0x0006df00,
    ORIGIN_X = 0x005b12e8, ORIGIN_Y = 0x005b12ea, LAYER = 0x005b12f0,
    HIGHLIGHT_A = 0x0029bc1c, HIGHLIGHT_B = 0x0029bc30,
    PLAYER_COLOR_A = 0x00304804, PLAYER_COLOR_B = 0x003047f4, WIDESCREEN = 0x00c59866,
    COLUMN_X_WIDE = 0x0029bc38, COLUMN_X = 0x0029bc34,
    JITTER_PLACE = 0x0029bc28, JITTER_SIZE = 0x0029bc2c, JITTER = 0x00c59104, ZERO = 0x00250ef8,
    LABEL = 0x0029bc04, DIGIT = 0x0029bc0c, BOX = 0x0029bc14, DRAW_COLOR = 0x00303cc4,
    /* The costume list input handler (0xcb760). */
    PRESSED = 0x009b7c10, HELD = 0x009b7c0e, MODE = 0x00596a33, QUICK = 0x00c59867,
    CYCLE = 0x000ccb80, VARIANT = 0x000ccd90, SOUND = 0x000a5610, PREVIEW = 0x000cd5a0,
    PREVIEW_STATE = 0x009b792d, ROW_TO_ID = 0x000cd110, CONFIRM = 0x000cc760,
};
#else
enum {
    /* The scan (0x80a00) and its tables. */
    PREFIXES = 0x00309a98, SCAN_FORMAT = 0x001fd854, SLOT_TABLE = 0x004a0df0,
    NAME_TABLE = 0x004a10d8, NAME_COUNT = 0x004a10c4, COUNTS = 0x004a210c,
    SPRINTF = 0x0018dad6, FIND_FIRST = 0x001633b7, FIND_NEXT = 0x001634c4,
    FIND_CLOSE = 0x00162caa,
    /* The menu total (0xc8140). */
    SELECT_TO_PREFIX = 0x00309cb8, DISC_ROWS = 0x0036806c, UNLOCKED = 0x00487806,
    /* costume_model_load (0x7e9c0). */
    DISC_COSTUMES = 0x00308770, MODEL_BUFFERS = 0x004889b8, LOADER = 0x004a10c8,
    PENDING = 0x0048086c, MODEL_REGISTER = 0x000810a0, MODEL_RELOCATE = 0x00046b40,
    /* The costume list draw (0xca540). */
    SELECT_OF_CURSOR = 0x000c7be0, DRAW_BEGIN = 0x000694e0, DRAW_ORIGIN_LAYER = 0x00069320,
    PLAYER_SIDE = 0x000c5050, PLAYER_COLOR_A = 0x00051930, PLAYER_COLOR_B = 0x00051920,
    ORIGIN = 0x00069300, SPRITE_PLACE = 0x0006d560, SPRITE_BOX = 0x0006d6b0,
    SPRITE_COLOR = 0x000692f0, SPRITE_DRAW = 0x0006d140,
    HIGHLIGHT_A = 0x00368778, HIGHLIGHT_B = 0x0036878c, WIDESCREEN = 0x0048e612,
    COLUMN_X_WIDE = 0x00368794, COLUMN_X = 0x00368790,
    PULSE_SCALE = 0x001ed720, PULSE_ADD = 0x001ed704, ALPHA_SCALE = 0x001ed728,
    ALPHA_ADD = 0x001ed724, ALPHA_MAX = 0x001ed514,
    JITTER_PLACE = 0x00368784, JITTER_SIZE = 0x00368788, JITTER = 0x008610a0, ZERO = 0x001ed528,
    LABEL = 0x00368760, DIGIT = 0x00368768, BOX = 0x00368770,
    /* The costume list input handler (0xc7830). */
    PRESSED = 0x0086132c, HELD = 0x0086132a, MODE = 0x0047e723, CONFIRM_FRAMES = 0x002fd55c,
    CYCLE = 0x000c6780, VARIANT = 0x000c6380, SOUND = 0x0009fd00, PREVIEW = 0x000c64c0,
    PREVIEW_STATE = 0x0086100d, ROW_TO_ID = 0x000c5350, CONFIRM = 0x000c7200,
};
#endif

/* Name table slots the model loader lends a costume's two files, two per
   player, so afs_load opens them by name as it opens any save costume. */
enum { LOANED_NAMES = 0xf8 };

/* The scan (USA 0x80a00, Japan 0x86ad0; no arguments). */
static void costume_scan(void)
{
    SavedRegisters saved = save_registers();
    uint32_t sp = recomp_runtime.registers.esp;
    uint32_t pattern = sp - 0x40, find_data = sp - 0x200;

    for (uint32_t i = 0; i < 0x85; ++i) set_u32(SLOT_TABLE + 4 * i, 0);
    for (uint32_t i = 0; i < COSTUME_PREFIXES; ++i) set_u8(COUNTS + i, 0);
    set_u32(NAME_TABLE, 0);
    set_u16(NAME_COUNT, 0);
    costume_tables_reset(&tables);

    recomp_runtime.registers.esp = sp - 0x240;
    for (unsigned prefix = 0; prefix < COSTUME_PREFIXES; ++prefix) {
        if (prefix == 12 || prefix == 16) continue;
        call(SPRINTF, (const uint32_t[]){pattern, SCAN_FORMAT, u32(PREFIXES + 4 * prefix)}, 3);
        uint32_t handle = call(FIND_FIRST, (const uint32_t[]){pattern, find_data}, 2);
        if (handle == 0xffffffffu) continue;
        do {
            char name[COSTUME_NAME_SIZE * 2];
            guest_string(find_data + 0x2c, name, sizeof name);
            for (char *c = name; *c; ++c)
                if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
            costume_tables_add(&tables, prefix, name);
        } while (call(FIND_NEXT, (const uint32_t[]){handle, find_data}, 2) != 0);
        call(FIND_CLOSE, &handle, 1);
        set_u8(COUNTS + prefix, (uint8_t)costume_tables_count(&tables, prefix));
    }
    recomp_runtime.registers.esp = sp;
    restore_registers(saved);
    kernel_return_caller_cleanup(0);
}

/* Disc rows, capped by those unlocked, then every save costume: the game's
   total without its cap of 9. */
static unsigned menu_rows(unsigned select)
{
    if (select > 0x18) return 0;
    unsigned disc = u8(DISC_ROWS + select);
    unsigned prefix = u8(SELECT_TO_PREFIX + select);
    unsigned unlocked = u8(UNLOCKED + prefix);
    unsigned rows = (unlocked < disc ? unlocked : disc) + u8(COUNTS + prefix);
    return rows > 127 ? 127 : rows;
}

static void loan_name(unsigned slot, const char *name)
{
    for (unsigned i = 0; i < COSTUME_NAME_SIZE; ++i)
        set_u8(NAME_TABLE + slot * COSTUME_NAME_SIZE + i, (uint8_t)name[i]);
}

static void decrement_pending(void)
{
    uint16_t *pending = recomp_memory_u16(PENDING);
    if (*pending != 0) --*pending;
}

/* The two file IDs costume_model_load hands the loader: a disc archive entry,
   or a save costume's names lent to the name table. */
static void costume_files(unsigned select, unsigned id, uint32_t player, uint32_t *xpr, uint32_t *cat)
{
    *xpr = *cat = 0;
    if (id < 0x40) {
        uint32_t entry = DISC_COSTUMES + (id * 25 + select) * 10;
        *xpr = u16(entry + 2);
        *cat = u16(entry);
        return;
    }
    const char *xpr_name, *cat_name;
    unsigned slot = LOANED_NAMES + (player & 3) * 2;
    if (costume_tables_get(&tables, u8(SELECT_TO_PREFIX + select), id - 0x40, &xpr_name, &cat_name)) {
        loan_name(slot, xpr_name);
        loan_name(slot + 1, cat_name);
        *xpr = 0x8000u | slot;
        *cat = 0x8000u | (slot + 1);
    }
}

/* A player's costume list state keeps the active selection in +0x16 and each
   selection's row in [+0x16]. */
static uint32_t selected_row_address(uint32_t player)
{
    return player + (uint32_t)(int32_t)(int8_t)u8(player + 0x16);
}

/* Moves the 12-bit x of the highlight's packed position and widens it by the
   menu's jitter, or undoes it (sign -1), as the original does around the
   highlight box. The jitter float is converted with _ftol, which truncates. */
static void jitter_highlight(int sign)
{
    float jitter = guest_float(JITTER);
    float width = jitter + jitter;
    if (width <= guest_float(ZERO)) width = -width;
    uint32_t place = u32(JITTER_PLACE);
    int32_t x = (int32_t)(place << 20) >> 20;
    x += sign * (int32_t)jitter;
    set_u32(JITTER_PLACE, place ^ ((place ^ (uint32_t)x) & 0xfffu));
    set_u16(JITTER_SIZE, (uint16_t)(u16(JITTER_SIZE) + sign * (int32_t)width));
}

/* Two-digit labels (COS.10 on) squeeze into the box the one digit had. */
enum { TENS_SHIFT = 1, DIGIT_ADVANCE = 7 };

#ifdef RECOMP_DOA3_JP_BINDINGS
/* The menu total (0xcd7c0): select in AL, rows in AL. */
static void costume_menu_total(void)
{
    uint32_t eax = recomp_runtime.registers.eax;
    kernel_return_caller_cleanup((eax & ~0xffu) | menu_rows(eax & 0xffu));
}

/* costume_model_load (0x86060): id in AL, select in CL, player on the stack. */
static void costume_model_load(void)
{
    SavedRegisters saved = save_registers();
    uint32_t player = kernel_arg(1) & 0xffu;
    uint32_t buffers = MODEL_BUFFERS + player * 0x1c, xpr, cat;
    costume_files(recomp_runtime.registers.ecx & 0xffu, recomp_runtime.registers.eax & 0xffu,
                  player, &xpr, &cat);
    call(u32(LOADER), (const uint32_t[]){xpr, u32(buffers)}, 2);
    decrement_pending();
    recomp_runtime.registers.eax = u32(buffers);
    call(MODEL_REGISTER, (const uint32_t[]){player, 0}, 2);
    decrement_pending();
    call(u32(LOADER), (const uint32_t[]){cat, u32(buffers + 8)}, 2);
    recomp_runtime.registers.eax = player;
    call(MODEL_RELOCATE, NULL, 0);
    decrement_pending();
    restore_registers(saved);
    kernel_return_caller_cleanup(recomp_runtime.registers.eax);
}

/* A player's cursor, through two tables, to a select index. */
static unsigned player_select(uint32_t player)
{
    return u8(SELECT_TABLE + 10u * u8(CURSOR_TO_SELECT + u8(player + 0x16)));
}

static void draw_sprite(uint32_t record)
{
    recomp_runtime.registers.eax = record;
    call(SPRITE_DRAW, (const uint32_t[]){0}, 1);
}

static void draw_label_number(unsigned number, int16_t x)
{
    uint32_t base = u32(DIGIT);
    if (number < 10) {
        set_u32(DIGIT, base + number);
        draw_sprite(DIGIT);
    } else {
        set_u16(ORIGIN_X, (uint16_t)(x - TENS_SHIFT));
        set_u32(DIGIT, base + number / 10);
        draw_sprite(DIGIT);
        set_u16(ORIGIN_X, (uint16_t)(x - TENS_SHIFT + DIGIT_ADVANCE));
        set_u32(DIGIT, base + number % 10);
        draw_sprite(DIGIT);
    }
    set_u32(DIGIT, base);
}

/* The costume list draw (0xd18a0): context in EDX, two stack arguments the
   caller removes. It draws the page holding each player's selected row. */
static void costume_menu_draw(void)
{
    SavedRegisters saved = save_registers();
    uint32_t context = recomp_runtime.registers.edx;

    call(DRAW_BEGIN, NULL, 0);
    set_u16(ORIGIN_X, 0);
    set_u16(ORIGIN_Y, 0);
    set_u8(LAYER, 8);
    for (uint32_t side = 0; side < 4; ++side) {
        uint32_t player = context + 0x10 + side * 0x38;
        if (u8(player + 0x13) == 0 || u32(player + 0x34) != 1) continue;
        recomp_runtime.registers.eax = player;
        unsigned column = call(PLAYER_SIDE, NULL, 0) & 0xffu;
        unsigned rows = menu_rows(player_select(player));
        unsigned selected = u8(selected_row_address(player)), first, count;
        costume_page_rows(rows, selected, &first, &count);
        for (unsigned row = first; row < first + count; ++row) {
            uint32_t color;
            if (row == selected) {
                recomp_runtime.registers.eax = 0xff;
                uint32_t pulse = call(PULSE, (const uint32_t[]){0x3c, u32(player + 0x28), 0x3c}, 3) & 0xffu;
                set_u32(HIGHLIGHT_A, u32(PLAYER_COLOR_A + side * 4));
                set_u32(HIGHLIGHT_B, (u32(PLAYER_COLOR_B + side * 4) & 0xffffffu) | pulse << 24);
                color = 0xffffffffu;
            } else {
                set_u32(HIGHLIGHT_A, 0x80333333u);
                set_u32(HIGHLIGHT_B, 0x80737373u);
                color = 0xff333333u;
            }
            int16_t x = (int16_t)(u8(WIDESCREEN) ? u16(COLUMN_X_WIDE + (column % 4) * 2)
                                                 : u16(COLUMN_X + (column % 2) * 2));
            set_u16(ORIGIN_X, (uint16_t)x);
            set_u16(ORIGIN_Y, (uint16_t)((row - first) * 0x14));
            jitter_highlight(1);
            recomp_runtime.registers.esi = JITTER_PLACE;
            call(SPRITE_PLACE, NULL, 0);
            recomp_runtime.registers.esi = BOX;
            call(SPRITE_BOX, NULL, 0);
            jitter_highlight(-1);
            set_u32(DRAW_COLOR, color);
            draw_sprite(LABEL);
            draw_label_number(row + 1, x);
            set_u32(DRAW_COLOR, 0xffffffffu);
        }
    }
    set_u16(ORIGIN_X, 0);
    set_u16(ORIGIN_Y, 0);
    restore_registers(saved);
    kernel_return_caller_cleanup(0);
}

static uint32_t cycle_row(uint32_t context, uint32_t player, int step)
{
    recomp_runtime.registers.eax = player;
    return call(CYCLE, (const uint32_t[]){context, (uint32_t)step}, 2);
}

/* After the row changes: reset the preview, play the move sound, show the
   new costume. */
static void row_changed(uint32_t player)
{
    recomp_runtime.registers.eax = player;
    unsigned side = call(PLAYER_SIDE, NULL, 0) & 0xffu;
    set_u8(PREVIEW_STATE + side * 0x20, 0);
    recomp_runtime.registers.esi = 0;
    recomp_runtime.registers.eax = 0x23;
    call(SOUND, NULL, 0);
    recomp_runtime.registers.edx &= ~0xffu;
    recomp_runtime.registers.esi = player;
    call(PREVIEW, NULL, 0);
    set_u32(player + 0x28, 0);
}

static void play_sound(uint32_t sound)
{
    recomp_runtime.registers.esi = 0;
    recomp_runtime.registers.eax = sound;
    call(SOUND, NULL, 0);
}

static int pick_variant(uint32_t player, uint32_t variant)
{
    recomp_runtime.registers.ecx = (recomp_runtime.registers.ecx & ~0xffu) | u8(player + 0x1a);
    recomp_runtime.registers.edi = variant;
    return call(VARIANT, (const uint32_t[]){u8(player + 0x16)}, 1) != 0;
}

static uint32_t confirm_delay(void)
{
    return 5u - (u8(QUICK) != 0);
}

static void confirm_costume(uint32_t context, uint32_t player, uint32_t pad)
{
    recomp_runtime.registers.eax = u8(selected_row_address(player));
    recomp_runtime.registers.edx = (recomp_runtime.registers.edx & ~0xffu) | u8(player + 0x14);
    recomp_runtime.registers.ecx = pad;
    uint32_t id = call(ROW_TO_ID, NULL, 0);
    recomp_runtime.registers.eax = (recomp_runtime.registers.eax & ~0xffu) | (pad & 0xffu);
    call(CONFIRM, (const uint32_t[]){context, id}, 2);
}
#else
/* The menu total (0xc8140): cdecl (select), rows in AL. */
static void costume_menu_total(void)
{
    uint32_t eax = recomp_runtime.registers.eax;
    kernel_return_caller_cleanup((eax & ~0xffu) | menu_rows(kernel_arg(1) & 0xffu));
}

/* costume_model_load (0x7e9c0): cdecl (select, id, player). */
static void costume_model_load(void)
{
    SavedRegisters saved = save_registers();
    uint32_t player = kernel_arg(3) & 0xffu;
    uint32_t buffers = MODEL_BUFFERS + player * 0x1c, xpr, cat;
    costume_files(kernel_arg(1) & 0xffu, kernel_arg(2) & 0xffu, player, &xpr, &cat);
    call(u32(LOADER), (const uint32_t[]){xpr, u32(buffers)}, 2);
    decrement_pending();
    call(MODEL_REGISTER, (const uint32_t[]){player, u32(buffers)}, 2);
    decrement_pending();
    call(u32(LOADER), (const uint32_t[]){cat, u32(buffers + 8)}, 2);
    call(MODEL_RELOCATE, &player, 1);
    decrement_pending();
    restore_registers(saved);
    kernel_return_caller_cleanup(recomp_runtime.registers.eax);
}

static unsigned player_select(uint32_t player)
{
    return call(SELECT_OF_CURSOR, (const uint32_t[]){u8(player + 0x16)}, 1) & 0xffu;
}

static void draw_sprite(uint32_t record)
{
    call(SPRITE_DRAW, (const uint32_t[]){record, 0}, 2);
}

static void draw_label_number(unsigned number, int16_t x, uint32_t y)
{
    uint32_t base = u32(DIGIT);
    if (number < 10) {
        set_u32(DIGIT, base + number);
        draw_sprite(DIGIT);
    } else {
        call(ORIGIN, (const uint32_t[]){(uint32_t)(int32_t)(x - TENS_SHIFT), y}, 2);
        set_u32(DIGIT, base + number / 10);
        draw_sprite(DIGIT);
        call(ORIGIN, (const uint32_t[]){(uint32_t)(int32_t)(x - TENS_SHIFT + DIGIT_ADVANCE), y}, 2);
        set_u32(DIGIT, base + number % 10);
        draw_sprite(DIGIT);
    }
    set_u32(DIGIT, base);
}

/* The highlight's alpha: a triangle wave over the player's timer, scaled as
   the original's x87 code does, truncated by _ftol. */
static uint32_t highlight_alpha(uint32_t player)
{
    int32_t phase = (int32_t)u32(player + 0x28) % 0x78;
    int32_t wave = phase - 0x3c > 0 ? phase - 0x3c : 0x3c - phase;
    double alpha = ((wave * (double)guest_float(PULSE_SCALE) + guest_float(PULSE_ADD)) *
                    guest_float(ALPHA_SCALE) + guest_float(ALPHA_ADD)) * guest_float(ALPHA_MAX);
    return (uint32_t)(int32_t)alpha & 0xffu;
}

/* The costume list draw (0xca540): cdecl (context, ...). It draws the page
   holding each player's selected row. */
static void costume_menu_draw(void)
{
    SavedRegisters saved = save_registers();
    uint32_t context = kernel_arg(1);

    call(DRAW_BEGIN, NULL, 0);
    call(DRAW_ORIGIN_LAYER, (const uint32_t[]){0, 0, 8}, 3);
    for (uint32_t side = 0; side < 4; ++side) {
        uint32_t player = context + 0x10 + side * 0x38;
        if (u8(player + 0x13) == 0 || u32(player + 0x34) != 1) continue;
        unsigned column = call(PLAYER_SIDE, &player, 1) & 0xffu;
        unsigned rows = menu_rows(player_select(player));
        unsigned selected = u8(selected_row_address(player)), first, count;
        costume_page_rows(rows, selected, &first, &count);
        for (unsigned row = first; row < first + count; ++row) {
            uint32_t color;
            if (row == selected) {
                set_u32(HIGHLIGHT_A, call(PLAYER_COLOR_A, &side, 1));
                uint32_t tint = call(PLAYER_COLOR_B, &side, 1) & 0xffffffu;
                set_u32(HIGHLIGHT_B, tint | highlight_alpha(player) << 24);
                color = 0xffffffffu;
            } else {
                set_u32(HIGHLIGHT_A, 0x80333333u);
                set_u32(HIGHLIGHT_B, 0x80737373u);
                color = 0xff333333u;
            }
            int16_t x = (int16_t)(u8(WIDESCREEN) ? u16(COLUMN_X_WIDE + (column % 4) * 2)
                                                 : u16(COLUMN_X + (column % 2) * 2));
            uint32_t y = (row - first) * 0x14;
            call(ORIGIN, (const uint32_t[]){(uint32_t)(int32_t)x, y}, 2);
            jitter_highlight(1);
            call(SPRITE_PLACE, (const uint32_t[]){JITTER_PLACE}, 1);
            call(SPRITE_BOX, (const uint32_t[]){BOX}, 1);
            jitter_highlight(-1);
            call(SPRITE_COLOR, &color, 1);
            draw_sprite(LABEL);
            draw_label_number(row + 1, x, y);
            call(SPRITE_COLOR, (const uint32_t[]){0xffffffffu}, 1);
        }
    }
    call(ORIGIN, (const uint32_t[]){0, 0}, 2);
    restore_registers(saved);
    kernel_return_caller_cleanup(0);
}

static uint32_t cycle_row(uint32_t context, uint32_t player, int step)
{
    return call(CYCLE, (const uint32_t[]){context, player, (uint32_t)step}, 3);
}

static void play_sound(uint32_t sound)
{
    call(SOUND, (const uint32_t[]){sound, 0}, 2);
}

/* After the row changes: reset the preview, play the move sound, show the
   new costume. */
static void row_changed(uint32_t player)
{
    unsigned side = call(PLAYER_SIDE, &player, 1) & 0xffu;
    set_u8(PREVIEW_STATE + side * 0x20, 0);
    play_sound(0x23);
    recomp_runtime.registers.ecx = player;
    call(PREVIEW, (const uint32_t[]){0}, 1);
    set_u32(player + 0x28, 0);
}

static int pick_variant(uint32_t player, uint32_t variant)
{
    recomp_runtime.registers.edi = variant;
    return call(VARIANT, (const uint32_t[]){u8(player + 0x1a), u8(player + 0x16)}, 2) != 0;
}

/* (frames + 6) * 5 / 60, from the game's confirm timing. */
static uint32_t confirm_delay(void)
{
    return (uint32_t)(((int32_t)u8(CONFIRM_FRAMES) + 6) * 5 / 60);
}

static void confirm_costume(uint32_t context, uint32_t player, uint32_t pad)
{
    recomp_runtime.registers.ecx = (recomp_runtime.registers.ecx & ~0xffu) | u8(selected_row_address(player));
    recomp_runtime.registers.eax = (recomp_runtime.registers.eax & ~0xffu) | u8(player + 0x14);
    recomp_runtime.registers.edx = pad;
    uint32_t id = call(ROW_TO_ID, NULL, 0) & 0xffu;
    recomp_runtime.registers.eax = (recomp_runtime.registers.eax & ~0xffu) | (pad & 0xffu);
    call(CONFIRM, (const uint32_t[]){context, id}, 2);
}
#endif

/* Left/Right: the same place on the previous or next page. The row is set one
   short of it and the game's own cycle steps onto it, so its locked-row skip
   and its bookkeeping run as for Up/Down. */
static void flip_page(uint32_t context, uint32_t player, int step)
{
    unsigned select = player_select(player);
    if (select == 0xff) return;
    unsigned rows = menu_rows(select);
    uint32_t row_address = selected_row_address(player);
    unsigned selected = u8(row_address);
    unsigned target = costume_page_flip(rows, selected, step);
    if (target == selected) return;
    set_u8(row_address, (uint8_t)((target + rows - 1) % rows));
    if (cycle_row(context, player, 1) == 1) row_changed(player);
}

/* The costume list input handler (USA 0xc7830, Japan 0xcb760): (context,
   player) on the stack, returns 1. Up/Down, confirm and back as the original;
   Left/Right, which the list ignored, flip pages. */
static void costume_menu_input(void)
{
    SavedRegisters saved = save_registers();
    uint32_t context = kernel_arg(1), player = kernel_arg(2);
    uint32_t pad = (uint32_t)(int32_t)(int8_t)u8(player + 0x12);
    uint16_t pressed = u16(PRESSED + pad * 8);
    int step = (pressed & 2) ? 1 : (pressed & 1) ? -1 : 0;
    uint32_t sp = recomp_runtime.registers.esp;
    uint32_t variant = sp - 0x10;

    recomp_runtime.registers.esp = sp - 0x20;
    if (step != 0) {
        if (u8(MODE) == 10 && pick_variant(player, variant))
            set_u8(selected_row_address(player), u8(variant));
        else if (cycle_row(context, player, step) == 1)
            row_changed(player);
    } else if (pressed & 0xc) {
        flip_page(context, player, (pressed & 8) ? 1 : -1);
    }
    uint16_t held = u16(HELD + pad * 8);
    if (held & 0x220) {
        set_u32(player + 0x34, 0);
        set_u32(player + 0x20, confirm_delay());
        play_sound(0x29);
    }
    if (held & 0xd10) {
        play_sound(0x24);
        confirm_costume(context, player, pad);
        set_u32(player + 0x20, confirm_delay());
    }
    recomp_runtime.registers.esp = sp;
    restore_registers(saved);
    kernel_return_caller_cleanup(1);
}

/* Keyed by USA address; the Japan build maps its entry points to these. */
RecompFunction recomp_costume_pages_lookup(uint32_t address)
{
    switch (address) {
    case 0x00080a00u: return costume_scan;
    case 0x000c8140u: return costume_menu_total;
    case 0x0007e9c0u: return costume_model_load;
    case 0x000ca540u: return costume_menu_draw;
    case 0x000c7830u: return costume_menu_input;
    default: return NULL;
    }
}
