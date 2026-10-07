#ifndef DOAXBV_RECOMP_TYPES_H
#define DOAXBV_RECOMP_TYPES_H

#define RECOMP_RUNTIME_NO_GENERATED_MACROS
#include "runtime.h"
#undef RECOMP_RUNTIME_NO_GENERATED_MACROS

#include <math.h>
#include <string.h>
#include <xmmintrin.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static inline uint32_t BSWAP32(uint32_t value)
{
    return (value >> 24) | ((value >> 8) & 0x0000ff00u) |
           ((value << 8) & 0x00ff0000u) | (value << 24);
}

/* Generated INT3 sites report their guest site instead of trapping the host. */
#undef __debugbreak
#define __debugbreak() recomp_generated_breakpoint(__FILE__, __LINE__)

typedef RecompFunction recomp_func_t;

recomp_func_t recomp_lookup(uint32_t guest_address);
recomp_func_t recomp_lookup_manual(uint32_t guest_address);
recomp_func_t recomp_lookup_kernel(uint32_t guest_address);

extern uint32_t g_recomp_178bb0_origin;
extern uint32_t g_recomp_186fb5_origin;
extern uint32_t g_recomp_17f8e0_origin;
extern uint32_t g_recomp_78d80_origin;
extern uint32_t g_recomp_7c450_origin;
extern uint32_t g_recomp_7c7c0_origin;

#define eax recomp_runtime.registers.eax
#define ecx recomp_runtime.registers.ecx
#define edx recomp_runtime.registers.edx
#define ebx recomp_runtime.registers.ebx
#define esi recomp_runtime.registers.esi
#define edi recomp_runtime.registers.edi
#define esp recomp_runtime.registers.esp
#define g_esp (*recomp_esp_register())
#define g_seh_ebp recomp_runtime.registers.ebp
/* The lifter publishes the established frame as g_ebp and re-publishes it
   before calls; g_seh_ebp is the same guest EBP the SEH helper path reads.
   One register, two emitted names. */
#define g_ebp recomp_runtime.registers.ebp
#define g_fp_stack recomp_runtime.fpu_stack
#define g_fp_top recomp_runtime.fpu_top
#define g_fp_control_word recomp_runtime.fpu_control_word
#define g_fp_cmp recomp_runtime.fpu_compare
#define g_fp_cc recomp_runtime.fpu_status_cc
#define g_df recomp_runtime.direction_flag
/* ponytail: the TIB stays at guest 0, where runner.cpp builds it, so a null
   dereference reads the TIB; move it to a real fs base to trap null. */
#define XBOX_FS_BASE 0u

/* Direct calls. Upstream's optional ABI check is not ported; this is a plain
   call, so direct calls never pass through recomp_lookup_manual. */
#define RECOMP_ABI_CALL(va, fn) (fn)()
#define RECOMP_ICALL_SAFE_AT(address, saved_esp, site) \
    RECOMP_ICALL_SAFE(address, saved_esp)

/* String-instruction step under EFLAGS.DF. */
#define RECOMP_DF_STEP(n) (g_df ? -(int32_t)(n) : (int32_t)(n))

/* lock cmpxchg. Guest code runs on one host thread, so a plain
   read-compare-write on guest memory is atomic with respect to the guest. */
static inline uint32_t recomp_atomic_cas32(
    uint32_t address, uint32_t compare, uint32_t value)
{
    uint32_t *slot = recomp_memory_u32(address);
    uint32_t old = *slot;

    if (old == compare) {
        *slot = value;
    }
    return old;
}
#define RECOMP_ATOMIC_CAS32(pointer, compare, value) \
    recomp_atomic_cas32((uint32_t)(uintptr_t)(pointer), \
                        (uint32_t)(compare), (uint32_t)(value))

/* An instruction the lifter could not translate. Still a no-op; the runtime
   reports the site, and RECOMP_UNIMPL_TRAP=1 stops at the first one. */
void recomp_unimpl(const char *text, uint32_t va);
#define RECOMP_UNIMPL(text, va) recomp_unimpl((text), (va))

/* rdtsc at the Xbox CPU clock, 733.33 MHz. */
uint64_t xbox_ReadTimeStampCounter(void);

/* x87 precision control (control word bits 8-9). PC=24 rounds every
   arithmetic result to float precision while keeping the register's
   exponent range. Ported from the lifter's runtime template. */
static inline double recomp_fp_round24(double x)
{
    double ax = fabs(x);
    int e;

    if ((ax <= 3.4028234663852886e38 && ax >= 1.1754943508222875e-38) ||
        ax == 0.0 || x != x || ax == INFINITY) {
        return (double)(float)x;
    }
    x = frexp(x, &e);
    return ldexp((double)(float)x, e);
}
#define RECOMP_FP_PC(x) \
    ((g_fp_control_word & 0x300u) ? (double)(x) : recomp_fp_round24(x))
/* RECOMP_FCMP result as status-word C3/C2/C0. */
#define RECOMP_FCMP_CC(c) ((uint16_t)((c) == 2 ? 0x4500u : (c) < 0 ? 0x0100u \
                                      : (c) > 0 ? 0u : 0x4000u))

static inline uint16_t recomp_fxam(double value)
{
    return (uint16_t)((signbit(value) ? 0x0200u : 0u) |
        (isnan(value) ? 0x0100u : isinf(value) ? 0x0500u :
         value == 0.0 ? 0x4000u : 0x0400u));
}

/* FRNDINT under the guest RC bits (control word bits 10-11), which the
   host rounding mode never sees. */
static inline double recomp_frndint(double value, uint16_t control)
{
    double rounded;

    if (!isfinite(value)) {
        return value;
    }
    switch ((control >> 10) & 3) {
    case 1: rounded = floor(value); break;
    case 2: rounded = ceil(value); break;
    case 3: rounded = trunc(value); break;
    default: {
        double low = floor(value);
        double fraction = value - low;

        rounded = low;
        if (fraction > 0.5 || (fraction == 0.5 && fmod(low, 2.0) != 0.0)) {
            rounded = low + 1.0;
        }
        break;
    }
    }
    return rounded == 0.0 ? copysign(0.0, value) : rounded;
}

/* FIST: guest rounding; out of range stores the integer-indefinite value. */
static inline int64_t recomp_fist(double value, uint16_t control, unsigned bits)
{
    double rounded = recomp_frndint(value, control);
    double limit = ldexp(1.0, (int)bits - 1);

    if (!isfinite(rounded) || rounded < -limit || rounded >= limit) {
        return bits == 64u ? INT64_MIN : -(INT64_C(1) << (bits - 1u));
    }
    return (int64_t)rounded;
}

/* Result of an x87 compare, in the shape the status word wants:
 *   -1 less, 0 equal, 1 greater, 2 unordered (either operand is NaN).
 * The unordered case matters: `fucompp` of a value with itself followed by
 * `test ah, 0x44; jp` is how this era's CRT asks "is this a NaN", and
 * collapsing it to "equal" answers no every time.
 * Ported from the lifter's own runtime template. */
#define RECOMP_FCMP(a, b) \
    (((a) != (a) || (b) != (b)) ? 2 : (a) < (b) ? -1 : (a) > (b) ? 1 : 0)

/* x86 parity flag: 1 when the low byte of the result has an EVEN number of set
 * bits. Used by the x87 float-compare idiom `fnstsw ax; test ah, mask; jp/jnp`,
 * which is how all pre-SSE code branches on a float comparison.
 * Ported from the lifter's own runtime template. */
static inline int recomp_parity8(uint32_t x)
{
    x &= 0xFFu; x ^= x >> 4; x ^= x >> 2; x ^= x >> 1;
    return (int)(~x & 1u);   /* 1 = even parity (PF set) */
}
#define RECOMP_PARITY8(x) recomp_parity8((uint32_t)(x))

#define MEM32(address) (*recomp_memory_u32((uint32_t)(address)))
#define MEM16(address) (*recomp_memory_u16((uint32_t)(address)))
#define MEM8(address) \
    (*(uint8_t *)(void *)recomp_memory_i8((uint32_t)(address)))
#define SMEM32(address) \
    (*(int32_t *)(void *)recomp_memory_u32((uint32_t)(address)))
#define SMEM64(address) \
    (*(int64_t *)(void *)recomp_memory_u64((uint32_t)(address)))
#define SMEM16(address) \
    (*(int16_t *)(void *)recomp_memory_u16((uint32_t)(address)))
#define SMEM8(address) (*recomp_memory_i8((uint32_t)(address)))
#define MEMF(address) \
    (*(float *)(void *)recomp_memory_u32((uint32_t)(address)))
#define MEMD(address) \
    (*(double *)(void *)recomp_memory_u64((uint32_t)(address)))
#define XBOX_PTR(address) ((void *)(uintptr_t)(uint32_t)(address))
#define memcpy(destination, source, size) \
    recomp_guest_memcpy( \
        (uint32_t)(uintptr_t)(destination), \
        (uint32_t)(uintptr_t)(source), \
        (size_t)(size))
#define memset(destination, value, size) \
    recomp_guest_memset( \
        (uint32_t)(uintptr_t)(destination), \
        (int)(value), \
        (size_t)(size))

#define ZX8(value) ((uint32_t)(uint8_t)(value))
#define ZX16(value) ((uint32_t)(uint16_t)(value))
#define SX8(value) ((uint32_t)(int32_t)(int8_t)(uint8_t)(value))
#define SX16(value) ((uint32_t)(int32_t)(int16_t)(uint16_t)(value))
#define LO8(value) ((uint8_t)((value) & 0xffu))
#define HI8(value) ((uint8_t)(((value) >> 8) & 0xffu))
/* x86 PF: set when the low byte of the result has an even number of set
   bits. The CRT branches on it after FNSTSW AX; TEST AH, mask. */
#define PARITY8(value) \
    ((((0x9669u >> (((uint8_t)(value) ^ ((uint8_t)(value) >> 4)) & 0xfu)) \
       & 1u)) != 0u)
#define LO16(value) ((uint16_t)((value) & 0xffffu))
#define SET_HI8(value, high) \
    ((value) = ((value) & 0xffff00ffu) | \
               ((uint32_t)(uint8_t)(high) << 8))
#define SET_LO8(value, low) \
    ((value) = ((value) & 0xffffff00u) | (uint32_t)(uint8_t)(low))
#define SET_LO16(value, low) \
    ((value) = ((value) & 0xffff0000u) | ((uint32_t)(uint16_t)(low)))

#define CMP_EQ(a, b) ((uint32_t)(a) == (uint32_t)(b))
#define CMP_NE(a, b) ((uint32_t)(a) != (uint32_t)(b))
#define CMP_B(a, b) ((uint32_t)(a) < (uint32_t)(b))
#define CMP_AE(a, b) ((uint32_t)(a) >= (uint32_t)(b))
#define CMP_BE(a, b) ((uint32_t)(a) <= (uint32_t)(b))
#define CMP_A(a, b) ((uint32_t)(a) > (uint32_t)(b))
/* x86 takes SF from the top bit of the operand width, not from bit 31. The
   generated code hands LO8/HI8/LO16 sub-register reads straight to these
   macros, so recover the operand width and sign-extend at that width. A
   sub-register read zero-extends, so casting it to int32_t would make every
   signed test read as non-negative. */
#define RECOMP_FLAG_WIDTH(a, b) (sizeof(a) < sizeof(b) ? sizeof(a) : sizeof(b))
#define RECOMP_SIGNED(value, width) \
    ((width) == 1u ? (int32_t)(int8_t)(uint8_t)(uint32_t)(value) \
     : (width) == 2u ? (int32_t)(int16_t)(uint16_t)(uint32_t)(value) \
     : (int32_t)(uint32_t)(value))
#define CMP_L(a, b) \
    (RECOMP_SIGNED(a, RECOMP_FLAG_WIDTH(a, b)) < \
     RECOMP_SIGNED(b, RECOMP_FLAG_WIDTH(a, b)))
#define CMP_GE(a, b) \
    (RECOMP_SIGNED(a, RECOMP_FLAG_WIDTH(a, b)) >= \
     RECOMP_SIGNED(b, RECOMP_FLAG_WIDTH(a, b)))
#define CMP_LE(a, b) \
    (RECOMP_SIGNED(a, RECOMP_FLAG_WIDTH(a, b)) <= \
     RECOMP_SIGNED(b, RECOMP_FLAG_WIDTH(a, b)))
#define CMP_G(a, b) \
    (RECOMP_SIGNED(a, RECOMP_FLAG_WIDTH(a, b)) > \
     RECOMP_SIGNED(b, RECOMP_FLAG_WIDTH(a, b)))
#define TEST_Z(a, b) (((uint32_t)(a) & (uint32_t)(b)) == 0u)
#define TEST_NZ(a, b) (((uint32_t)(a) & (uint32_t)(b)) != 0u)
#define TEST_S(a, b) \
    (RECOMP_SIGNED((uint32_t)(a) & (uint32_t)(b), \
                   RECOMP_FLAG_WIDTH(a, b)) < 0)

static inline uint32_t ROL32(uint32_t value, unsigned int count)
{
    count &= 31u;
    return count == 0u ? value : (value << count) | (value >> (32u - count));
}

static inline uint32_t ROR32(uint32_t value, unsigned int count)
{
    count &= 31u;
    return count == 0u ? value : (value >> count) | (value << (32u - count));
}

static inline uint8_t ROL8(uint8_t value, int count)
{
    count = (count & 31) % 8;
    return count ? (uint8_t)((value << count) | (value >> (8 - count))) : value;
}

static inline uint8_t ROR8(uint8_t value, int count)
{
    count = (count & 31) % 8;
    return count ? (uint8_t)((value >> count) | (value << (8 - count))) : value;
}

/* RCL/RCR: rotate through carry over width+1 bits; cf carries in and out. */
static inline uint32_t RC_ROT(uint32_t value, unsigned count, int *cf,
                              unsigned width, int left)
{
    unsigned mod = width + 1u;
    uint64_t mask = width >= 32u ? 0xffffffffull : (((uint64_t)1 << width) - 1u);
    uint64_t x = ((uint64_t)(*cf & 1) << width) | ((uint64_t)value & mask);

    count &= 31u;
    if (width < 32u) {
        count %= mod;
    }
    if (count != 0u) {
        x = left ? (x << count) | (x >> (mod - count))
                 : (x >> count) | (x << (mod - count));
        x &= ((uint64_t)1 << mod) - 1u;
    }
    *cf = (int)((x >> width) & 1u);
    return (uint32_t)(x & mask);
}

#define PUSH32(sp, value) do { \
    uint32_t push_value = (uint32_t)(value); \
    (sp) -= 4u; \
    MEM32(sp) = push_value; \
} while (0)
#define POP32(sp, value) do { \
    (value) = MEM32(sp); \
    (sp) += 4u; \
} while (0)
#define RECOMP_ICALL_SAFE(address, saved_esp) do { \
    recomp_dispatch_indirect_site( \
        (uint32_t)(address), (uint32_t)(saved_esp), __FILE__, __LINE__); \
} while (0)
#define RECOMP_ITAIL(address) do { \
    recomp_dispatch_indirect_site( \
        (uint32_t)(address), g_esp, __FILE__, __LINE__); \
} while (0)

/* ── SSE ────────────────────────────────────────────────────────────────
   RecompXmm and the register file live in runtime.h beside the other
   architectural state. These names alias it so generated code can keep
   writing xmm0..xmm7 directly. */
#define xmm0 recomp_runtime.xmm[0]
#define xmm1 recomp_runtime.xmm[1]
#define xmm2 recomp_runtime.xmm[2]
#define xmm3 recomp_runtime.xmm[3]
#define xmm4 recomp_runtime.xmm[4]
#define xmm5 recomp_runtime.xmm[5]
#define xmm6 recomp_runtime.xmm[6]
#define xmm7 recomp_runtime.xmm[7]

/* Packed transfers keep the guest side under the normal address translation,
   so bounds checks, the cached alias, and the APU aperture still apply. The
   register itself is a host local and is addressed as a host pointer. */
#define XMM_MEM(address) recomp_xmm_mem((uint32_t)(address))
#define XMM_STORE(address, reg) \
    recomp_guest_store((uint32_t)(address), &(reg), 16u)
#define XMM_LOAD_LOW(reg, address) \
    recomp_guest_load(&(reg).f[0], (uint32_t)(address), 8u)
#define XMM_STORE_LOW(address, reg) \
    recomp_guest_store((uint32_t)(address), &(reg).f[0], 8u)
#define XMM_LOAD_HIGH(reg, address) \
    recomp_guest_load(&(reg).f[2], (uint32_t)(address), 8u)
#define XMM_STORE_HIGH(address, reg) \
    recomp_guest_store((uint32_t)(address), &(reg).f[2], 8u)

static inline RecompXmm recomp_xmm_mem(uint32_t address)
{
    RecompXmm value;

    recomp_guest_load(&value, address, 16u);
    return value;
}

static inline RecompXmm XMM_ZERO(void)
{
    RecompXmm value;

    value.u[0] = 0u;
    value.u[1] = 0u;
    value.u[2] = 0u;
    value.u[3] = 0u;
    return value;
}

/* MOVSS from memory zeroes bits 127:32; MOVSS between registers does not. */
static inline RecompXmm XMM_SCALAR(float lane0)
{
    RecompXmm value = XMM_ZERO();

    value.f[0] = lane0;
    return value;
}

static inline RecompXmm XMM_SCALAR_BITS(uint32_t lane0)
{
    RecompXmm value = XMM_ZERO();

    value.u[0] = lane0;
    return value;
}

static inline RecompXmm XMM_SCALAR_DOUBLE(double lane0)
{
    RecompXmm value = XMM_ZERO();

    value.d[0] = lane0;
    return value;
}

#define RECOMP_XMM_LANEWISE(name, expression) \
    static inline RecompXmm name(RecompXmm a, RecompXmm b) \
    { \
        RecompXmm r; \
        int i; \
        for (i = 0; i < 4; ++i) { \
            expression; \
        } \
        return r; \
    }

RECOMP_XMM_LANEWISE(XMM_ADD, r.f[i] = a.f[i] + b.f[i])
RECOMP_XMM_LANEWISE(XMM_SUB, r.f[i] = a.f[i] - b.f[i])
RECOMP_XMM_LANEWISE(XMM_MUL, r.f[i] = a.f[i] * b.f[i])
RECOMP_XMM_LANEWISE(XMM_DIV, r.f[i] = a.f[i] / b.f[i])
/* MINPS/MAXPS return the second operand when either input is NaN. */
RECOMP_XMM_LANEWISE(XMM_MIN, r.f[i] = a.f[i] < b.f[i] ? a.f[i] : b.f[i])
RECOMP_XMM_LANEWISE(XMM_MAX, r.f[i] = a.f[i] > b.f[i] ? a.f[i] : b.f[i])
RECOMP_XMM_LANEWISE(XMM_AND, r.u[i] = a.u[i] & b.u[i])
RECOMP_XMM_LANEWISE(XMM_ANDN, r.u[i] = ~a.u[i] & b.u[i])
RECOMP_XMM_LANEWISE(XMM_OR, r.u[i] = a.u[i] | b.u[i])
RECOMP_XMM_LANEWISE(XMM_XOR, r.u[i] = a.u[i] ^ b.u[i])
RECOMP_XMM_LANEWISE(XMM_CMP_EQ, r.u[i] = a.f[i] == b.f[i] ? 0xffffffffu : 0u)
RECOMP_XMM_LANEWISE(XMM_CMP_LT, r.u[i] = a.f[i] < b.f[i] ? 0xffffffffu : 0u)
RECOMP_XMM_LANEWISE(XMM_CMP_LE, r.u[i] = a.f[i] <= b.f[i] ? 0xffffffffu : 0u)
RECOMP_XMM_LANEWISE(XMM_CMP_NEQ, r.u[i] = a.f[i] != b.f[i] ? 0xffffffffu : 0u)

/* CMPPS/CMPSS immediate: 0 EQ, 1 LT, 2 LE, 3 UNORD, 4 NEQ, 5 NLT, 6 NLE,
   7 ORD. The negated forms are true when either side is NaN. */
static inline int recomp_cmp_pred(float a, float b, int predicate)
{
    switch (predicate & 7) {
    case 0: return a == b;
    case 1: return a < b;
    case 2: return a <= b;
    case 3: return a != a || b != b;
    case 4: return !(a == b);
    case 5: return !(a < b);
    case 6: return !(a <= b);
    default: return !(a != a || b != b);
    }
}

static inline RecompXmm XMM_CMP_PRED(RecompXmm a, RecompXmm b, int predicate)
{
    RecompXmm r;

    for (int i = 0; i < 4; ++i) {
        r.u[i] = recomp_cmp_pred(a.f[i], b.f[i], predicate) ? 0xffffffffu : 0u;
    }
    return r;
}

/* SHUFPS takes the low half from the destination and the high half from the
   source. It is the broadcast in every matrix concatenation. */
static inline RecompXmm recomp_xmm_from_native(__m128 value)
{
    RecompXmm r;

    _mm_storeu_ps(r.f, value);
    return r;
}

/* SHUFPS carries an immediate byte; retain it for the native instruction. */
#define XMM_SHUFFLE(a, b, imm) \
    recomp_xmm_from_native(_mm_shuffle_ps( \
        _mm_loadu_ps((a).f), _mm_loadu_ps((b).f), (imm)))

static inline RecompXmm XMM_UNPACK_LOW(RecompXmm a, RecompXmm b)
{
    RecompXmm r;

    r.u[0] = a.u[0];
    r.u[1] = b.u[0];
    r.u[2] = a.u[1];
    r.u[3] = b.u[1];
    return r;
}

static inline RecompXmm XMM_UNPACK_HIGH(RecompXmm a, RecompXmm b)
{
    RecompXmm r;

    r.u[0] = a.u[2];
    r.u[1] = b.u[2];
    r.u[2] = a.u[3];
    r.u[3] = b.u[3];
    return r;
}

/* MOVLHPS: destination high half takes the source low half. */
static inline RecompXmm XMM_MOVE_LOW_TO_HIGH(RecompXmm a, RecompXmm b)
{
    RecompXmm r = a;

    r.u[2] = b.u[0];
    r.u[3] = b.u[1];
    return r;
}

/* MOVHLPS: destination low half takes the source high half. */
static inline RecompXmm XMM_MOVE_HIGH_TO_LOW(RecompXmm a, RecompXmm b)
{
    RecompXmm r = a;

    r.u[0] = b.u[2];
    r.u[1] = b.u[3];
    return r;
}

static inline uint32_t XMM_MOVEMASK(RecompXmm a)
{
    return ((a.u[0] >> 31) & 1u) | ((a.u[1] >> 30) & 2u) |
           ((a.u[2] >> 29) & 4u) | ((a.u[3] >> 28) & 8u);
}

/* ── MMX ────────────────────────────────────────────────────────────────
   The register file lives in runtime.h. Lane-wise ops ported from the
   lifter's runtime template, limited to the ones generated code emits. */
#define mm0 recomp_runtime.mmx[0]
#define mm1 recomp_runtime.mmx[1]
#define mm2 recomp_runtime.mmx[2]
#define mm3 recomp_runtime.mmx[3]
#define mm4 recomp_runtime.mmx[4]
#define mm5 recomp_runtime.mmx[5]
#define mm6 recomp_runtime.mmx[6]
#define mm7 recomp_runtime.mmx[7]

static inline RecompMmx MMX_MEM(uint32_t address)
{
    RecompMmx value;

    recomp_guest_load(&value, address, 8u);
    return value;
}

#define MMX_STORE(address, reg) \
    recomp_guest_store((uint32_t)(address), &(reg), 8u)

static inline int16_t recomp_sat_i16(int32_t v)
{
    return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

static inline uint8_t recomp_sat_u8(int32_t v)
{
    return (uint8_t)(v > 255 ? 255 : v < 0 ? 0 : v);
}

#define RECOMP_MMX_BINOP(name, lanes, field, expression) \
    static inline RecompMmx name(RecompMmx a, RecompMmx b) \
    { \
        RecompMmx r; \
        for (int i = 0; i < (lanes); ++i) { \
            r.field[i] = (expression); \
        } \
        return r; \
    }

RECOMP_MMX_BINOP(MMX_PADDW, 4, w, (int16_t)((uint16_t)a.w[i] + (uint16_t)b.w[i]))
RECOMP_MMX_BINOP(MMX_PSUBW, 4, w, (int16_t)((uint16_t)a.w[i] - (uint16_t)b.w[i]))
RECOMP_MMX_BINOP(MMX_PMULLW, 4, w, (int16_t)((int32_t)a.w[i] * b.w[i]))
RECOMP_MMX_BINOP(MMX_PMULHW, 4, w, (int16_t)(((int32_t)a.w[i] * b.w[i]) >> 16))
RECOMP_MMX_BINOP(MMX_PAVGB, 8, ub, (uint8_t)(((int32_t)a.ub[i] + b.ub[i] + 1) >> 1))

/* Shift counts at or past the lane width zero the logical shifts and fill
   the arithmetic ones with the sign. */
static inline RecompMmx MMX_PSLLW(RecompMmx a, uint64_t count)
{
    RecompMmx r;

    for (int i = 0; i < 4; ++i) {
        r.uw[i] = count >= 16u ? 0u : (uint16_t)(a.uw[i] << count);
    }
    return r;
}

static inline RecompMmx MMX_PSRAW(RecompMmx a, uint64_t count)
{
    RecompMmx r;

    for (int i = 0; i < 4; ++i) {
        r.w[i] = (int16_t)(a.w[i] >> (count >= 16u ? 15u : count));
    }
    return r;
}

static inline RecompMmx MMX_PSRAD(RecompMmx a, uint64_t count)
{
    RecompMmx r;

    for (int i = 0; i < 2; ++i) {
        r.d[i] = a.d[i] >> (count >= 32u ? 31u : count);
    }
    return r;
}

static inline RecompMmx MMX_PUNPCKLWD(RecompMmx a, RecompMmx b)
{
    RecompMmx r;

    for (int i = 0; i < 2; ++i) {
        r.uw[i * 2] = a.uw[i];
        r.uw[i * 2 + 1] = b.uw[i];
    }
    return r;
}

static inline RecompMmx MMX_PUNPCKHWD(RecompMmx a, RecompMmx b)
{
    RecompMmx r;

    for (int i = 0; i < 2; ++i) {
        r.uw[i * 2] = a.uw[i + 2];
        r.uw[i * 2 + 1] = b.uw[i + 2];
    }
    return r;
}

static inline RecompMmx MMX_PACKUSWB(RecompMmx a, RecompMmx b)
{
    RecompMmx r;

    for (int i = 0; i < 4; ++i) {
        r.ub[i] = recomp_sat_u8(a.w[i]);
        r.ub[i + 4] = recomp_sat_u8(b.w[i]);
    }
    return r;
}

static inline RecompMmx MMX_PACKSSDW(RecompMmx a, RecompMmx b)
{
    RecompMmx r;

    for (int i = 0; i < 2; ++i) {
        r.w[i] = recomp_sat_i16(a.d[i]);
        r.w[i + 2] = recomp_sat_i16(b.d[i]);
    }
    return r;
}

static inline uint32_t MMX_PEXTRW(RecompMmx a, uint32_t imm)
{
    return a.uw[imm & 3u];
}

#endif
