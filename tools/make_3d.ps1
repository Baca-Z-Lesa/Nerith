# Regenerates every texture from its own script, then lights all of them with
# the same relief pass so the whole game looks like it was drawn by one hand.
#
# Run this, not the individual scripts: the relief pass is not idempotent, so
# shading an already shaded sprite would bake the highlight in twice. Going
# through here guarantees each texture is drawn flat and then lit exactly once.

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\relief.ps1"

$TexDir = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$FlatDir = "$PSScriptRoot\flat"   # the unlit art, kept so the pass can be retuned

# What every texture is made of. Anything not listed here is left alone, which
# is how a new texture fails loudly rather than silently going unshaded.
$Materials = @{
    # --- the sub and its hardware ---
    'player_sub.png'   = 'metal'
    'att_armour.png'   = 'metal'
    'att_cannon.png'   = 'metal'
    'att_drone.png'    = 'hardware'
    'att_engine.png'   = 'metal'
    'att_sonar.png'    = 'hardware'
    'att_torpedo.png'  = 'metal'
    'att_twin.png'     = 'metal'
    'torpedo.png'      = 'metal'
    'mine.png'         = 'hardware'

    # --- creatures ---
    'enemy_lurker.png'    = 'flesh'
    'enemy_darter.png'    = 'flesh'
    'enemy_hulk.png'      = 'shell'
    'enemy_spitter.png'   = 'flesh'
    'enemy_bloom.png'     = 'jelly'
    'enemy_spawnling.png' = 'flesh'
    'enemy_brooder.png'   = 'flesh'
    'enemy_lancer.png'    = 'shell'
    'enemy_weaver.png'    = 'shell'
    'enemy_husk.png'      = 'flesh'
    'enemy_angler.png'    = 'flesh'
    'fish.png'            = 'flesh'

    # --- bosses ---
    'hol_crown.png'     = 'shell'
    'hol_vertebra.png'  = 'shell'
    'hol_lamp.png'      = 'glow'
    'hol_ribs.png'      = 'shell'
    'hol_mass.png'      = 'jelly'
    'lev_head.png'      = 'shell'
    'lev_body.png'      = 'shell'
    'lev_fin.png'       = 'flesh'
    'lev_eye.png'       = 'glass'
    'lev_plate.png'     = 'shell'
    'lev_organ.png'     = 'jelly'
    'med_bell.png'      = 'jelly'
    'med_tentacle.png'  = 'jelly'
    'med_nucleus.png'   = 'glow'
    'worm_head.png'     = 'shell'
    'worm_seg.png'      = 'shell'
    'worm_vent.png'     = 'jelly'

    # --- the world ---
    'rock_a.png'  = 'rock'
    'rock_b.png'  = 'rock'
    'coral_a.png' = 'plant'
    'coral_b.png' = 'plant'
    'kelp.png'    = 'plant'
    'nest.png'    = 'shell'

    # --- projectiles and motes ---
    'shot_spitter.png'    = 'glow'
    'shot_bloom.png'      = 'glow'
    'shot_weaver.png'     = 'glow'
    'shot_husk.png'       = 'shell'
    'shot_leviathan.png'  = 'glow'
    'shot_sovereign.png'  = 'glow'
    'shot_worm.png'       = 'glow'
    'shot_hollow.png'     = 'glow'

    'water_jet.png'    = 'soft'
    'enemy_orb.png'    = 'glow'
    'particle_dot.png' = 'soft'
}

Write-Host "Drawing the flat art"
foreach ($script in @('make_sub.ps1', 'make_hull.ps1', 'make_art.ps1',
                      'make_particle.ps1', 'make_world.ps1',
                      'make_enemies.ps1', 'make_enemies2.ps1', 'make_bestiary.ps1',
                      'make_creatures.ps1', 'make_bosses.ps1',
                      'make_boss4.ps1', 'make_shots.ps1', 'make_nest.ps1', 'make_ship.ps1')) {
    Write-Host "  $script"
    & "$PSScriptRoot\$script" | Out-Null
}

if (-not (Test-Path $FlatDir)) { New-Item -ItemType Directory -Force -Path $FlatDir | Out-Null }

Write-Host "Lighting it"
$seed = 1
$missing = @()

foreach ($file in (Get-ChildItem "$TexDir\*.png" | Sort-Object Name)) {
    $name = $file.Name

    if (-not $Materials.ContainsKey($name)) {
        $missing += $name
        continue
    }

    # Keep the unlit version: the relief pass gets retuned, and rerunning every
    # drawing script to try a new number would be slow and pointless.
    Copy-Item $file.FullName "$FlatDir\$name" -Force

    $seed++
    $material = New-Material -Kind $Materials[$name] -Seed ([uint32]$seed)

    [Relief]::Apply("$FlatDir\$name", $file.FullName, $material)

    # The preview folder is what gets looked at, so it gets the lit version too.
    $img = [System.Drawing.Image]::FromFile($file.FullName)
    $w = $img.Width * 3; $h = $img.Height * 3
    $dst = New-Object System.Drawing.Bitmap -ArgumentList $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gd = [System.Drawing.Graphics]::FromImage($dst)
    $gd.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $gd.DrawImage($img, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $w, $h))
    $gd.Dispose(); $img.Dispose()
    if (-not (Test-Path $PreviewDir)) { New-Item -ItemType Directory -Force -Path $PreviewDir | Out-Null }
    $dst.Save("$PreviewDir\$name", [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()

    Write-Host ("  {0,-22} {1}" -f $name, $Materials[$name])
}

if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "No material for, so left flat:" -ForegroundColor Yellow
    foreach ($m in $missing) { Write-Host "  $m" -ForegroundColor Yellow }
}
