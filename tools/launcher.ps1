# SPDX-License-Identifier: GPL-3.0-or-later
# Pick resolution, anti-aliasing, widescreen, volume and the DOAXBV swimsuit switch (and its bounce), remember them, then start Play.cmd.
Add-Type -AssemblyName System.Windows.Forms
# The console is hidden, so show any failure instead of exiting silently.
$ErrorActionPreference = 'Stop'
trap { [Windows.Forms.MessageBox]::Show("$_", 'DOA3 launcher') | Out-Null; exit 1 }
$root = Split-Path $PSScriptRoot -Parent
$settingsPath = Join-Path $root 'private\launcher.json'
$heights = 480, 720, 1080, 1440, 2160
$samples = 1, 2, 4, 8
$volumes = 100, 75, 50, 25, 0
$settings = [pscustomobject]@{ height = [Windows.Forms.Screen]::PrimaryScreen.Bounds.Height; msaa = 1; smaa = $false; widescreen = $false; volume = 100 }
try { $settings = Get-Content $settingsPath -Raw -ErrorAction Stop | ConvertFrom-Json } catch {}

$form = New-Object Windows.Forms.Form
$form.Text = 'Dead or Alive 3'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.StartPosition = 'CenterScreen'
$form.ClientSize = New-Object Drawing.Size 240, 346

function Add-Choice($text, $top, $items, $index) {
    $label = New-Object Windows.Forms.Label -Property @{ Text = $text; Left = 12; Top = $top + 3; Width = 80 }
    $box = New-Object Windows.Forms.ComboBox -Property @{ Left = 100; Top = $top; Width = 126; DropDownStyle = 'DropDownList' }
    $box.Items.AddRange($items)
    $box.SelectedIndex = [Math]::Max(0, $index)
    $form.Controls.AddRange(@($label, $box))
    $box
}
$heightNames = [string[]]($heights | ForEach-Object { if ($_ -eq 480) { '480p (windowed)' } else { "$($_)p" } })
$nearest = @($heights | Where-Object { $_ -le [int]$settings.height }).Count - 1
$resolution = Add-Choice 'Resolution' 12 $heightNames $nearest
$msaaNames = [string[]]($samples | ForEach-Object { if ($_ -eq 1) { 'Off' } else { "$($_)x" } })
$msaa = Add-Choice 'MSAA' 44 $msaaNames ([Array]::IndexOf($samples, [int]$settings.msaa))
$smaa = New-Object Windows.Forms.CheckBox -Property @{ Text = 'SMAA'; Left = 100; Top = 76; Checked = [bool]$settings.smaa }
# PowerShell 7 loads JSON numbers as Int64, so cast; a missing volume means 100%.
$savedVolume = if ($null -ne $settings.volume) { [Array]::IndexOf($volumes, [int]$settings.volume) } else { -1 }
$volumeNames = [string[]]($volumes | ForEach-Object { if ($_ -eq 0) { 'Mute' } else { "$_%" } })
$volume = Add-Choice 'Volume' 104 $volumeNames $savedVolume
$widescreen = New-Object Windows.Forms.CheckBox -Property @{ Text = 'Widescreen (16:9)'; Left = 100; Top = 132; Width = 136; Checked = [bool]$settings.widescreen }
# The swimsuit switch builds DOAXBV swimsuits from the player's DOAXBV disc.
$doaxbvFile = Join-Path $root 'private\doaxbv-path.txt'
$swimsuits = New-Object Windows.Forms.CheckBox -Property @{ Text = 'DOAXBV swimsuits'; Left = 12; Top = 160; Width = 216; Checked = ($null -eq $settings.swimsuits -or [bool]$settings.swimsuits) }
$startFavorite = New-Object Windows.Forms.CheckBox -Property @{ Text = 'Start in a favorite'; Left = 28; Top = 182; Width = 200; Checked = [bool]$settings.swimsuit_start_favorite }
# Bounce: chest physics strength, in percent of the default.
$bounceValues = 0, 25, 50, 75, 100, 125, 150, 175, 200, 250, 300, 350, 400, 500, 600, 700, 800, 900, 1000
$savedBounce = if ($null -ne $settings.swimsuit_bounce) { [Array]::IndexOf($bounceValues, [int]$settings.swimsuit_bounce) } else { 4 }
$bounceLabel = New-Object Windows.Forms.Label -Property @{ Text = 'Bounce'; Left = 28; Top = 211; Width = 64 }
$bounce = New-Object Windows.Forms.ComboBox -Property @{ Left = 100; Top = 208; Width = 126; DropDownStyle = 'DropDownList' }
$bounce.Items.AddRange([string[]]($bounceValues | ForEach-Object { if ($_ -eq 0) { 'Off' } else { "$_%" } }))
$bounce.SelectedIndex = if ($savedBounce -ge 0) { $savedBounce } else { 4 }
# Size: chest size, in percent.
$sizeValues = 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200
$savedSize = if ($null -ne $settings.swimsuit_size) { [Array]::IndexOf($sizeValues, [int]$settings.swimsuit_size) } else { 5 }
$sizeLabel = New-Object Windows.Forms.Label -Property @{ Text = 'Size'; Left = 28; Top = 241; Width = 64 }
$size = New-Object Windows.Forms.ComboBox -Property @{ Left = 100; Top = 238; Width = 126; DropDownStyle = 'DropDownList' }
$size.Items.AddRange([string[]]($sizeValues | ForEach-Object { "$_%" }))
$size.SelectedIndex = if ($savedSize -ge 0) { $savedSize } else { 5 }
$choose = New-Object Windows.Forms.Button -Property @{ Text = 'Choose DOAXBV disc...'; Left = 12; Top = 268; Width = 214 }
$discLabel = New-Object Windows.Forms.Label -Property @{ Left = 12; Top = 296; Width = 216; AutoEllipsis = $true }
$discLabel.Text = if (Test-Path $doaxbvFile) { Split-Path ((Get-Content $doaxbvFile -TotalCount 1).Trim()) -Leaf } else { 'No DOAXBV disc chosen' }
$choose.Add_Click({
    $dialog = New-Object Windows.Forms.OpenFileDialog -Property @{ Title = 'DOAXBV disc image (.iso), or datahdd.afs in its extracted folder'; Filter = 'DOAXBV disc|*.iso;datahdd.afs|All files|*.*' }
    if ($dialog.ShowDialog() -eq 'OK') {
        $chosen = if ($dialog.FileName -like '*.afs') { Split-Path $dialog.FileName } else { $dialog.FileName }
        New-Item -ItemType Directory -Force (Split-Path $doaxbvFile) | Out-Null
        Set-Content $doaxbvFile $chosen -Encoding UTF8
        $discLabel.Text = Split-Path $chosen -Leaf
    }
})
$tipText = 'Every DOA3 woman can wear her DOAXBV swimsuits, built from your DOAXBV disc. In a fight: hold Back, then RB / LB (yours) or RT / LT (opponent).'
$play = New-Object Windows.Forms.Button -Property @{ Text = 'Play'; Left = 151; Top = 316; DialogResult = 'OK' }
$tip = New-Object Windows.Forms.ToolTip
$tip.SetToolTip($widescreen, 'Wider 3D view. The HUD and menus are stretched.')
$tip.SetToolTip($swimsuits, $tipText)
$tip.SetToolTip($startFavorite, 'Each fighter starts in her first favorite swimsuit (Back + Y marks a favorite).')
$tip.SetToolTip($bounce, 'Chest physics on DOAXBV swimsuits. 100% is the default; Off keeps them still.')
$tip.SetToolTip($size, 'Chest size on DOAXBV swimsuits. 100% is normal.')
$form.Controls.AddRange(@($smaa, $widescreen, $swimsuits, $startFavorite, $bounceLabel, $bounce, $sizeLabel, $size, $choose, $discLabel, $play))
$form.AcceptButton = $play
if ($form.ShowDialog() -ne 'OK') { exit }

$height = $heights[$resolution.SelectedIndex]
$count = $samples[$msaa.SelectedIndex]
$percent = $volumes[$volume.SelectedIndex]
New-Item -ItemType Directory -Force (Split-Path $settingsPath) | Out-Null
[pscustomobject]@{ height = $height; msaa = $count; smaa = $smaa.Checked; widescreen = $widescreen.Checked; volume = $percent; swimsuits = $swimsuits.Checked; swimsuit_start_favorite = $startFavorite.Checked; swimsuit_bounce = $bounceValues[$bounce.SelectedIndex]; swimsuit_size = $sizeValues[$size.SelectedIndex] } |
    ConvertTo-Json | Set-Content $settingsPath -Encoding ASCII
# The runner parses the scale with atof, so always write a '.' decimal point.
$env:RECOMP_D3D_SCALE = ($height / 480).ToString([Globalization.CultureInfo]::InvariantCulture)
$env:RECOMP_D3D_MSAA = "$count"
$env:RECOMP_D3D_SMAA = if ($smaa.Checked) { '1' } else { '0' }
$env:RECOMP_D3D_WIDESCREEN = if ($widescreen.Checked) { '1' } else { '0' }
$env:RECOMP_AUDIO_GAIN = ($percent / 100).ToString([Globalization.CultureInfo]::InvariantCulture)
Start-Process -FilePath (Join-Path $root 'Play.cmd') -WorkingDirectory $root
