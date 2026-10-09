#ifndef DOA3_FIGHTER_H
#define DOA3_FIGHTER_H

#include <stddef.h>
#include <stdint.h>

/* Guest layout, not a host-owned fighter. Unknown bytes stay unnamed. */
typedef struct Doa3Fighter {
    uint8_t unknown_00;
    uint8_t character;                 /* 0x01 */
    uint8_t unknown_02[2];
    float x, y, z;                     /* 0x04 */
    uint8_t unknown_10[4];
    uint32_t facing;                   /* 0x14, one turn = 0x10000 */
    uint8_t unknown_18[4];
    uint32_t direction_1c;
    uint8_t unknown_20[14];
    uint16_t action;                   /* 0x2e, within the action group */
    uint8_t unknown_30[3];
    uint8_t action_group;              /* 0x33 */
    uint8_t unknown_34[11];
    uint8_t paired_3f;
    uint8_t edge_reaction;             /* 0x40 */
    uint8_t unknown_41[16];
    uint8_t boundary_mask;             /* 0x51 */
    uint8_t unknown_52[3];
    uint8_t contact_sequence;          /* 0x55 */
    uint8_t unknown_56[12];
    uint8_t fall_state;                /* 0x62 */
    uint8_t unknown_63[2];
    uint8_t state_65;
    uint8_t unknown_66[2];
} Doa3Fighter;

_Static_assert(sizeof(Doa3Fighter) == 0x68, "fighter stride");
_Static_assert(offsetof(Doa3Fighter, action) == 0x2e, "action offset");
_Static_assert(offsetof(Doa3Fighter, edge_reaction) == 0x40, "edge offset");
_Static_assert(offsetof(Doa3Fighter, fall_state) == 0x62, "fall offset");

enum {
    DOA3_FIGHTERS = 0x00484c48,
    DOA3_EDGE_FIRST_SOLID = 8,
    DOA3_EDGE_CLIFF = 0x0e,
    DOA3_EDGE_SPECIAL = 0x12,
    DOA3_BOUNDARY_EXEMPT = 0x14,
    DOA3_REACTION_CONTACT = 1,
    DOA3_REACTION_TRANSFER = 4,
    DOA3_FALL_PENDING = 2
};

#endif
