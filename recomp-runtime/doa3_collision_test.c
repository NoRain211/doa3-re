#include "doa3_collision_adapter.h"
#include "doa3_fighter.h"
#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t ram[0x01000000];
RecompRuntime recomp_runtime;
uint8_t *recomp_fast_ram;
static uint32_t table_result, last_table, last_first, last_second;
static unsigned calls, transfers, transforms, adjustments;
static uint32_t region_result = UINT32_MAX, edge_result, boundary_edge_result, paired_depth;

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

uint8_t *recomp_memory(uint32_t address, size_t size)
{
    CHECK(address <= sizeof ram && size <= sizeof ram - address);
    return ram + address;
}

uint32_t *recomp_memory_u32_checked(uint32_t address)
{
    return (uint32_t *)recomp_memory(address, 4);
}

/* Synthetic callees check the actual adapter arguments and cleanup. */
void recomp_dispatch_indirect_site(uint32_t address, uint32_t saved_sp,
                                  const char *file, int line)
{
    (void)saved_sp; (void)file; (void)line;
    if (address == 0x000a9ac0) {
        recomp_doa3_action_matches();
        return;
    }
    uint32_t frame = recomp_runtime.registers.esp;
    uint32_t *args = recomp_memory_u32(frame + 4);
    switch (address) {
    case 0x000d48a0: recomp_runtime.registers.eax = region_result; break;
    case 0x000d6130:
        *recomp_memory_u32(args[0]) = 0;
        *recomp_memory_u32(args[1]) = 0;
        recomp_runtime.registers.eax = edge_result, boundary_edge_result, paired_depth;
        break;
    case 0x000deb30: ++transfers; break;
    case 0x00154680: ++transforms; break;
    case 0x00154750:
        memset(recomp_memory(recomp_runtime.registers.edx, 16), 0, 16);
        break;
    case 0x000a2510:
        *recomp_memory_u32(args[0]) = args[2] ? 0x40800000 : 0x40000000;
        *recomp_memory_u32(args[1]) = 0;
        recomp_runtime.registers.eax = boundary_edge_result;
        break;
    case 0x000a4450: recomp_runtime.registers.eax = 1; break;
    case 0x000a2b00:
    case 0x00154040: recomp_runtime.registers.eax = 0; break;
    case 0x00153f60:
    case 0x00153f80:
        recomp_runtime.fpu_top = (recomp_runtime.fpu_top + 7) & 7;
        recomp_runtime.fpu_stack[recomp_runtime.fpu_top] = 1.0;
        break;
    case 0x000950e0: break;
    case 0x000a3180:
        *recomp_memory_u32(args[0]) = 0;
        *recomp_memory_u32(args[1]) = 0;
        recomp_runtime.registers.eax = 0;
        break;
    case 0x0009a090: ++adjustments; break;
    case 0x0009a2c0:
        if (args[0] == 0x004bbd68) paired_depth = args[3];
        memset(recomp_memory(args[0], 16), 0, 16);
        break;
    case 0x000a1430: recomp_runtime.registers.eax = 0; break;
    default: goto predicate;
    }
    recomp_runtime.registers.esp += 4;
    return;
predicate:
    CHECK(address == 0x000a5fe0 || address == 0x000a7810);
    ++calls;
    uint32_t sp = recomp_runtime.registers.esp;
    if (address == 0x000a5fe0) {
        last_table = *recomp_memory_u32(sp + 4);
        last_first = *recomp_memory_u32(sp + 8);
        last_second = *recomp_memory_u32(sp + 12);
    }
    recomp_runtime.registers.eax = 0x12340000u | table_result;
    recomp_runtime.registers.esp += 4;
}

static uint32_t invoke(void (*entry)(void), unsigned index, unsigned category)
{
    const uint32_t sp = 0x00e00000;
    RecompRegisters incoming = {0x11223344, 0x22334455, 0x33445566,
                               0x44556677, 0x55667788, 0x66778899,
                               0x778899aa, sp};
    recomp_runtime.registers = incoming;
    *recomp_memory_u32(sp) = 0x01020304;
    *recomp_memory_u32(sp + 4) = index;
    *recomp_memory_u32(sp + 8) = category;
    calls = 0;
    entry();
    CHECK(recomp_runtime.registers.esp == sp + 4);
    CHECK(recomp_runtime.registers.ebx == incoming.ebx);
    CHECK(recomp_runtime.registers.esi == incoming.esi);
    CHECK(recomp_runtime.registers.edi == incoming.edi);
    CHECK(recomp_runtime.registers.ebp == incoming.ebp);
    CHECK(*recomp_memory_u32(sp) == 0x01020304);
    CHECK(*recomp_memory_u32(sp + 4) == index);
    CHECK(*recomp_memory_u32(sp + 8) == category);
    return recomp_runtime.registers.eax;
}

int main(void)
{
    CHECK(recomp_doa3_collision_lookup(0x000a1380) == recomp_doa3_boundary_exempt);
    CHECK(recomp_doa3_collision_lookup(0x000a9ac0) == recomp_doa3_action_matches);
    CHECK(recomp_doa3_collision_lookup(0x0008d180) == recomp_doa3_boundary_pass);
    Doa3Fighter *f = (Doa3Fighter *)(ram + DOA3_FIGHTERS);
    CHECK(invoke(recomp_doa3_boundary_exempt, 0, 0) == 0);
    CHECK(calls == 1 && last_table == 0x0033799c);
    CHECK(last_first == DOA3_FIGHTERS && last_second == DOA3_FIGHTERS);

    table_result = 1;
    CHECK(invoke(recomp_doa3_boundary_exempt, 1, 0) == 1);
    CHECK(last_first == DOA3_FIGHTERS + 0x68 && last_second == last_first);
    table_result = 0;
    ram[0x0048e60e] = 1;
    CHECK(invoke(recomp_doa3_boundary_exempt, 0, 0) == 1 && calls == 0);
    ram[0x0085bd04] = 2;
    CHECK((uint8_t)invoke(recomp_doa3_boundary_exempt, 0, 0) == 0 && calls == 1);
    ram[0x0048e60e] = 0;
    f->action_group = 13; f->action = 0xca;
    CHECK((uint8_t)invoke(recomp_doa3_boundary_exempt, 0, 0) == 1 && calls == 0);
    f->action_group = 0; f->action = 0;
    ram[0x0085bd48] = 1;
    f[1].action_group = 10;
    CHECK((uint8_t)invoke(recomp_doa3_boundary_exempt, 0, 0) == 0 && calls == 1);
    ram[0x004bc33d] = 1;
    CHECK((uint8_t)invoke(recomp_doa3_boundary_exempt, 0, 0) == 1 && calls == 0);
    ram[0x0085bd48] = 0;

    /* Mixed-width flag join: AL == 1 must not hide an unequal facing. */
    f->state_65 = 1; f->direction_1c = 0x9000; f->facing = 0x1001;
    CHECK((uint8_t)invoke(recomp_doa3_action_matches, 0, 5) == 0);
    f->facing = 0x1000;
    CHECK((uint8_t)invoke(recomp_doa3_action_matches, 0, 5) == 1);
    f->action_group = 3; f->character = 0x13;
    for (unsigned action = 0x90; action <= 0x95; ++action) {
        f->action = (uint16_t)action;
        CHECK((uint8_t)invoke(recomp_doa3_action_matches, 0, 5) == (action & 1));
    }
    f->character = 7; f->action = 0x91;
    CHECK((uint8_t)invoke(recomp_doa3_action_matches, 0, 5) == 0);
    f->action = 0x92;
    CHECK((uint8_t)invoke(recomp_doa3_action_matches, 0, 5) == 1);
    CHECK((uint8_t)invoke(recomp_doa3_action_matches, 0, 7) == 0);
    CHECK(last_first == DOA3_FIGHTERS + 0x68 && last_second == DOA3_FIGHTERS);
    CHECK((uint8_t)invoke(recomp_doa3_action_matches, 0, 1) == 0);
    CHECK(last_first == DOA3_FIGHTERS && last_second == DOA3_FIGHTERS + 0x68);
    CHECK((uint8_t)invoke(recomp_doa3_action_matches, 0, 23) == 0 && calls == 0);
    memset(ram, 0, sizeof ram);
    f[0].fall_state = f[1].fall_state = DOA3_FALL_PENDING;
    invoke(recomp_doa3_danger_zones, 0, 0);
    CHECK(f[0].fall_state == 0 && f[1].fall_state == 0);
    ram[0x00484d74] = 0x3b;
    edge_result = 0x18;
    f[0].fall_state = DOA3_FALL_PENDING;
    invoke(recomp_doa3_danger_zones, 0, 0);
    CHECK(f[0].fall_state == DOA3_FALL_PENDING);
    edge_result = 0;
    invoke(recomp_doa3_danger_zones, 0, 0);
    CHECK(f[0].fall_state == 0);
    ram[0x00484d74] = 0x38;
    region_result = 0;
    f[0].fall_state = DOA3_FALL_PENDING;
    f[1].edge_reaction = DOA3_REACTION_TRANSFER;
    invoke(recomp_doa3_danger_zones, 0, 0);
    CHECK(transfers == 2 && f[0].fall_state == 0);
    CHECK(f[1].edge_reaction == DOA3_REACTION_CONTACT);

    memset(ram, 0, sizeof ram);
    ram[0x0047e795] = 1;
    f[0].edge_reaction = 4;
    invoke(recomp_doa3_boundary_pass, 0, 0);
    CHECK(f[0].edge_reaction == 4 && transforms == 0);
    ram[0x0047e795] = 0;
    ram[0x004b838a] = 1;
    invoke(recomp_doa3_boundary_pass, 0, 0);
    CHECK(f[0].edge_reaction == 0 && transforms == 0);
    ram[0x004b838a] = 0;
    region_result = UINT32_MAX;
    invoke(recomp_doa3_boundary_pass, 0, 0);
    CHECK(transforms == 18 && adjustments == 2);
    CHECK(*recomp_memory_u32(0x0085bed0) == UINT32_MAX);
    CHECK(*recomp_memory_u32(0x0085bed4) == UINT32_MAX);
    f[0].action_group = 10; f[1].action_group = 4;
    boundary_edge_result = DOA3_EDGE_FIRST_SOLID;
    *recomp_memory_u32(0x004bcf80) = 1;
    invoke(recomp_doa3_boundary_pass, 0, 0);
    CHECK(ram[0x004bb7fa] == 1 && paired_depth == 0x40800000);
    puts("synthetic collision passes: passed");
    return 0;
}
