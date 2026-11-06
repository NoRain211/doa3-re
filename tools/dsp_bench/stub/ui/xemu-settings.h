#pragma once
#include <stdbool.h>
struct bench_config { struct { bool use_dsp_jit; } audio; };
static const struct bench_config g_config = {{false}};

