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

# The swimsuit switch: DOAXBV swimsuits built from the player's DOAXBV disc
# (an .iso or its extracted folder). On unless the launcher turned it off.
$swimsuits = $true
try { $saved = Get-Content (Join-Path $root 'private\launcher.json') -Raw -ErrorAction Stop | ConvertFrom-Json
      if ($null -ne $saved.swimsuits) { $swimsuits = [bool]$saved.swimsuits } } catch {}
if ($swimsuits -and -not $env:RECOMP_DOAXBV_DISC) {
    $doaxbvFile = Join-Path $root 'private\doaxbv-path.txt'
    if (Test-Path $doaxbvFile) { $env:RECOMP_DOAXBV_DISC = (Get-Content $doaxbvFile -TotalCount 1).Trim() }
    else {
        $image = Get-ChildItem (Join-Path $root 'private') -Filter *.iso -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match 'xtreme|doax|beach' } | Select-Object -First 1
        if ($image) { $env:RECOMP_DOAXBV_DISC = $image.FullName }
    }
}
$packed = Join-Path $root 'private\doaxbv-suits.pack.gz'
if (-not (Test-Path $packed)) { $packed = Join-Path $root 'recipes\doaxbv-suits.pack.gz' }
$pack = Join-Path $root 'private\doaxbv-suits.pack'
if ($swimsuits -and $env:RECOMP_DOAXBV_DISC -and (Test-Path $packed)) {
    if (-not (Test-Path $pack) -or (Get-Item $pack).LastWriteTime -lt (Get-Item $packed).LastWriteTime) {
        $in = [IO.File]::OpenRead($packed); $out = [IO.File]::Create("$pack.tmp")
        $gz = New-Object IO.Compression.GZipStream($in, [IO.Compression.CompressionMode]::Decompress)
        $gz.CopyTo($out); $gz.Dispose(); $out.Dispose(); $in.Dispose()
        Move-Item -Force "$pack.tmp" $pack
    }
    $env:RECOMP_COSTUME_PACK = $pack
    if ($saved -and $saved.swimsuit_start_favorite) { $env:RECOMP_COSTUME_START_FAVORITE = '1' }
}

# Test-harness settings would speed the game up or stop it after a timeout.
Remove-Item Env:RECOMP_UNPACED, Env:RECOMP_WATCHDOG_MS -ErrorAction SilentlyContinue
$logs = Join-Path $root 'private\play-logs'
New-Item -ItemType Directory -Force $logs | Out-Null
$log = Join-Path $logs ((Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
# Start-Process joins arguments with spaces, so quote a path that may contain them.
$runnerArgs = @('--xbe', ('"' + (Join-Path $disc 'default.xbe') + '"'))
if ($VSync) { $runnerArgs += '--vsync' }
$p = Start-Process -FilePath $exe -ArgumentList $runnerArgs -WorkingDirectory $root -RedirectStandardError $log -RedirectStandardOutput "$log.out" -PassThru -Wait
"exit=$($p.ExitCode) log=$log"
exit $p.ExitCode
