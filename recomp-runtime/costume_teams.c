#include "costume_teams.h"

/* Presents a girl may be off her team (a tag swap, a hovered fighter) before she starts over. */
enum { FORGET_PRESENTS = 60 };

void recomp_costume_teams_init(RecompCostumeTeams *t)
{
    for (unsigned team = 0; team < RECOMP_COSTUME_TEAMS; ++team) {
        t->team_girl[team] = -1;
        for (unsigned g = 0; g < RECOMP_COSTUME_GIRLS; ++g) { t->choice[team][g] = RECOMP_COSTUME_UNSET; t->seen[team][g] = 0; }
    }
    for (unsigned b = 0; b < RECOMP_COSTUME_BANKS; ++b) { t->bank_girl[b] = t->bank_twin[b] = -1; t->bank_shown[b] = 0; }
}

/* In one of the team's banks. Off screen, a bank's twin is a guess from old hair, so she counts too. */
static bool on_team(const RecompCostumeTeams *t, unsigned team, int girl, uint32_t present)
{
    for (unsigned b = team; b < RECOMP_COSTUME_BANKS; b += RECOMP_COSTUME_TEAMS)
        if (t->bank_girl[b] == girl || (t->bank_twin[b] == girl && present - t->bank_shown[b] > 1u)) return true;
    return false;
}

static void forget_if_gone(RecompCostumeTeams *t, unsigned team, int girl, uint32_t present)
{
    if (on_team(t, team, girl, present)) t->seen[team][girl] = present;
    else if (present - t->seen[team][girl] > FORGET_PRESENTS) t->choice[team][girl] = RECOMP_COSTUME_UNSET;
}

bool recomp_costume_teams_note(RecompCostumeTeams *t, unsigned bank, int girl, int twin, bool head, uint32_t present)
{
    const unsigned team = bank % RECOMP_COSTUME_TEAMS, partner = team + RECOMP_COSTUME_TEAMS;
    if (head) t->bank_shown[bank] = present;
    const bool changed = girl >= 0 && girl != t->bank_girl[bank];
    /* Before she joins: a girl back after a while starts over. */
    if (changed) forget_if_gone(t, team, girl, present);
    if (girl >= 0) { t->bank_girl[bank] = girl; t->bank_twin[bank] = twin; }
    for (int g = 0; g < RECOMP_COSTUME_GIRLS; ++g) forget_if_gone(t, team, g, present);
    const int on = t->bank_girl[t->bank_shown[partner] > t->bank_shown[team] ? partner : team];
    if (on >= 0) t->team_girl[team] = on;
    return changed;
}
