#ifndef DOA3_RECOMP_COSTUME_TEAMS_H
#define DOA3_RECOMP_COSTUME_TEAMS_H

#include <stdbool.h>
#include <stdint.h>

/* Which swimsuit each fighter bank wears (docs/doaxbv-swimsuits.md). DOA3
   keeps fighters in four banks, team = bank % 2; Tag Battle puts a team's
   partner in bank team + 2. Each girl on a team keeps her own suit, the
   team's menu edits the girl whose head drew last, and a girl off the team
   for over a second starts over. Knows nothing of the guest:
   costume_inject.c feeds it each bank's girl every draw. */

enum { RECOMP_COSTUME_TEAMS = 2, RECOMP_COSTUME_BANKS = 4, RECOMP_COSTUME_GIRLS = 7, RECOMP_COSTUME_UNSET = -2 };

typedef struct RecompCostumeTeams {
    /* Each girl's suit on each team: a suit index, -1 stock, or
       RECOMP_COSTUME_UNSET until the caller picks her starting suit. */
    int choice[RECOMP_COSTUME_TEAMS][RECOMP_COSTUME_GIRLS];
    uint32_t seen[RECOMP_COSTUME_TEAMS][RECOMP_COSTUME_GIRLS];
    /* Each bank's last girl (-1 none yet), the girl she may be mistaken for
       (-1 none), and the present her head last drew. */
    int bank_girl[RECOMP_COSTUME_BANKS], bank_twin[RECOMP_COSTUME_BANKS];
    uint32_t bank_shown[RECOMP_COSTUME_BANKS];
    /* The girl each team's menu edits, -1 none. */
    int team_girl[RECOMP_COSTUME_TEAMS];
} RecompCostumeTeams;

void recomp_costume_teams_init(RecompCostumeTeams *t);

/* A draw saw bank's fighter: girl (-1 not one of the girls; the bank keeps
   its last girl), twin, the girl sharing her skeleton (-1 none; told apart
   only while her head draws), and whether this draw is her head. Returns
   true when the bank's girl changed: she then wears choice[team][girl],
   which the caller fills when RECOMP_COSTUME_UNSET. */
bool recomp_costume_teams_note(RecompCostumeTeams *t, unsigned bank, int girl, int twin, bool head, uint32_t present);

#endif
