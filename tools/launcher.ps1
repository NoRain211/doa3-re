# SPDX-License-Identifier: GPL-3.0-or-later
# Pick resolution, anti-aliasing, widescreen and volume, remember them, then start Play.cmd.
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
$form.ClientSize = New-Object Drawing.Size 240, 196

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
$play = New-Object Windows.Forms.Button -Property @{ Text = 'Play'; Left = 151; Top = 160; DialogResult = 'OK' }
$tip = New-Object Windows.Forms.ToolTip
$tip.SetToolTip($widescreen, 'Wider 3D view. The HUD and menus are stretched.')
$form.Controls.AddRange(@($smaa, $widescreen, $play))
$form.AcceptButton = $play
if ($form.ShowDialog() -ne 'OK') { exit }

$height = $heights[$resolution.SelectedIndex]
$count = $samples[$msaa.SelectedIndex]
$percent = $volumes[$volume.SelectedIndex]
New-Item -ItemType Directory -Force (Split-Path $settingsPath) | Out-Null
[pscustomobject]@{ height = $height; msaa = $count; smaa = $smaa.Checked; widescreen = $widescreen.Checked; volume = $percent } |
    ConvertTo-Json | Set-Content $settingsPath -Encoding ASCII
# The runner parses the scale with atof, so always write a '.' decimal point.
$env:RECOMP_D3D_SCALE = ($height / 480).ToString([Globalization.CultureInfo]::InvariantCulture)
$env:RECOMP_D3D_MSAA = "$count"
$env:RECOMP_D3D_SMAA = if ($smaa.Checked) { '1' } else { '0' }
$env:RECOMP_D3D_WIDESCREEN = if ($widescreen.Checked) { '1' } else { '0' }
$env:RECOMP_AUDIO_GAIN = ($percent / 100).ToString([Globalization.CultureInfo]::InvariantCulture)
Start-Process -FilePath (Join-Path $root 'Play.cmd') -WorkingDirectory $root
