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
    # Fill a staging folder so an interrupted copy is never taken as complete.
    $staging = "$disc.tmp"
    if (Test-Path $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
    Get-ChildItem -LiteralPath $source -File -Recurse | ForEach-Object {
        $file = $_.FullName
        $relative = $file.Substring($source.Length + 1)
        if ($relative -like '.recomp-storage\*') { return }
        $target = Join-Path $staging $relative
        New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
        try { New-Item -ItemType HardLink -Path $target -Target $file -ErrorAction Stop | Out-Null }
        catch { Copy-Item -LiteralPath $file -Destination $target }
    }
    Rename-Item -LiteralPath $staging -NewName (Split-Path $disc -Leaf)
}

# Test-harness settings would speed the game up or stop it after a timeout.
Remove-Item Env:RECOMP_UNPACED, Env:RECOMP_WATCHDOG_MS -ErrorAction SilentlyContinue
$logs = Join-Path $root 'private\play-logs'
New-Item -ItemType Directory -Force $logs | Out-Null
$log = Join-Path $logs ((Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
# Win32_Process.Create detaches the runner from this shell's job; a launcher or
# agent shell that exits otherwise sends the game window a close request.
# It starts from the WMI service's environment, so pass ours (the launcher's
# RECOMP_* settings) explicitly.
$vars = [string[]](Get-ChildItem Env: | ForEach-Object { "$($_.Name)=$($_.Value)" })
$startup = New-CimInstance -ClassName Win32_ProcessStartup -ClientOnly -Property @{ EnvironmentVariables = $vars }
$cmd = 'cmd /c ""{0}" --xbe "{1}"{2} 2>"{3}" >"{3}.out""' -f $exe, (Join-Path $disc 'default.xbe'), $(if ($VSync) { ' --vsync' } else { '' }), $log
$res = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd; CurrentDirectory = $root; ProcessStartupInformation = $startup }
if ($res.ReturnValue) { throw "Could not start the runner (Win32_Process.Create returned $($res.ReturnValue))." }
"pid=$($res.ProcessId) log=$log"
# Detached, so the exit code is lost; a runner gone within seconds failed to start.
Start-Sleep -Seconds 5
if (-not (Get-Process -Id $res.ProcessId -ErrorAction SilentlyContinue)) {
    Get-Content -LiteralPath $log -Tail 20 -ErrorAction SilentlyContinue
    Write-Error "The runner exited during startup; see $log" -ErrorAction Continue
    exit 1
}
