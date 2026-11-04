/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECOMP_GAME_STATE_H
#define RECOMP_GAME_STATE_H

#include "input_model.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DOA3 guest state a netplay session reads and steers. */

/* A lobby session predicts only while a round is live: the game in its match
   state (1), the pause menu closed and the fight tasks running, the round in
   its live phase (1), and neither decided by a KO nor out of time. The judge
   decides a KO in the frame a life reaches zero, Tag Battle included, so a
   round's end closes the window before the save at the end of a match. */
bool recomp_game_round_live(void);
/* Sets the frame's press toward Versus: Start until the front end runs (game
   state 0), Right until the main menu (scene 0) has Versus (index 5) under
   its cursor, then with select A, and A for Single, the first entry on the
   battle type menu (scene 2), to character select (scene 3). It presses for
   one frame in every 16 and only while the scene is settled. True on
   arrival, leaving pad alone. */
bool recomp_game_walk_to_versus(uint32_t frame, bool select, RecompInputGamepad *pad);
/* The settled main menu has Versus under its cursor, where A opens the netplay menu. */
bool recomp_game_versus_chosen(void);
/* Fighter 1's life, for RECOMP_ROLLBACK_FLIP's desync test. */
uint32_t recomp_game_life_address(void);

#ifdef __cplusplus
}
#endif

#endif
