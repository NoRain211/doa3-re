/* Runs a snapshot of the MCPX GP DSP (snapshot.py) in xemu's DSP interpreter.
   build.ps1 fetches xemu's DSP sources at a pinned commit; they are compiled
   into this file so the bench can reach the interpreter's static disassembler. */
#include "qemu/osdep.h"
#include "dsp.c"
#include "dsp_c.c"
#include "dsp_dma.c"
#include "interp/dsp_cpu.c"
#include <math.h>

const DSPOps jit_dsp_ops;
void dsp_jit_init(DSPState *dsp) { (void)dsp; abort(); }
int bench_trace;

static uint8_t *scratch;
static size_t scratch_len;
static uint32_t snap_p[4096], snap_x[4096], snap_y[2048], snap_mix[1024];

static void scratch_rw(void *opaque, uint8_t *ptr, uint32_t addr, size_t len, bool dir)
{
    (void)opaque;
    if ((size_t)addr + len > scratch_len) {
        fprintf(stderr, "scratch access %08x+%zx outside %zx\n", addr, len, scratch_len);
        exit(3);
    }
    if (dir) memcpy(scratch + addr, ptr, len);
    else memcpy(ptr, scratch + addr, len);
}

static void load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(2); }
    uint32_t hdr;
    if (fread(&hdr, 4, 1, f) != 1 || fseek(f, hdr, SEEK_CUR) ||
        fread(snap_p, 4, 4096, f) != 4096 || fread(snap_x, 4, 4096, f) != 4096 ||
        fread(snap_y, 4, 2048, f) != 2048 || fread(snap_mix, 4, 1024, f) != 1024) {
        fprintf(stderr, "%s: short snapshot\n", path); exit(2);
    }
    long at = ftell(f); fseek(f, 0, SEEK_END); scratch_len = (size_t)(ftell(f) - at); fseek(f, at, SEEK_SET);
    scratch = malloc(scratch_len);
    if (fread(scratch, 1, scratch_len, f) != scratch_len) { fprintf(stderr, "short scratch\n"); exit(2); }
    fclose(f);
}

static void fifo_record(void *opaque, uint8_t *ptr, unsigned int index, size_t len, bool dir);

static DSPState *make_dsp(void)
{
    DSPState *dsp = dsp_init(NULL, scratch_rw, fifo_record, true);
    for (uint32_t i = 0; i < 4096; ++i) dsp_write_memory(dsp, 'P', i, snap_p[i] & 0xffffff);
    for (uint32_t i = 0; i < 4096; ++i) dsp_write_memory(dsp, 'X', i, snap_x[i] & 0xffffff);
    for (uint32_t i = 0; i < 2048; ++i) dsp_write_memory(dsp, 'Y', i, snap_y[i] & 0xffffff);
    for (uint32_t i = 0; i < 1024; ++i) dsp_write_memory(dsp, 'X', 0x1400 + i, snap_mix[i] & 0xffffff);
    return dsp;
}

static void disasm(DSPState *dsp, uint32_t from, uint32_t to)
{
    dsp_core_t *core = c_core(dsp);
    for (uint32_t pc = from; pc < to;) {
        core->pc = pc;
        core->disasm_prev_inst_pc = 0xffffffff;
        uint16_t len = disasm_instruction(core, DSP_DISASM_MODE);
        printf("%s\n", disasm_get_instruction_text(core));
        pc += len ? len : 1;
    }
}

/* One APU frame as xemu's mcpx_apu_dsp_frame runs it. Returns cycles used. */
static uint32_t frame(DSPState *dsp)
{
    dsp_start_frame(dsp);
    dsp_set_halt_requested(dsp, false);
    dsp_set_cycle_count(dsp, 0);
    int guard = 0;
    do {
        dsp_run(dsp, 1000);
        if (++guard > 2000) { fprintf(stderr, "frame did not halt (pc=%04x)\n", c_core(dsp)->pc); exit(4); }
    } while (!dsp_get_halt_requested(dsp));
    return dsp_get_cycle_count(dsp);
}


static size_t fifo_len[4];

/* The I3DL2 reverb writes mix bins only; count FIFO output to show if it ever doesn't. */
static void fifo_record(void *opaque, uint8_t *ptr, unsigned int index, size_t len, bool dir)
{
    (void)opaque;
    if (!dir) { memset(ptr, 0, len); return; }
    if (index < 4) fifo_len[index] += len / 4;
}

static float word_to_float(uint32_t w) { return (float)((int32_t)(w << 8) >> 8) / 8388608.0f; }
static uint32_t float_to_word(float f)
{
    double v = f * 8388608.0; if (v > 8388607.0) v = 8388607.0; if (v < -8388608.0) v = -8388608.0;
    return (uint32_t)(int32_t)lrint(v) & 0xffffff;
}

/* Writes all 32 input bins (one sample each per call site), runs a frame, and
   appends the 32x32 mix buffer to out. */
static void impulse(DSPState *dsp, int bin, int warm, int frames, float amp, const char *path)
{
    FILE *out = fopen(path, "wb");
    if (!out) { perror(path); exit(2); }
    c_core(dsp)->pc = 0x2b;
    for (int n = -warm; n < frames; ++n) {
        for (int b = 0; b < 32; ++b)
            for (int s = 0; s < 32; ++s)
                dsp_write_memory(dsp, 'X', 0x1400 + b * 32 + s, float_to_word(n == 0 && s == 0 && b == bin ? amp : 0.0f));
        frame(dsp);
        if (n < 0) continue;
        float mix[1024];
        for (int i = 0; i < 1024; ++i) mix[i] = word_to_float(dsp_read_memory(dsp, 'X', 0x1400 + i));
        fwrite(mix, sizeof mix, 1, out);
    }
    fclose(out);
    for (int i = 0; i < 4; ++i) if (fifo_len[i]) fprintf(stderr, "fifo %d: %zu words\n", i, fifo_len[i]);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: dspbench disasm <snap> <from> <to>\n"
                        "       dspbench trace <snap> <pc> <frames>\n"
                        "       dspbench impulse <snap> <bin> <warm frames> <frames> <amp> <out.f32>\n");
        return 1;
    }
    load(argv[2]);
    DSPState *dsp = make_dsp();
    if (!strcmp(argv[1], "disasm") && argc == 5) {
        disasm(dsp, strtoul(argv[3], NULL, 16), strtoul(argv[4], NULL, 16));
    } else if (!strcmp(argv[1], "trace") && argc == 5) {
        c_core(dsp)->pc = strtoul(argv[3], NULL, 16);
        bench_trace = 1;
        for (int n = atoi(argv[4]); n > 0; --n) fprintf(stderr, "frame cycles=%u\n", frame(dsp));
    } else if (!strcmp(argv[1], "impulse") && argc == 8) {
        impulse(dsp, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), (float)atof(argv[6]), argv[7]);
    } else {
        return 1;
    }
    return 0;
}
