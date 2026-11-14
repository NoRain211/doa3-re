/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOA3_RECOMP_MENU_QUIT_H
#define DOA3_RECOMP_MENU_QUIT_H

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Quit Game, the main menu's last entry: a label and picture the runner
   draws, and a help line. Choosing it opens the runner's quit prompt
   (quit_prompt.c); keeping on leaves the menu as it was. Ported from DOA2U's
   menu_quit.c; DOA3's carousel is a table of 12-byte entries, not DOA2U's
   list, so the hooks differ. */

/* Call at each boot, once the XBE is mapped and before the game runs: takes
   guest memory from the heap and draws the label and picture. Does nothing
   outside the USA build, whose forwards wrap the functions below. */
void recomp_menu_quit_install(void);

/* program_forwards.c wraps these game functions; each takes the original. */
void recomp_menu_quit_build(RecompFunction original);   /* 0x000c8010: lists the entries */
void recomp_menu_quit_entry(RecompFunction original);   /* 0x000c83a0: entry at a position */
void recomp_menu_quit_mode(RecompFunction original);    /* 0x000c7fc0: entry of a mode */
void recomp_menu_quit_help(RecompFunction original);    /* 0x000559f0: help line by number */
void recomp_menu_quit_draw(RecompFunction original);    /* 0x0006d140: draws one sprite */
void recomp_menu_quit_update(RecompFunction original);  /* 0x000d0060: the menu's frame */

#ifdef __cplusplus
}
#endif

#endif
