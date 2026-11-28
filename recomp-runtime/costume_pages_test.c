#include "costume_pages.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

static CostumeTables tables;

int main(void)
{
    costume_tables_reset(&tables);
    /* Folder order: the pack's kas17 before the Booster Disc's kasa0. */
    costume_tables_add(&tables, 5, "kas17.cat");
    costume_tables_add(&tables, 5, "kas17.xpr");
    costume_tables_add(&tables, 5, "kas18.xpr");      /* no .cat: not counted */
    costume_tables_add(&tables, 5, "kasa0.cat");
    costume_tables_add(&tables, 5, "kasa0.xpr");
    costume_tables_add(&tables, 5, "kasxx.xpr");      /* no hex key */
    costume_tables_add(&tables, 5, "kas19.jpg");      /* neither file */
    check(costume_tables_count(&tables, 5) == 2, "complete costumes count");
    const char *xpr, *cat;
    check(costume_tables_get(&tables, 5, 1, &xpr, &cat) &&
          strcmp(xpr, "kasa0.xpr") == 0 && strcmp(cat, "kasa0.cat") == 0,
          "the second complete costume skips the incomplete kas18");
    check(!costume_tables_get(&tables, 5, 2, &xpr, &cat), "nothing past the count");
    check(costume_tables_count(&tables, 0) == 0, "another prefix is empty");

    /* 64 keys get slots and the 65th is dropped, as the game drops the 8th. */
    costume_tables_reset(&tables);
    char name[16];
    for (unsigned key = 0; key < 70; ++key) {
        snprintf(name, sizeof name, "aya%02X.xpr", key);
        costume_tables_add(&tables, 11, name);
        snprintf(name, sizeof name, "aya%02X.cat", key);
        costume_tables_add(&tables, 11, name);
    }
    check(costume_tables_count(&tables, 11) == COSTUME_MAX, "64 costumes at most");
    check(costume_tables_get(&tables, 11, 63, &xpr, &cat) && strcmp(xpr, "aya3F.xpr") == 0,
          "the 64th costume is the 64th key");

    unsigned first, count;
    costume_page_rows(5, 4, &first, &count);
    check(first == 0 && count == 5, "a short menu is one page");
    costume_page_rows(23, 12, &first, &count);
    check(first == 9 && count == 9, "row 12 is on the second page");
    costume_page_rows(23, 22, &first, &count);
    check(first == 18 && count == 5, "the last page is short");

    check(costume_page_flip(9, 4, 1) == 4, "one page keeps its row");
    check(costume_page_flip(23, 4, 1) == 13, "Right keeps the place on the next page");
    check(costume_page_flip(23, 13, 1) == 22, "a shorter last page takes its last row");
    check(costume_page_flip(23, 20, 1) == 2, "Right wraps to the first page");
    check(costume_page_flip(23, 2, -1) == 20, "Left wraps to the last page");

    if (failures == 0) printf("costume pages: ok\n");
    return failures != 0;
}
