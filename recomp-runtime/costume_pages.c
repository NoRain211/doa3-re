#include "costume_pages.h"

#include <string.h>

void costume_tables_reset(CostumeTables *tables)
{
    memset(tables, 0, sizeof *tables);
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void costume_tables_add(CostumeTables *tables, unsigned prefix, const char *name)
{
    if (prefix >= COSTUME_PREFIXES || strlen(name) < 5) return;
    int high = hex_digit(name[3]), low = hex_digit(name[4]);
    /* The game reads the key with "%2X"; a name without one has no slot. */
    if (high < 0 || low < 0) return;
    const char *dot = strchr(name, '.');
    bool xpr = dot != NULL && strcmp(dot + 1, "xpr") == 0;
    bool cat = dot != NULL && strcmp(dot + 1, "cat") == 0;
    if (!xpr && !cat) return;

    uint8_t key = (uint8_t)(high << 4 | low);
    CostumeSlot *slots = tables->slots[prefix];
    unsigned *count = &tables->slot_count[prefix];
    unsigned i = 0;
    while (i < *count && slots[i].key != key) ++i;
    if (i == *count) {
        if (*count == COSTUME_MAX) return;
        slots[i] = (CostumeSlot){key, -1, -1};
        ++*count;
    }
    if (tables->name_count == COSTUME_NAMES) return;
    unsigned index = tables->name_count++;
    strncpy(tables->names[index], name, COSTUME_NAME_SIZE - 1);
    tables->names[index][COSTUME_NAME_SIZE - 1] = '\0';
    if (xpr) slots[i].xpr = (int16_t)index; else slots[i].cat = (int16_t)index;
}

unsigned costume_tables_count(const CostumeTables *tables, unsigned prefix)
{
    if (prefix >= COSTUME_PREFIXES) return 0;
    unsigned complete = 0;
    for (unsigned i = 0; i < tables->slot_count[prefix]; ++i) {
        const CostumeSlot *slot = &tables->slots[prefix][i];
        if (slot->xpr >= 0 && slot->cat >= 0) ++complete;
    }
    return complete;
}

bool costume_tables_get(const CostumeTables *tables, unsigned prefix, unsigned index,
    const char **xpr, const char **cat)
{
    if (prefix >= COSTUME_PREFIXES) return false;
    for (unsigned i = 0; i < tables->slot_count[prefix]; ++i) {
        const CostumeSlot *slot = &tables->slots[prefix][i];
        if (slot->xpr < 0 || slot->cat < 0) continue;
        if (index-- == 0) {
            *xpr = tables->names[slot->xpr];
            *cat = tables->names[slot->cat];
            return true;
        }
    }
    return false;
}

void costume_page_rows(unsigned rows, unsigned selected, unsigned *first, unsigned *count)
{
    if (selected >= rows) selected = 0;
    *first = selected / COSTUME_PAGE_ROWS * COSTUME_PAGE_ROWS;
    *count = rows - *first < COSTUME_PAGE_ROWS ? rows - *first : COSTUME_PAGE_ROWS;
}

unsigned costume_page_flip(unsigned rows, unsigned selected, int step)
{
    unsigned pages = (rows + COSTUME_PAGE_ROWS - 1) / COSTUME_PAGE_ROWS;
    if (pages < 2 || selected >= rows) return selected;
    unsigned page = selected / COSTUME_PAGE_ROWS;
    page = (page + pages + (step < 0 ? pages - 1 : 1)) % pages;
    unsigned target = page * COSTUME_PAGE_ROWS + selected % COSTUME_PAGE_ROWS;
    return target < rows ? target : rows - 1;
}
