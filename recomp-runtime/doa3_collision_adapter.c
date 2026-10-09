#include "doa3_collision_adapter.h"
#include "doa3_fighter.h"
#include "kernel_abi.h"

#include <string.h>

static uint8_t *byte_at(uint32_t address)
{
    return recomp_memory(address, 1);
}

static Doa3Fighter *fighter(unsigned index)
{
    return (Doa3Fighter *)recomp_memory(
        DOA3_FIGHTERS + index * sizeof(Doa3Fighter), sizeof(Doa3Fighter));
}

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static float *float_at(uint32_t address)
{
    return (float *)recomp_memory(address, sizeof(float));
}

/* One bridge for cdecl calls: push arguments, dispatch, reclaim the frame. */
static uint32_t call(uint32_t address, const uint32_t *args, unsigned count)
{
    kernel_call_guest(address, args, count);
    return recomp_runtime.registers.eax;
}

static uint8_t action_table(uint32_t table, unsigned first, unsigned second)
{
    const uint32_t args[] = {table, DOA3_FIGHTERS + first * 0x68,
                            DOA3_FIGHTERS + second * 0x68};
    return (uint8_t)call(0x000a5fe0, args, 3) == 1;
}

uint8_t doa3_action_matches(unsigned index, unsigned category)
{
    Doa3Fighter *f = fighter(index);
    unsigned opponent = index ^ 1;
    uint32_t table;
    unsigned first = index, second = index;

    switch (category) {
    case 0:
        if (action_table(0x00332898, opponent, opponent)) return 1;
        return (uint8_t)call(0x000a7810, &index, 1) == 1;
    case 1: table = 0x00332910; second = opponent; break;
    case 2:
        if ((f->action_group == 0 &&
             ((f->action >= 0x15 && f->action <= 0x2c) ||
              f->action == 0x30 || f->action == 0x31)) ||
            (f->action_group == 13 && (f->action == 0x32 || f->action == 0x33)) ||
            f->action_group == 7) return 1;
        table = 0x003326c8;
        break;
    case 3:
        if (*byte_at(0x0085ba30 + index) == 1 ||
            (f->action_group == 0 && (f->state_65 == 1 || f->state_65 == 4)))
            return 1;
        table = 0x003327e8;
        break;
    case 4: table = 0x00332870; break;
    case 5:
        if (action_table(0x00332964, index, index)) return 1;
        if (f->action_group == 3 && f->action >= 0x90 && f->action <= 0x95) {
            if (f->character == 7) return f->action != 0x91 && f->action != 0x94;
            if (f->character == 0x13)
                return f->action != 0x90 && f->action != 0x92 && f->action != 0x94;
            return 1;
        }
        if ((f->action_group == 13 && (f->action == 0x12 || f->action == 0x13)) ||
            (f->action_group == 5 && (f->action == 0x7a || f->action == 0x7b)))
            return 1;
        return f->action_group == 0 && (f->state_65 == 1 || f->state_65 == 4) &&
               f->facing == ((f->direction_1c - 0x8000u) & 0xffffu);
    case 6: table = 0x00332980; break;
    case 7: table = 0x00332a58; first = opponent; break;
    case 8: table = 0x00332a7c; break;
    case 9: table = 0x0033720c; break;
    case 10: table = 0x00337240; break;
    case 11: table = 0x00337288; first = opponent; break;
    case 12: table = 0x00332898; break;
    case 14: table = 0x00337990; first = opponent; break;
    case 15: table = 0x003375b0; first = opponent; break;
    case 16: table = 0x003375f8; first = opponent; break;
    case 17: table = 0x00337608; break;
    case DOA3_BOUNDARY_EXEMPT: table = 0x0033799c; break;
    case 21: table = 0x003379b0; break;
    case 22: table = 0x003376dc; break;
    default: return 0;
    }
    return action_table(table, first, second);
}

uint8_t doa3_boundary_exempt(unsigned index)
{
    Doa3Fighter *f = fighter(index);
    unsigned opponent = index ^ 1;
    uint8_t other_group = fighter(opponent)->action_group;

    if ((*byte_at(0x0048e60e) == 1 && *byte_at(0x0085bd04 + index) != 2) ||
        *byte_at(0x0085bd84 + index) == 1 ||
        (f->action_group == 13 && (f->action == 0xca || f->action == 0xcb)) ||
        (f->action_group == 1 && f->action == 0x3b) ||
        (*byte_at(0x0085bd48 + index) == 1 &&
         ((other_group != 10 && other_group != 11) ||
          *byte_at(0x004bc33c + opponent) == 1))) return 1;
    return doa3_action_matches(index, DOA3_BOUNDARY_EXEMPT);
}

static uint8_t stage(void)
{
    return *byte_at(0x00484d74 + *byte_at(0x0048e611));
}

static int scripted_transfer(uint8_t location)
{
    return location == 0x38 || location == 0x3a || location == 0x44;
}

/* The two switches in the original retain their stack byte for stage 0x35
   when 0x479cd0 is not 1. Read that byte rather than inventing an initial value. */
static uint8_t fall_stage(uint8_t location, uint8_t previous)
{
    switch (location) {
    case 0x35: return *byte_at(0x00479cd0) == 1 ? 1 : previous;
    case 0x3b: case 0x3d: case 0x3e: case 0x41: case 0x4d: return 1;
    default: return 0;
    }
}

void doa3_danger_zones(void)
{
    uint32_t saved_sp = recomp_runtime.registers.esp;
    uint32_t scratch = saved_sp - 0x20;
    uint8_t pending_stage = *byte_at(saved_sp - 0x13);
    uint8_t reaction_stage = *byte_at(saved_sp - 0x12);
    recomp_runtime.registers.esp = scratch;

    for (unsigned i = 0; i < 2; ++i) {
        Doa3Fighter *f = fighter(i);
        uint8_t location = stage();
        if (f->fall_state == DOA3_FALL_PENDING) {
            pending_stage = fall_stage(location, pending_stage);
            if (!pending_stage && !scripted_transfer(location)) {
                f->fall_state = 0;
            } else {
                uint32_t point[] = {float_bits(f->x), float_bits(f->z)};
                int32_t region = (int32_t)call(0x000d48a0, point, 2);
                location = stage();
                int keep;
                if (region >= 0) {
                    keep = location == 0x37 || location == 0x49 || location == 0x40 ||
                           scripted_transfer(location) || location == 0x43 || location == 0x4c;
                } else if (location == 0x29) {
                    keep = 0;
                } else {
                    uint32_t args[] = {scratch + 0x1c, scratch + 0x18, point[0], point[1]};
                    uint32_t edge = call(0x000d6130, args, 4);
                    keep = edge == 0x0d || edge == 0x0c || edge == 0x18;
                }
                if (!keep) f->fall_state = 0;
                if (f->fall_state == DOA3_FALL_PENDING && scripted_transfer(stage())) {
                    call(0x000deb30, &i, 1);
                    f->fall_state = 0;
                }
            }
        }
        if (f->edge_reaction != DOA3_REACTION_TRANSFER) continue;
        location = stage();
        if (location == 0x37 || location == 0x40 || location == 0x43 || location == 0x4c) {
            if (f->action_group != 10 && f->action_group != 11) continue;
        } else {
            reaction_stage = fall_stage(location, reaction_stage);
            if (reaction_stage) continue;
            if (location == 0x34 || location == 0x3f ||
                (location == 0x35 && *byte_at(0x00479cd0) == 0)) {
                uint8_t state = *byte_at(0x0085bc04 + i);
                if ((state == 2 || state == 4) && *byte_at(0x0085bbb0 + i) == 1 &&
                    *byte_at(0x0085bed8 + i) == 0 &&
                    (*byte_at(0x004b838a) == 1 ||
                     (int8_t)*byte_at(0x00309c68 + location) < 0 ||
                     (uint8_t)call(0x000701d0, NULL, 0) != 0) &&
                    *float_at(0x0085bd8c + i * 4) <= *float_at(0x001ed6c8) &&
                    (*byte_at(0x0047e727) != 4 || i != *byte_at(0x0048a2ce)) &&
                    fighter(i ^ 1)->edge_reaction != DOA3_REACTION_TRANSFER &&
                    *byte_at(0x0085bd04 + (i ^ 1)) == 0) continue;
            } else if (scripted_transfer(location)) {
                call(0x000deb30, &i, 1);
            }
        }
        f->edge_reaction = DOA3_REACTION_CONTACT;
    }
    recomp_runtime.registers.esp = saved_sp;
}

/* Sine/cosine use the game's angle convention and guest lookup data. */
static double angle_component(uint32_t address, uint32_t angle)
{
    recomp_runtime.registers.ecx = angle;
    call(address, NULL, 0);
    unsigned top = recomp_runtime.fpu_top;
    double value = recomp_runtime.fpu_stack[top];
    recomp_runtime.fpu_top = (top + 1) & 7;
    return value;
}

static void contact(unsigned i, uint32_t edge, uint32_t angle, int react)
{
    *recomp_memory_u32(0x004ba880 + i * 4) = angle - 0x4000u;
    *recomp_memory_u32(0x004bb858 + i * 4) = edge;
    if (react) {
        uint32_t args[] = {i, edge};
        *byte_at(0x004bb7f8 + i) = (uint8_t)call(0x000a4450, args, 2);
    }
}

typedef struct BoundaryScratch {
    uint32_t hit_angle;
    float hit_depth, plane_distance;
    float distances[2];
    uint32_t angles[2];
    float point[4], origin[4];
} BoundaryScratch;

static void transform_point(uint32_t matrix, uint32_t point, uint32_t result)
{
    /* Both math helpers use register arguments (ECX, then ECX/EDX). */
    recomp_runtime.registers.ecx = matrix;
    call(0x00154680, NULL, 0);
    recomp_runtime.registers.ecx = point;
    recomp_runtime.registers.edx = result;
    call(0x00154750, NULL, 0);
}

void doa3_boundary_pass(void)
{
    if (*byte_at(0x0047e795)) return;
    if (*byte_at(0x004b838a) && !*byte_at(0x004b8432)) {
        fighter(0)->edge_reaction = fighter(1)->edge_reaction = 0;
        return;
    }

    /* Lifted helpers require guest addresses for their output parameters. */
    uint32_t saved_sp = recomp_runtime.registers.esp;
    uint32_t scratch = saved_sp - sizeof(BoundaryScratch);
    BoundaryScratch *local = (BoundaryScratch *)recomp_memory(scratch, sizeof *local);
    uint32_t distances = scratch + offsetof(BoundaryScratch, distances);
    uint32_t angles = scratch + offsetof(BoundaryScratch, angles);
    uint32_t hit_angle = scratch + offsetof(BoundaryScratch, hit_angle);
    uint32_t hit_depth = scratch + offsetof(BoundaryScratch, hit_depth);
    uint32_t point_address = scratch + offsetof(BoundaryScratch, point);
    recomp_runtime.registers.esp = scratch;
    memset(byte_at(0x004bb7f8), 0, 3);
    memset(recomp_memory(0x004bb858, 8), 0, 8);
    memset(recomp_memory(0x004ba880, 8), 0, 8);
    memset(recomp_memory(0x0085bd5c, 2), 0, 2);
    unsigned paired = 0;
    for (unsigned i = 0; i < 2; ++i) {
        Doa3Fighter *f = fighter(i), *other = fighter(i ^ 1);
        f->edge_reaction = 0;
        if (((f->action_group == 10 && other->action_group == 4) ||
             (f->action_group == 11 && other->action_group == 6)) &&
            *byte_at(0x004bc33c + i) == 0) paired = 1;
    }
    if (!paired && (fighter(0)->paired_3f == 1 || fighter(1)->paired_3f == 1) &&
        (uint8_t)call(0x000a1be0, NULL, 0) == 0) paired = 2;

    for (unsigned i = 0; i < 2; ++i) {
        Doa3Fighter *f = fighter(i);
        float *distance = float_at(distances + i * 4);
        uint32_t *angle = recomp_memory_u32(angles + i * 4);
        *distance = 0;
        *angle = 0;
        *recomp_memory_u32(0x0085bed0 + i * 4) = UINT32_MAX;
        if (doa3_boundary_exempt(i) == 1 ||
            (uint8_t)call(0x000a1430, &i, 1) == 1) continue;
        uint32_t point[] = {float_bits(f->x), float_bits(f->z)};
        int32_t region = (int32_t)call(0x000d48a0, point, 2);
        uint32_t args[] = {distances + i * 4, angles + i * 4, i};
        uint32_t edge = (uint8_t)call(0x000a2510, args, 3);
        int special = edge == DOA3_EDGE_SPECIAL;
        int32_t mask = 0;
        float push_x = 0, push_z = 0;
        if (edge >= DOA3_EDGE_FIRST_SOLID) {
            if (!special) {
                mask = (int8_t)*byte_at(0x0032e978);
                *byte_at(0x0085bd5c + i) = 1;
                contact(i, edge, *angle, 0);
                push_x = (float)(angle_component(0x00153f60, *angle) * *distance);
                push_z = (float)(angle_component(0x00153f80, *angle) * *distance);
            }
            uint32_t reaction[] = {i, edge};
            *byte_at(0x004bb7f8 + i) = (uint8_t)call(0x000a4450, reaction, 2);
        }
        uint32_t bones = 0x0032e8d8, points = 0x0032e8e8, masks = 0x0032e978;
        unsigned count = 9;
        if (f->character == 0x13 && f->action_group == 13 &&
            f->action >= 0x106 && f->action <= 0x109) {
            bones = 0x0032e984; points = 0x0032e990; masks = 0x0032ea50; count = 12;
            uint32_t message = 0x001ff708;
            call(0x0017ec00, &message, 1);
        }
        for (unsigned j = 0; j < count; ++j) {
            int32_t bone = (int8_t)*byte_at(bones + j);
            transform_point(0x004785d0 + (bone + i * 23) * 64,
                            points + j * 16, point_address);
            uint32_t query[] = {hit_angle, hit_depth, point_address, i};
            int32_t type = (int32_t)call(0x000a3180, query, 4);
            if (type < DOA3_EDGE_FIRST_SOLID && region >= 0) {
                uint32_t region_query[] = {query[0], query[1], query[2],
                    0x0047e7d0 + (bone + i * 15) * 32, (uint32_t)region, i};
                type = (int32_t)call(0x000a31a0, region_query, 6);
                if (type >= DOA3_EDGE_FIRST_SOLID) {
                    local->origin[0] = f->x;
                    local->origin[2] = f->z;
                    uint32_t plane[] = {query[0], scratch + offsetof(BoundaryScratch, plane_distance),
                        scratch + offsetof(BoundaryScratch, origin), (uint32_t)region};
                    call(0x000d5330, plane, 4);
                    *recomp_memory_u32(0x0085bed0 + i * 4) = (uint32_t)region;
                }
            }
            if (type < DOA3_EDGE_FIRST_SOLID) continue;
            uint32_t contact_angle = *recomp_memory_u32(query[0]);
            float depth = *float_at(query[1]);
            if (!mask) contact(i, (uint32_t)type, contact_angle, !special);
            mask |= (int8_t)*byte_at(masks + j);
            if (special) continue;
            if (*distance < depth) { *distance = depth; *angle = contact_angle; }
            push_x = (float)(angle_component(0x00153f60, contact_angle) * depth + push_x);
            push_z = (float)(angle_component(0x00153f80, contact_angle) * depth + push_z);
        }
        f->boundary_mask = (uint8_t)mask;
        if (!special && *byte_at(0x004bb7f8 + i)) {
            uint32_t query[] = {hit_angle, i, float_bits(*distance)};
            edge = (uint8_t)call(0x000a2b00, query, 3);
            if (edge >= DOA3_EDGE_FIRST_SOLID)
                contact(i, edge, *recomp_memory_u32(query[0]), 1);
            uint32_t vector[] = {float_bits(push_x), float_bits(push_z)};
            *angle = call(0x00154040, vector, 2);
        }
        if ((f->edge_reaction == 1 || f->edge_reaction == 4) &&
            (f->action_group == 10 || f->action_group == 11) &&
            (uint8_t)call(0x000a4f00, &i, 1) == 0 && !f->contact_sequence)
            f->contact_sequence = ++*byte_at(0x004bad90 + i);
    }

    uint8_t *reactions = recomp_memory(0x004bb7f8, 3);
    if (paired == 1 || (paired == 2 && ((reactions[0] == 1) != (reactions[1] == 1)))) {
        reactions[0] = reactions[1] = reactions[2] = reactions[0] | reactions[1];
        unsigned deeper = *float_at(distances) < *float_at(distances + 4) ? 1 : 0;
        float depth = *float_at(distances + deeper * 4);
        uint32_t angle = *recomp_memory_u32(angles + deeper * 4);
        *float_at(distances) = *float_at(distances + 4) = local->hit_depth = depth;
        *recomp_memory_u32(angles) = *recomp_memory_u32(angles + 4) =
            local->hit_angle = angle;
    }
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t args[] = {0x004bb7f8 + i, distances + i * 4, angles + i * 4, i};
        call(0x0009a090, args, 4);
    }
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t args[] = {0x004bbd48 + i * 16, i,
            *recomp_memory_u32(angles + i * 4), *recomp_memory_u32(distances + i * 4)};
        call(0x0009a2c0, args, 4);
    }
    if (*recomp_memory_u32(0x004bcf80) && reactions[2] && !*byte_at(0x004bcfd7)) {
        uint32_t args[] = {0x004bbd68, 0, local->hit_angle,
                           float_bits(local->hit_depth)};
        call(0x0009a2c0, args, 4);
        call(0x000950e0, args, 1);
    }
    recomp_runtime.registers.esp = saved_sp;
    doa3_danger_zones();
}

/* Temporary scaffold seam: geometry, transforms and reaction helpers remain
   lifted. The two predicates return AL; both passes are cdecl void functions.
   Stage 0x35 still consumes the original danger pass's uninitialized bytes. */
RecompFunction recomp_doa3_collision_lookup(uint32_t address)
{
    switch (address) {
    case 0x000a1380: return recomp_doa3_boundary_exempt;
    case 0x000a9ac0: return recomp_doa3_action_matches;
    case 0x0008d180: return recomp_doa3_boundary_pass;
    case 0x000a40e0: return recomp_doa3_danger_zones;
    default: return NULL;
    }
}

void recomp_doa3_boundary_exempt(void)
{
    kernel_return_caller_cleanup(doa3_boundary_exempt(kernel_arg(1)));
}

void recomp_doa3_action_matches(void)
{
    kernel_return_caller_cleanup(doa3_action_matches(kernel_arg(1), kernel_arg(2)));
}

void recomp_doa3_boundary_pass(void)
{
    doa3_boundary_pass();
    recomp_runtime.registers.esp += 4;
}

void recomp_doa3_danger_zones(void)
{
    doa3_danger_zones();
    recomp_runtime.registers.esp += 4;
}
