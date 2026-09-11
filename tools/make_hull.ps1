Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\organic.ps1"

# The sub. Not a toy and not a fish: a pressure vessel somebody welded together
# and then took somewhere it was not rated for.
#
# What has to read at a glance, in this order: which end is the front, where the
# window is, and that it is metal. Everything else is wear - the seams between
# rolled plates, the rings where the frames are, the rivet lines, the scuffs
# along the keel where it has been dragged over rock.
#
# Facing right, drawn at 4x, downsampled. make_3d.ps1 lights it.

$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 3), ($outH * 3)))) {
        $dir = $target[0]
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
        $dst = New-Object System.Drawing.Bitmap -ArgumentList $target[1], $target[2], ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $gd = [System.Drawing.Graphics]::FromImage($dst)
        $gd.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $gd.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $gd.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $gd.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $gd.DrawImage($bmp, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $target[1], $target[2]))
        $gd.Dispose()
        $dst.Save("$dir\$name", [System.Drawing.Imaging.ImageFormat]::Png)
        $dst.Dispose()
    }
    $bmp.Dispose()
    "  $name  ($outW x $outH)"
}

# A line of rivets along a path. Small, regular, and the single cheapest thing
# that says "this was built" rather than "this was grown".
function Rivets($g, $x0, $y0, $x1, $y1, $count, $col, $rad) {
    for ($i = 0; $i -lt $count; $i++) {
        $t = ($i + 0.5) / $count
        Disc $g $col ($x0 + ($x1 - $x0) * $t) ($y0 + ($y1 - $y0) * $t) $rad
    }
}

"generating the hull..."

$outW = 128; $outH = 80
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$steel   = C 92 106 118
$steelLo = C 44 54 64
$steelHi = C 158 176 190
$shadow  = C 26 32 40
$glass   = C 126 236 226
$brass   = C 188 150 92

# ---------------------------------------------------------------- the stern
# A shrouded thruster: the ring, the blades behind it, and the pylon it is on.
Ring $g $steelLo ($W * 0.075) $cy 44 18
Ring $g $steelHi ($W * 0.075) $cy 44 7
for ($i = 0; $i -lt 4; $i++) {
    $a = [math]::PI * $i / 4.0 + 0.4
    Stroke $g (C 62 74 86) ($W * 0.075 - [math]::Cos($a) * 34) ($cy - [math]::Sin($a) * 34) `
        ($W * 0.075 + [math]::Cos($a) * 34) ($cy + [math]::Sin($a) * 34) 11
}
Disc $g $steelLo ($W * 0.075) $cy 13

# ---------------------------------------------------------------- dive planes
# Set aft, where they belong on something that steers by pitching.
foreach ($side in @(-1, 1)) {
    Blob $g @( (P ($W * 0.30) ($cy + $side * 22)), (P ($W * 0.19) ($cy + $side * 58)),
               (P ($W * 0.10) ($cy + $side * 60)), (P ($W * 0.13) ($cy + $side * 30)) ) `
        $steelLo 0.35
    Stroke $g (C 120 138 150 180) ($W * 0.16) ($cy + $side * 50) ($W * 0.27) ($cy + $side * 26) 4
}

# ---------------------------------------------------------------- the hull
# A cigar with a blunt nose and a waisted tail, built as a body so the plating
# and the shading follow the form rather than lying flat across it.
$spine = @()
for ($i = 0; $i -le 12; $i++) {
    $t = $i / 12.0
    $x = $W * (0.94 - 0.80 * $t)
    $half = $H * (0.075 + 0.30 * [math]::Sin([math]::PI * [math]::Pow($t, 0.44)))
    $spine += , @($x, $cy, $half)
}
Body $g $spine $steel $steelLo $steelHi 0.5

$outline = BodyOutline $spine
$clip = New-Object System.Drawing.Drawing2D.GraphicsPath
$clip.AddClosedCurve([System.Drawing.PointF[]]$outline, 0.5)
$saved = $g.Save()
$g.SetClip($clip, [System.Drawing.Drawing2D.CombineMode]::Intersect)

# Frames: the rings the pressure hull is built on, showing through the plate.
for ($i = 0; $i -lt 7; $i++) {
    $x = $W * (0.22 + 0.105 * $i)
    Stroke $g (C 36 44 54 170) $x ($cy - $H * 0.34) $x ($cy + $H * 0.34) 7
    Stroke $g (C 128 146 160 110) ($x + 6) ($cy - $H * 0.34) ($x + 6) ($cy + $H * 0.34) 3
}

# The seam where the two rolled halves were welded together, and the rivets
# holding them.
Stroke $g (C 34 42 52 150) ($W * 0.16) ($cy - $H * 0.20) ($W * 0.92) ($cy - $H * 0.20) 5
Rivets $g ($W * 0.20) ($cy - $H * 0.20) ($W * 0.90) ($cy - $H * 0.20) 16 (C 150 168 182 150) 3.5
Stroke $g (C 34 42 52 130) ($W * 0.16) ($cy + $H * 0.22) ($W * 0.92) ($cy + $H * 0.22) 5

# Scuffs down the keel: it has been dragged over rock more than once.
$rand = New-Object System.Random -ArgumentList 17
for ($i = 0; $i -lt 14; $i++) {
    $x = $W * (0.20 + $rand.NextDouble() * 0.68)
    $y = $cy + $H * (0.16 + $rand.NextDouble() * 0.18)
    Stroke $g (C 178 194 206 70) $x $y ($x + 10 + $rand.NextDouble() * 22) ($y - 3) 3
}
# And rust where the water has got into the welds.
for ($i = 0; $i -lt 10; $i++) {
    $x = $W * (0.18 + $rand.NextDouble() * 0.7)
    $y = $cy + ($rand.NextDouble() - 0.5) * $H * 0.5
    Disc $g (C 128 82 48 60) $x $y (6 + $rand.NextDouble() * 12)
}

$g.Restore($saved)
$clip.Dispose()

# ---------------------------------------------------------------- conning tower
# Low and faired back, because anything that stands proud gets torn off.
Blob $g @( (P ($W * 0.62) ($cy - $H * 0.30)), (P ($W * 0.56) ($cy - $H * 0.44)),
           (P ($W * 0.40) ($cy - $H * 0.46)), (P ($W * 0.33) ($cy - $H * 0.30)) ) $steelLo 0.4
Stroke $g (C 138 156 170 170) ($W * 0.37) ($cy - $H * 0.42) ($W * 0.56) ($cy - $H * 0.41) 4
Disc $g (C 30 38 46) ($W * 0.46) ($cy - $H * 0.37) 9
Disc $g $glass ($W * 0.46) ($cy - $H * 0.37) 5

# ---------------------------------------------------------------- the window
# The one warm thing on it, and the reason it reads as crewed rather than as a
# drone: a brass ring, thick glass, and light behind it.
Disc $g $shadow ($W * 0.735) $cy 46
Disc $g $brass ($W * 0.735) $cy 40
Ring $g (C 246 214 148 200) ($W * 0.735) $cy 40 5
Disc $g (C 18 44 48) ($W * 0.735) $cy 31
for ($i = 5; $i -ge 1; $i--) {
    $t = $i / 5.0
    Disc $g (C $glass.R $glass.G $glass.B ([int](40 * (1 - $t) * 3.4))) ($W * 0.735) $cy (31 * $t)
}
Disc $g (C 214 255 250) ($W * 0.722) ($cy - 10) 11
# The bolts round the rim.
for ($i = 0; $i -lt 10; $i++) {
    $a = 2 * [math]::PI * $i / 10.0
    Disc $g (C 236 206 144) ($W * 0.735 + [math]::Cos($a) * 44) ($cy + [math]::Sin($a) * 44) 4.5
}

# ---------------------------------------------------------------- the bow
# A cast nose cap, ribbed, with the lamp housing under it.
Blob $g @( (P ($W * 0.985) $cy), (P ($W * 0.92) ($cy - $H * 0.17)),
           (P ($W * 0.84) ($cy - $H * 0.19)), (P ($W * 0.84) ($cy + $H * 0.19)),
           (P ($W * 0.92) ($cy + $H * 0.17)) ) (C 74 88 100) 0.45
for ($i = 0; $i -lt 3; $i++) {
    $x = $W * (0.88 + 0.032 * $i)
    Stroke $g (C 132 150 164 150) $x ($cy - $H * 0.14) $x ($cy + $H * 0.14) 4
}

# The lamp: a housing, and the beam it is throwing.
Poly $g @( (P ($W * 0.965) ($cy + 10)), (P ($W * 0.965) ($cy + 30)),
           (P ($W * 0.90) ($cy + 26)), (P ($W * 0.90) ($cy + 12)) ) $steelLo
Lamp $g ($W * 0.965) ($cy + 20) 10 (C 255 242 198)

# Running lights: one red to port, one green to starboard. Nothing else on the
# hull is saturated, so these read instantly as lamps.
Lamp $g ($W * 0.56) ($cy - $H * 0.33) 6 (C 255 90 80)
Lamp $g ($W * 0.56) ($cy + $H * 0.34) 6 (C 110 255 130)

Finish $bmp $g $outW $outH "player_sub.png"
