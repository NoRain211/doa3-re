#include "costume_teams.h"

#include <stdio.h>

enum { TINA = 2, KASUMI = 3, HELENA = 4, AYANE = 6, SUIT = 40 };

static int failures;

static void check(bool condition, const char *what)
{
    if (!condition) {
        fprintf(stderr, "costume teams test failed: %s\n", what);
        ++failures;
    }
}

/* One present of a fight: every bank is noted (as each draw does) and the
   shown bank also draws its head. girl[b] < 0 leaves bank b's memory alone. */
static void present(RecompCostumeTeams *t, uint32_t p, const int girl[4], const int twin[4], unsigned shown)
{
    for (unsigned b = 0; b < RECOMP_COSTUME_BANKS; ++b) recomp_costume_teams_note(t, b, girl[b], twin[b], b == shown, p);
}

/* What a caller does when a bank's girl changes: start her in stock. */
static void start(RecompCostumeTeams *t, unsigned team, int girl)
{
    if (t->choice[team][girl] == RECOMP_COSTUME_UNSET) t->choice[team][girl] = -1;
}

static void tag_with_banks_in_place(void)
{
    RecompCostumeTeams t;
    recomp_costume_teams_init(&t);
    const int girls[4] = {TINA, -1, HELENA, -1}, none[4] = {-1, -1, -1, -1};
    uint32_t p = 100;
    for (unsigned b = 0; b < 4; b += 2) check(recomp_costume_teams_note(&t, b, girls[b], -1, b == 0, p), "a bank's first girl is a change");
    start(&t, 0, TINA); start(&t, 0, HELENA);
    check(t.team_girl[0] == TINA, "the menu edits the girl on screen");
    t.choice[0][TINA] = SUIT;
    for (++p; p < 400; ++p) present(&t, p, girls, none, 2);
    check(t.team_girl[0] == HELENA, "the menu follows the partner who tagged in");
    check(t.choice[0][TINA] == SUIT, "the partner off screen keeps her suit");
    check(t.choice[0][HELENA] == -1, "the partner on screen keeps hers");
}

static void tag_with_banks_swapped(void)
{
    RecompCostumeTeams t;
    recomp_costume_teams_init(&t);
    const int before[4] = {TINA, -1, HELENA, -1}, after[4] = {HELENA, -1, TINA, -1}, none[4] = {-1, -1, -1, -1};
    uint32_t p = 100;
    present(&t, p, before, none, 0);
    start(&t, 0, TINA); start(&t, 0, HELENA);
    t.choice[0][TINA] = SUIT;
    for (int round = 0; round < 6; ++round) {
        const int *girls = round & 1 ? before : after;
        const uint32_t end = p + 300;
        for (++p; p < end; ++p) present(&t, p, girls, none, 0);
    }
    check(t.choice[0][TINA] == SUIT, "a suit lasts through tags that swap banks");
    check(t.team_girl[0] == TINA, "the menu edits the girl now in bank 0");
}

static void new_girl_starts_over(void)
{
    RecompCostumeTeams t;
    recomp_costume_teams_init(&t);
    const int tina[4] = {TINA, -1, -1, -1}, helena[4] = {HELENA, -1, -1, -1}, none[4] = {-1, -1, -1, -1};
    uint32_t p = 100;
    present(&t, p, tina, none, 0);
    start(&t, 0, TINA);
    t.choice[0][TINA] = SUIT;
    /* No draws for a while (a loading screen): she stays. */
    p += 500;
    present(&t, p, tina, none, 0);
    check(t.choice[0][TINA] == SUIT, "a girl still in her bank keeps her suit across a gap");
    for (++p; p < 1000; ++p) present(&t, p, helena, none, 0);
    check(t.choice[0][TINA] == RECOMP_COSTUME_UNSET, "a girl gone from the team starts over");
    check(recomp_costume_teams_note(&t, 0, TINA, -1, true, p), "her return is a change");
    check(t.choice[0][TINA] == RECOMP_COSTUME_UNSET, "and she returns unset");
    check(t.choice[1][TINA] == RECOMP_COSTUME_UNSET, "the other team never had her");
}

static void kasumi_then_ayane(void)
{
    RecompCostumeTeams t;
    recomp_costume_teams_init(&t);
    const int ayane[4] = {AYANE, -1, -1, -1}, kasumi[4] = {KASUMI, -1, -1, -1};
    const int ayane_twin[4] = {KASUMI, -1, -1, -1}, kasumi_twin[4] = {AYANE, -1, -1, -1};
    uint32_t p = 100;
    present(&t, p, ayane, ayane_twin, 0);
    start(&t, 0, AYANE);
    t.choice[0][AYANE] = SUIT;
    for (++p; p < 400; ++p) present(&t, p, kasumi, kasumi_twin, 0);
    check(t.choice[0][AYANE] == RECOMP_COSTUME_UNSET, "Ayane replaced by Kasumi on screen starts over");
}

static void kasumi_and_ayane_tag(void)
{
    RecompCostumeTeams t;
    recomp_costume_teams_init(&t);
    /* Banks swap on a tag; the bank off screen keeps its old hair, so it
       names the girl who used to be there. */
    const int before[4] = {KASUMI, -1, AYANE, -1}, after[4] = {AYANE, -1, AYANE, -1};
    const int before_twin[4] = {AYANE, -1, KASUMI, -1}, after_twin[4] = {KASUMI, -1, KASUMI, -1};
    uint32_t p = 100;
    present(&t, p, before, before_twin, 0);
    start(&t, 0, KASUMI); start(&t, 0, AYANE);
    t.choice[0][KASUMI] = SUIT;
    for (++p; p < 400; ++p) present(&t, p, after, after_twin, 0);
    check(t.choice[0][KASUMI] == SUIT, "Kasumi off screen behind stale hair keeps her suit");
}

int main(void)
{
    tag_with_banks_in_place();
    tag_with_banks_swapped();
    new_girl_starts_over();
    kasumi_then_ayane();
    kasumi_and_ayane_tag();
    if (failures) return 1;
    puts("costume teams test passed");
    return 0;
}
