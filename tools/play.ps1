# Play DOA3 on the local Release build: pwsh tools/play.ps1 [-Build] [-VSync]
param([switch]$Build, [switch]$VSync)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$buildDir = Join-Path $root 'build\recomp-program'
$exe = Join-Path $buildDir 'Release\recomp_program_runner.exe'
if ($Build) {
    cmake --build $buildDir --config Release --target recomp_program_runner
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
if (-not (Test-Path $exe)) {
    throw "No Release runner at $exe. Configure build\recomp-program (docs/bring-up.md), then pass -Build."
}

# A hard-linked view of the extracted disc keeps the game's cache and saves
# (.recomp-storage) out of the source disc. The first boots fill the cache.
# BuildGame.cmd records the disc it built from; copies cover other drives.
$pathFile = Join-Path $root 'private\disc-path.txt'
$source = if (Test-Path $pathFile) { (Get-Content $pathFile -TotalCount 1).Trim() }
          else { Join-Path $root 'private\imported-disc\disc' }
$source = (Resolve-Path -LiteralPath $source).Path.TrimEnd('\')
$disc = Join-Path $root 'private\play-disc'
if (-not (Test-Path $disc)) {
    Get-ChildItem -LiteralPath $source -File -Recurse | ForEach-Object {
        $file = $_.FullName
        $relative = $file.Substring($source.Length + 1)
        if ($relative -like '.recomp-storage\*') { return }
        $target = Join-Path $disc $relative
        New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
        try { New-Item -ItemType HardLink -Path $target -Target $file -ErrorAction Stop | Out-Null }
        catch { Copy-Item -LiteralPath $file -Destination $target }
    }
}

# Test-harness settings would speed the game up or stop it after a timeout.
Remove-Item Env:RECOMP_UNPACED, Env:RECOMP_WATCHDOG_MS -ErrorAction SilentlyContinue
$logs = Join-Path $root 'private\play-logs'
New-Item -ItemType Directory -Force $logs | Out-Null
$log = Join-Path $logs ((Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
$runnerArgs = @('--xbe', (Join-Path $disc 'default.xbe'))
if ($VSync) { $runnerArgs += '--vsync' }
$p = Start-Process -FilePath $exe -ArgumentList $runnerArgs -WorkingDirectory $root -RedirectStandardError $log -RedirectStandardOutput "$log.out" -PassThru -Wait
"exit=$($p.ExitCode) log=$log"
