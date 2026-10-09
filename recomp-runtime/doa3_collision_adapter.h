#ifndef DOA3_COLLISION_ADAPTER_H
#define DOA3_COLLISION_ADAPTER_H

#include "runtime.h"

void recomp_doa3_boundary_exempt(void);
void recomp_doa3_action_matches(void);
RecompFunction recomp_doa3_collision_lookup(uint32_t address);
void recomp_doa3_boundary_pass(void);
void recomp_doa3_danger_zones(void);
uint8_t doa3_boundary_exempt(unsigned index);
uint8_t doa3_action_matches(unsigned index, unsigned category);
void doa3_boundary_pass(void);
void doa3_danger_zones(void);

#endif
