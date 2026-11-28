#ifndef DOA3_COSTUME_PAGES_H
#define DOA3_COSTUME_PAGES_H

#include <stdbool.h>
#include <stdint.h>

/* The game's save-folder costumes, held by the runtime instead of the game's
   7-per-character table, and the select menu's pages of 9 rows
   (docs/doa3-costumes.md). */

enum {
    COSTUME_PREFIXES = 19,   /* the scan's character prefixes */
    COSTUME_MAX = 64,        /* IDs 0x40-0x7f: the menu's signed-byte maths */
    COSTUME_NAMES = 2048,
    COSTUME_NAME_SIZE = 16,  /* the game's name table entries */
    COSTUME_PAGE_ROWS = 9,   /* the rows the portrait panel holds */
};

typedef struct {
    uint8_t key;             /* the two hex digits after the prefix */
    int16_t xpr, cat;        /* name indices, -1 until found */
} CostumeSlot;

typedef struct {
    char names[COSTUME_NAMES][COSTUME_NAME_SIZE];
    unsigned name_count;
    CostumeSlot slots[COSTUME_PREFIXES][COSTUME_MAX];
    unsigned slot_count[COSTUME_PREFIXES];
} CostumeTables;

void costume_tables_reset(CostumeTables *tables);
/* Adds a file the scan found for a prefix, as the game does: the first
   COSTUME_MAX distinct keys in folder order get a slot, later ones are
   dropped, and a key needs both its .xpr and .cat to count. */
void costume_tables_add(CostumeTables *tables, unsigned prefix, const char *name);
/* The costumes with both files, in slot order. */
unsigned costume_tables_count(const CostumeTables *tables, unsigned prefix);
/* The index'th complete costume's file names; false past the count. */
bool costume_tables_get(const CostumeTables *tables, unsigned prefix, unsigned index,
    const char **xpr, const char **cat);

/* The rows a menu of `rows` shows around the selected row: first and the
   count, at most COSTUME_PAGE_ROWS. */
void costume_page_rows(unsigned rows, unsigned selected, unsigned *first, unsigned *count);
/* The row Left (step -1) or Right (+1) moves the selection to: the same
   place on the previous or next page, wrapping, and the last row if that
   page is shorter. A menu of one page keeps its row. */
unsigned costume_page_flip(unsigned rows, unsigned selected, int step);

#endif
