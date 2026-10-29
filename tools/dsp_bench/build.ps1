# Builds the GP DSP bench: xemu's DSP interpreter (fetched at a pinned
# commit, never vendored) plus the runtime's reverb, into private/dsp-bench.
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot)
$out = Join-Path $root 'private/dsp-bench'
# xemu v0.8.139, the release the snapshots were taken with.
$commit = 'aa54c34cae1c09410d74c69a44210a9867fe2d16'
$sources = 'dsp/dsp.c', 'dsp/dsp.h', 'dsp/dsp_c.c', 'dsp/dsp_dma.c', 'dsp/dsp_dma.h',
    'dsp/dsp_dma_regs.h', 'dsp/dsp_internal.h', 'dsp/debug.h', 'dsp/interp/dsp_cpu.c',
    'dsp/interp/dsp_cpu.h', 'dsp/interp/dsp_cpu_regs.h', 'dsp/interp/dsp_dis.c.inc',
    'dsp/interp/dsp_emu.c.inc'
$xemu = Join-Path $out "xemu-$($commit.Substring(0, 7))"
foreach ($file in $sources) {
    $target = Join-Path $xemu $file
    if (Test-Path $target) { continue }
    New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
    curl.exe -fsSL --remove-on-error -o $target "https://raw.githubusercontent.com/xemu-project/xemu/$commit/hw/xbox/mcpx/apu/$file"
    if ($LASTEXITCODE) { throw "Could not fetch $file" }
}
$stub = Join-Path $PSScriptRoot 'stub'
# xemu's sources come in as system headers, so their warnings stay quiet and the bench's own show.
gcc -O2 -std=gnu11 -Wall "-I$stub" -isystem "$xemu/dsp" -isystem "$xemu/dsp/interp" -o "$out/dspbench.exe" "$PSScriptRoot/dspbench.c" -lm
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$runtime = Join-Path $root 'recomp-runtime'
gcc -O2 -std=gnu11 "-I$runtime" -o "$out/reverb_ir.exe" "$PSScriptRoot/reverb_ir.c" "$runtime/dsound_reverb.c" -lm
if ($LASTEXITCODE) { exit $LASTEXITCODE }
"built $out/dspbench.exe and reverb_ir.exe"
