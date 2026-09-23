Add-Type -AssemblyName System.Drawing

# What every particle in the game is made of.
#
# It used to be one soft dot. Everything was that dot tinted and scaled, which
# keeps the system to a single draw call - and also meant a spark off a plate,
# a chunk of a dead animal, a bubble and a cloud of silt were the same picture
# four times in four colours. A player could not tell what had just happened
# without reading the colour, and in a dark basin the colour is the first thing
# the water takes away.
#
# Four cells in one strip instead. Still one texture and still one draw call:
# a particle just says which quarter of it to use.
#
#   0  dot     the original glow, unchanged, and still the default
#   1  shard   a hard angular chip - what a body comes apart into
#   2  bubble  a rim with nothing in it - gas, not light
#   3  streak  a soft smear, drawn along whatever threw it
$OutPath = "$PSScriptRoot\..\assets\textures\particle_dot.png"
$N = 64
$Cells = 4
$c = ($N - 1) / 2.0

$bmp = New-Object System.Drawing.Bitmap -ArgumentList ($N * $Cells), $N, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

function Put($cell, $x, $y, $a) {
    if ($a -lt 0) { $a = 0 }
    if ($a -gt 255) { $a = 255 }
    $bmp.SetPixel($cell * $N + $x, $y, [System.Drawing.Color]::FromArgb([int]$a, 255, 255, 255))
}

for ($y = 0; $y -lt $N; $y++) {
    for ($x = 0; $x -lt $N; $x++) {
        $dx = ($x - $c) / $c
        $dy = ($y - $c) / $c
        $d = [math]::Sqrt($dx * $dx + $dy * $dy)

        # --- 0: the dot, exactly as it always was ---------------------
        $f = 1.0 - $d
        if ($f -lt 0) { $f = 0 }
        Put 0 $x $y ([math]::Round($f * $f * 255))

        # --- 1: a shard ----------------------------------------------
        # A four-sided chip on the diagonal with a hard edge. Hard is the
        # point: a piece of something is not a glow.
        $chip = [math]::Abs($dx) * 0.72 + [math]::Abs($dy)
        $a = 0
        if ($chip -lt 0.86) { $a = 255 }
        elseif ($chip -lt 1.0) { $a = [math]::Round((1.0 - ($chip - 0.86) / 0.14) * 255) }
        Put 1 $x $y $a

        # --- 2: a bubble ---------------------------------------------
        # A rim with nothing in it, and a small highlight up and left where
        # every other lit thing in this game has one.
        $ring = 1.0 - [math]::Abs($d - 0.74) / 0.22
        if ($ring -lt 0) { $ring = 0 }
        $a = [math]::Round($ring * $ring * 210)

        $hx = $dx + 0.34
        $hy = $dy + 0.40
        $spot = 1.0 - [math]::Sqrt($hx * $hx + $hy * $hy) / 0.26
        if ($spot -gt 0) { $a = $a + [math]::Round($spot * 200) }

        Put 2 $x $y $a

        # --- 3: a streak ---------------------------------------------
        # Long across the cell and thin through it, faded at both ends, so
        # that drawn along the direction it was thrown it reads as speed.
        $along = 1.0 - [math]::Abs($dx)
        if ($along -lt 0) { $along = 0 }
        $through = 1.0 - [math]::Abs($dy) / 0.30
        if ($through -lt 0) { $through = 0 }

        Put 3 $x $y ([math]::Round($along * $through * $through * 255))
    }
}

$dir = Split-Path -Parent $OutPath
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

"Saved $OutPath ($($N * $Cells) x $N, $Cells shapes)"
