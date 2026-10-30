#pragma once
#include <stdio.h>
#include <stdint.h>
/* The bench switches xemu's instruction trace on per run. */
extern int bench_trace;
#define TRACE_DSP56K_EXECUTE_INSTRUCTION_DISASM 0
#define trace_event_get_state(x) (bench_trace)
#define trace_dsp56k_execute_instruction_disasm(text) fprintf(stderr, "%s\n", text)
#define trace_dsp56k_execute_instruction(...) ((void)0)
#define trace_dsp_read_peripheral(a, v) ((void)0)
#define trace_dsp_write_peripheral(a, v) ((void)0)

