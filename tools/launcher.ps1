# SPDX-License-Identifier: GPL-3.0-or-later
# Pick resolution, anti-aliasing, widescreen and volume, remember them, then start Play.cmd.
# The window redraws DOA3's Options screen with GDI+; no retail art is used.
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
# The console is hidden, so show any failure instead of exiting silently.
$ErrorActionPreference = 'Stop'
trap { [Windows.Forms.MessageBox]::Show("$_", 'DOA3 launcher') | Out-Null; exit 1 }
$root = Split-Path $PSScriptRoot -Parent
$settingsPath = Join-Path $root 'private\launcher.json'
$heights = 480, 720, 1080, 1440, 2160
$samples = 1, 2, 4, 8
$volumes = 0, 25, 50, 75, 100
$settings = [pscustomobject]@{ height = [Windows.Forms.Screen]::PrimaryScreen.Bounds.Height; msaa = 1; smaa = $false; widescreen = $false; volume = 100 }
try { $settings = Get-Content $settingsPath -Raw -ErrorAction Stop | ConvertFrom-Json } catch {}

# PowerShell 7 loads JSON numbers as Int64, so cast; a missing volume means 100%.
$savedVolume = if ($null -ne $settings.volume) { [Array]::IndexOf($volumes, [int]$settings.volume) } else { -1 }
if ($savedVolume -lt 0) { $savedVolume = $volumes.Count - 1 }
$nearestHeight = { param($h) [Math]::Max(0, @($heights | Where-Object { $_ -le $h }).Count - 1) }
$resolution = @{ label = 'Resolution'; info = 'Sets the render resolution. 480p opens in a window'
    names = [string[]]($heights | ForEach-Object { "$($_)p" }); index = & $nearestHeight ([int]$settings.height)
    default = & $nearestHeight ([Windows.Forms.Screen]::PrimaryScreen.Bounds.Height) }
$msaa = @{ label = 'MSAA'; info = 'Smooths polygon edges with multisampling'; default = 0
    names = [string[]]($samples | ForEach-Object { if ($_ -eq 1) { 'Off' } else { "$($_)x" } }); index = [Math]::Max(0, [Array]::IndexOf($samples, [int]$settings.msaa)) }
$smaa = @{ label = 'SMAA'; info = 'Smooths edges with a post-process filter'; names = 'Off', 'On'; index = [int][bool]$settings.smaa; default = 0 }
$screen = @{ label = 'Screen'; info = 'Wider 3D view. The HUD and menus stay 4:3'; names = '4:3', '16:9'; index = [int][bool]$settings.widescreen; default = 0; boxes = $true }
$volume = @{ label = 'Volume'; info = 'Sets the master volume'; names = [string[]]$volumes; index = $savedVolume; default = $volumes.Count - 1; bar = $true }
function Page-End {
    @{ label = 'Default Setting'; info = 'Restores the defaults on this page'; action = 'default' }
    @{ label = 'Exit'; info = 'Returns to the menu'; action = 'back' }
}
$pages = @(
    @{ name = 'VIDEO SETTINGS'; info = 'Sets resolution, anti-aliasing and screen shape'; rows = @($resolution, $msaa, $smaa, $screen) + (Page-End) }
    @{ name = 'AUDIO SETTINGS'; info = 'Sets the master volume'; rows = @($volume) + (Page-End) }
    @{ name = 'START GAME'; info = 'Saves these settings and starts the game'; rows = @(); action = 'play' }
    @{ name = 'EXIT'; info = 'Closes the launcher'; rows = @(); action = 'exit' }
)
# The menu cursor picks a page; inside a page the row cursor picks a setting.
$script:menu = 2; $script:row = 0; $script:inPage = $false

# Layout of a 640x480 screen, matching the game's own proportions.
$top = 84; $step = 30; $barH = 24
$labelX = 226; $labelW = 176; $valueX = 406; $valueW = 176
function RowY($i) { $top + $i * $step }
function Rgb($r, $g, $b, $a = 255) { [Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Gradient($x, $y, $w, $h, $c1, $c2) { New-Object Drawing.Drawing2D.LinearGradientBrush ([Drawing.RectangleF]::new($x, $y, $w, $h)), $c1, $c2, 90.0 }

$itemFont = New-Object Drawing.Font 'Arial', 15, ([Drawing.FontStyle]'Bold, Italic'), ([Drawing.GraphicsUnit]::Pixel)
$infoFont = New-Object Drawing.Font 'Arial', 16, ([Drawing.FontStyle]'Bold, Italic'), ([Drawing.GraphicsUnit]::Pixel)
$titleFont = New-Object Drawing.Font 'Georgia', 34, ([Drawing.FontStyle]::Bold), ([Drawing.GraphicsUnit]::Pixel)
$logoFont = New-Object Drawing.Font 'Arial Black', 34, ([Drawing.FontStyle]::Italic), ([Drawing.GraphicsUnit]::Pixel)
$far = New-Object Drawing.StringFormat -Property @{ Alignment = 'Far'; LineAlignment = 'Center' }
$near = New-Object Drawing.StringFormat -Property @{ Alignment = 'Near'; LineAlignment = 'Center' }
$center = New-Object Drawing.StringFormat -Property @{ Alignment = 'Center'; LineAlignment = 'Center' }

# Dim olive arena behind chain-link fence, rendered once.
$background = New-Object Drawing.Bitmap 640, 480
$bg = [Drawing.Graphics]::FromImage($background)
$bg.SmoothingMode = 'AntiAlias'
$glow = New-Object Drawing.Drawing2D.GraphicsPath
$glow.AddEllipse(-160, -120, 960, 680)
$light = New-Object Drawing.Drawing2D.PathGradientBrush $glow
$light.CenterPoint = [Drawing.PointF]::new(380, 210)
$light.CenterColor = Rgb 92 100 72
$light.SurroundColors = @(Rgb 10 12 9)
$bg.Clear((Rgb 10 12 9))
$bg.FillPath($light, $glow)
$wire = New-Object Drawing.Pen (Rgb 200 210 180 45), 1
for ($x = -480; $x -lt 640; $x += 18) { $bg.DrawLine($wire, $x, 60, $x + 420, 480); $bg.DrawLine($wire, $x + 420, 60, $x, 480) }
$rail = New-Object Drawing.Pen (Rgb 150 160 130 40), 4
foreach ($y in 96, 250, 392) { $bg.DrawLine($rail, 0, $y, 640, $y) }
foreach ($x in 120, 330, 540) { $bg.DrawLine($rail, $x, 60, $x, 420) }
$bg.FillRectangle((Gradient 0 380 640 100 (Rgb 0 0 0 0) (Rgb 0 0 0 220)), 0, 380, 640, 100)
$bg.Dispose()

function Draw-GlowText($g, $text, $font, $color, $rect, $format) {
    $halo = New-Object Drawing.SolidBrush (Rgb 70 150 255 70)
    foreach ($d in -2, -1, 1, 2) {
        $g.DrawString($text, $font, $halo, [Drawing.RectangleF]::new($rect.X + $d, $rect.Y, $rect.Width, $rect.Height), $format)
        $g.DrawString($text, $font, $halo, [Drawing.RectangleF]::new($rect.X, $rect.Y + $d, $rect.Width, $rect.Height), $format)
    }
    $g.DrawString($text, $font, (New-Object Drawing.SolidBrush $color), $rect, $format)
}

function Draw-Bar($g, $x, $y, $w, $selected) {
    $brush = if ($selected) { Gradient $x $y $w $barH (Rgb 92 140 228) (Rgb 38 82 176) } else { Gradient $x $y $w $barH (Rgb 74 74 74 230) (Rgb 34 34 36 230) }
    $g.FillRectangle($brush, $x, $y, $w, $barH)
    $g.DrawLine((New-Object Drawing.Pen (Rgb 255 255 255 50)), $x, $y, $x + $w, $y)
}

function Draw-Arrow($g, $x, $y, $dir) {
    $cy = $y + $barH / 2
    $tip = [Drawing.PointF]::new($x + 7 * $dir, $cy)
    $g.FillPolygon([Drawing.Brushes]::White, [Drawing.PointF[]]@($tip, [Drawing.PointF]::new($x, $cy - 5), [Drawing.PointF]::new($x, $cy + 5)))
}

function Draw-Screen($g) {
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAlias'
    $g.DrawImage($background, 0, 0)

    # Header band with the ghosted logo behind the screen title.
    $band = [Drawing.PointF[]]@([Drawing.PointF]::new(0, 0), [Drawing.PointF]::new(640, 0), [Drawing.PointF]::new(640, 66),
        [Drawing.PointF]::new(292, 66), [Drawing.PointF]::new(276, 52), [Drawing.PointF]::new(0, 52))
    $g.FillPolygon((Gradient 0 0 640 66 (Rgb 0 0 0) (Rgb 14 14 14)), $band)
    $streak = New-Object Drawing.Pen (Rgb 255 255 255 18), 1
    for ($y = 4; $y -lt 30; $y += 3) { $g.DrawLine($streak, 280, $y, 640, $y) }
    $g.DrawString('DEAD OR ALIVE 3', $logoFont, (New-Object Drawing.SolidBrush (Rgb 120 120 120 60)), [Drawing.RectangleF]::new(240, 4, 395, 52), $far)
    $silver = Gradient 0 14 640 40 (Rgb 255 255 255) (Rgb 150 150 150)
    $g.DrawString('OPTIONS', $titleFont, (New-Object Drawing.SolidBrush (Rgb 0 0 0 160)), [Drawing.RectangleF]::new(2, 14, 620, 44), $far)
    $g.DrawString('OPTIONS', $titleFont, $silver, [Drawing.RectangleF]::new(0, 12, 620, 44), $far)

    # Menu column on the left.
    for ($m = 0; $m -lt $pages.Count; $m++) {
        $y = RowY $m
        $g.FillRectangle((New-Object Drawing.SolidBrush (Rgb 0 0 0 150)), 0, $y, 214, $barH)
        $rect = [Drawing.RectangleF]::new(0, $y, 206, $barH)
        if ($m -eq $script:menu) { Draw-GlowText $g $pages[$m].name $itemFont (Rgb 255 255 255) $rect $far }
        else { $g.DrawString($pages[$m].name, $itemFont, (New-Object Drawing.SolidBrush (Rgb 175 175 175)), $rect, $far) }
    }
    $g.DrawLine((New-Object Drawing.Pen (Rgb 200 200 200 160), 2), 218, $top - 4, 218, (RowY 6) - 2)

    # The chosen page's rows: label bar, then the value. From the menu they show as a preview.
    $rows = $pages[$script:menu].rows
    for ($i = 0; $i -lt $rows.Count; $i++) {
        $row = $rows[$i]; $y = RowY $i; $selected = $script:inPage -and $i -eq $script:row
        Draw-Bar $g $labelX $y $labelW $selected
        $textColor = if ($selected) { Rgb 255 255 255 } else { Rgb 150 150 150 }
        $g.DrawString($row.label, $itemFont, (New-Object Drawing.SolidBrush $textColor), [Drawing.RectangleF]::new($labelX, $y, $labelW - 8, $barH), $far)
        if (-not $row.names) { continue }
        if ($row.boxes) {
            for ($b = 0; $b -lt $row.names.Count; $b++) {
                $box = [Drawing.Rectangle]::new($valueX + 8 + $b * 62, $y, 52, $barH)
                $fill = if ($b -eq $row.index) { Gradient $box.X $y 52 $barH (Rgb 100 150 240) (Rgb 40 84 180) } else { New-Object Drawing.SolidBrush (Rgb 0 0 0 220) }
                $g.FillRectangle($fill, $box)
                $g.DrawRectangle((New-Object Drawing.Pen (Rgb 230 230 230), 2), $box)
                $g.DrawString($row.names[$b], $itemFont, [Drawing.Brushes]::White, [Drawing.RectangleF]$box, $center)
            }
            continue
        }
        $g.FillRectangle((New-Object Drawing.SolidBrush (Rgb 22 22 26 200)), $valueX, $y, $valueW, $barH)
        $valueColor = Rgb 190 180 255
        if ($row.bar) {
            $g.DrawString($row.names[$row.index], $itemFont, (New-Object Drawing.SolidBrush $valueColor), [Drawing.RectangleF]::new($valueX + 20, $y, 40, $barH), $far)
            $meter = [Drawing.Rectangle]::new($valueX + 68, $y + 5, $valueW - 88, $barH - 10)
            $g.FillRectangle((New-Object Drawing.SolidBrush (Rgb 20 20 40)), $meter)
            $fillW = [int]($meter.Width * $volumes[$row.index] / 100)
            if ($fillW -gt 0) { $g.FillRectangle((Gradient $meter.X $meter.Y $fillW $meter.Height (Rgb 255 255 255) (Rgb 170 170 190)), $meter.X, $meter.Y, $fillW, $meter.Height) }
            $g.DrawRectangle((New-Object Drawing.Pen (Rgb 200 200 220)), $meter)
        } else {
            $g.DrawString($row.names[$row.index], $itemFont, (New-Object Drawing.SolidBrush $valueColor), [Drawing.RectangleF]::new($valueX + 26, $y, $valueW - 40, $barH), $near)
        }
        if ($selected) { Draw-Arrow $g ($valueX + 14) $y -1; Draw-Arrow $g ($valueX + $valueW - 12) $y 1 }
    }

    # Button prompts.
    foreach ($p in @(@{ x = 372; text = 'SELECT'; key = 'A'; c1 = Rgb 220 255 120; c2 = Rgb 70 160 20 }, @{ x = 500; text = 'BACK'; key = 'B'; c1 = Rgb 255 150 130; c2 = Rgb 180 20 20 })) {
        $g.DrawString($p.text, $itemFont, (New-Object Drawing.SolidBrush (Rgb 210 210 255)), [Drawing.RectangleF]::new($p.x, 384, 80, 24), $far)
        $g.FillEllipse((Gradient ($p.x + 86) 384 22 22 $p.c1 $p.c2), $p.x + 86, 384, 22, 22)
        $g.DrawEllipse((New-Object Drawing.Pen (Rgb 40 40 40), 2), $p.x + 86, 384, 22, 22)
        $g.DrawString($p.key, $itemFont, [Drawing.Brushes]::White, [Drawing.RectangleF]::new($p.x + 86, 384, 22, 22), $center)
    }

    # Information strip.
    $g.FillRectangle((New-Object Drawing.SolidBrush (Rgb 0 0 0 200)), 0, 424, 640, 26)
    $tab = [Drawing.PointF[]]@([Drawing.PointF]::new(0, 426), [Drawing.PointF]::new(134, 426), [Drawing.PointF]::new(148, 448), [Drawing.PointF]::new(0, 448))
    $g.FillPolygon((New-Object Drawing.Drawing2D.LinearGradientBrush ([Drawing.PointF]::new(0, 0)), ([Drawing.PointF]::new(148, 0)), (Rgb 120 120 120), (Rgb 245 245 245)), $tab)
    $g.DrawString('information', $infoFont, (New-Object Drawing.SolidBrush (Rgb 30 30 30)), [Drawing.RectangleF]::new(0, 426, 136, 22), $far)
    $info = if ($script:inPage) { $rows[$script:row].info } else { $pages[$script:menu].info }
    $g.DrawString($info, $infoFont, [Drawing.Brushes]::White, [Drawing.RectangleF]::new(156, 426, 480, 22), $near)
}

$form = New-Object Windows.Forms.Form -Property @{ Text = 'Dead or Alive 3'; FormBorderStyle = 'None'; StartPosition = 'CenterScreen'; KeyPreview = $true; BackColor = 'Black' }
$form.ClientSize = New-Object Drawing.Size 640, 480
# Form.DoubleBuffered is protected; set it to stop flicker on every redraw.
$form.GetType().GetProperty('DoubleBuffered', [Reflection.BindingFlags]'NonPublic, Instance').SetValue($form, $true)
$form.Add_Paint({ Draw-Screen $_.Graphics })

function Clamp($value, $max) { [Math]::Min([Math]::Max($value, 0), $max) }
function Page-Rows { $pages[$script:menu].rows }
function Move-Cursor($delta) {
    if ($script:inPage) { $script:row = Clamp ($script:row + $delta) ((Page-Rows).Count - 1) }
    else { $script:menu = Clamp ($script:menu + $delta) ($pages.Count - 1) }
    $form.Invalidate()
}
function Change-Value($delta) {
    if (-not $script:inPage) { return }
    $row = (Page-Rows)[$script:row]
    if ($row.names) { $row.index = Clamp ($row.index + $delta) ($row.names.Count - 1); $form.Invalidate() }
}
function Accept {
    $page = $pages[$script:menu]
    if (-not $script:inPage) {
        if ($page.action) { $form.Tag = $page.action; $form.Close(); return }
        $script:inPage = $true; $script:row = 0
    } else {
        $row = $page.rows[$script:row]
        if ($row.action -eq 'default') { foreach ($r in $page.rows) { if ($r.names) { $r.index = $r.default } } }
        elseif ($row.action -eq 'back') { $script:inPage = $false }
    }
    $form.Invalidate()
}
function Back { if ($script:inPage) { $script:inPage = $false; $form.Invalidate() } else { $form.Close() } }
function Hit-Row($y) { $i = [Math]::Floor(($y - $top) / $step); if ($i -ge 0 -and ($y - (RowY $i)) -lt $barH) { $i } else { -1 } }

$form.Add_KeyDown({
    switch ($_.KeyCode) {
        'Up' { Move-Cursor -1 }
        'Down' { Move-Cursor 1 }
        'Left' { if ($script:inPage) { Change-Value -1 } }
        'Right' { if ($script:inPage) { Change-Value 1 } elseif ((Page-Rows).Count) { Accept } }
        'Return' { Accept }
        'Escape' { Back }
        'Back' { Back }
    }
})
$form.Add_MouseMove({
    $i = Hit-Row $_.Y
    if ($i -lt 0) { return }
    if ($_.X -lt 214 -and -not $script:inPage -and $i -lt $pages.Count -and $i -ne $script:menu) { $script:menu = $i; $form.Invalidate() }
    if ($_.X -gt $labelX -and $script:inPage -and $i -lt (Page-Rows).Count -and $i -ne $script:row) { $script:row = $i; $form.Invalidate() }
})
$form.Add_MouseClick({
    $x = $_.X; $i = Hit-Row $_.Y
    if ($_.Button -eq 'Right') { Back; return }
    if ($i -lt 0) { return }
    if ($x -lt 214) {
        if ($i -lt $pages.Count) { $script:menu = $i; $script:inPage = $false; Accept }
        return
    }
    $rows = Page-Rows
    if ($x -lt $labelX -or $i -ge $rows.Count) { return }
    # From the menu, the first click only enters the page and selects the row.
    $entering = -not $script:inPage
    $script:inPage = $true; $script:row = $i
    if ($entering) { $form.Invalidate(); return }
    $row = $rows[$i]
    if ($row.action) { Accept }
    elseif ($row.boxes) {
        $b = [Math]::Floor(($x - $valueX - 8) / 62)
        if ($b -ge 0 -and $b -lt $row.names.Count -and ($x - $valueX - 8 - $b * 62) -lt 52) { $row.index = $b }
    }
    elseif ($x -ge $valueX) { Change-Value $(if ($x -lt $valueX + $valueW / 2) { -1 } else { 1 }) }
    $form.Invalidate()
})
# Borderless, so let a drag on the header move the window.
$form.Add_MouseDown({ if ($_.Y -lt 66) { $script:drag = $_.Location } })
$form.Add_MouseUp({ $script:drag = $null })
$form.Add_MouseMove({ if ($script:drag) { $form.Location = [Drawing.Point]::new($form.Left + $_.X - $script:drag.X, $form.Top + $_.Y - $script:drag.Y) } })

[void]$form.ShowDialog()
if ($form.Tag -ne 'play') { exit }

$height = $heights[$resolution.index]
$count = $samples[$msaa.index]
$smaaOn = $smaa.index -eq 1
$widescreen = $screen.index -eq 1
$percent = $volumes[$volume.index]
New-Item -ItemType Directory -Force (Split-Path $settingsPath) | Out-Null
[pscustomobject]@{ height = $height; msaa = $count; smaa = $smaaOn; widescreen = $widescreen; volume = $percent } |
    ConvertTo-Json | Set-Content $settingsPath -Encoding ASCII
# The runner parses the scale with atof, so always write a '.' decimal point.
$env:RECOMP_D3D_SCALE = ($height / 480).ToString([Globalization.CultureInfo]::InvariantCulture)
$env:RECOMP_D3D_MSAA = "$count"
$env:RECOMP_D3D_SMAA = if ($smaaOn) { '1' } else { '0' }
$env:RECOMP_D3D_WIDESCREEN = if ($widescreen) { '1' } else { '0' }
$env:RECOMP_AUDIO_GAIN = ($percent / 100).ToString([Globalization.CultureInfo]::InvariantCulture)
Start-Process -FilePath (Join-Path $root 'Play.cmd') -WorkingDirectory $root
