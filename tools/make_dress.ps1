Add-Type -AssemblyName System.Drawing

# What is lying about at a given depth, as opposed to at every depth.
#
# The seabed scattered the same five things - two rocks, two corals and kelp -
# from the shallows to the bottom, and a depth was a palette and nothing else.
# You could not tell the trench from the cradle with the HUD covered up, which
# on a game whose whole subject is going further down is the wrong thing not
# to be able to tell.
#
# Four more, handed out by DepthTheme:
#
#   worms   the mat that grows on a vent field, and nowhere else
#   bones   something large fell here a long time ago
#   pipes   the industry: three crews came through and left their gear
#   wall    architecture. The deeper it gets, the less it looks natural.
. "$PSScriptRoot\organic.ps1"

$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function Fade($col, $alpha) { C $col.R $col.G $col.B $alpha }

function Shape($g, $pts, $fill, $edge, $width, $tension = 0.5) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, [float]$tension)
    if ($null -ne $fill) { $g.FillPath((Brush $fill), $path) }
    if ($null -ne $edge) {
        $pen = New-Object System.Drawing.Pen -ArgumentList $edge, $width
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($pen, $path)
        $pen.Dispose()
    }
    $path.Dispose()
}

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

"generating the depth dressing..."

$ink = C 10 12 16

# =====================================================================
#  WORMS  -  a mat of tube worms. The one thing down here that is not
#  scavenging: it eats the water coming out of the floor, so where this
#  grows, the floor is open.
# =====================================================================
$outW = 96; $outH = 72
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$rand = New-Object System.Random -ArgumentList 4477

$tube   = C 186 174 154
$tubeLo = C 118 108 92
$plume  = C 206 92 84

# The crust they are rooted in, which is what makes it a colony rather than a
# handful of separate tubes.
Shape $g @( (P ($W * 0.08) ($H * 0.86)), (P ($W * 0.30) ($H * 0.72)),
            (P ($W * 0.62) ($H * 0.74)), (P ($W * 0.92) ($H * 0.88)),
            (P ($W * 0.70) ($H * 0.98)), (P ($W * 0.24) ($H * 0.98)) ) (C 62 56 50) $null 0 0.3

for ($i = 0; $i -lt 22; $i++) {
    $bx = $W * (0.10 + 0.038 * $i)
    $by = $H * (0.90 - 0.10 * $rand.NextDouble())
    $len = $H * (0.24 + 0.42 * $rand.NextDouble())
    $lean = ($rand.NextDouble() - 0.5) * $W * 0.08

    Stroke $g $tubeLo $bx $by ($bx + $lean) ($by - $len) (3.4 * $SS)
    Stroke $g $tube ($bx - 0.6 * $SS) $by ($bx + $lean - 0.6 * $SS) ($by - $len) (1.8 * $SS)
    Disc $g $plume ($bx + $lean) ($by - $len) (2.4 * $SS)
    Disc $g (Fade (C 240 150 140) 190) ($bx + $lean - 0.6 * $SS) ($by - $len - 0.6 * $SS) (1.1 * $SS)
}
Finish $bmp $g $outW $outH "deep_worms.png"

# =====================================================================
#  BONES  -  a fall. Something enormous came down here and the floor has
#  been eating it ever since, which is why the ribs are still standing
#  and nothing else is.
# =====================================================================
$outW = 128; $outH = 88
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$bone   = C 196 190 172
$boneLo = C 118 112 98
$boneHi = C 232 228 212

# The spine, half buried, running the length of it.
for ($i = 0; $i -le 9; $i++) {
    $t = $i / 9.0
    $x = $W * (0.10 + 0.80 * $t)
    $y = $H * (0.66 + 0.06 * [math]::Sin($t * 3.4))
    $r = $H * (0.085 - 0.03 * $t)

    Disc $g $boneLo $x ($y + $r * 0.2) $r
    Disc $g $bone $x $y ($r * 0.9)
    Disc $g (Fade $boneHi 150) ($x - $r * 0.28) ($y - $r * 0.3) ($r * 0.4)
}

# Ribs off the front half only: the back of it went first, which is how a
# fall actually goes.
for ($i = 0; $i -le 5; $i++) {
    $t = $i / 5.0
    $x = $W * (0.14 + 0.44 * $t)
    $y = $H * (0.66 + 0.06 * [math]::Sin($t * 1.9))
    $span = $H * (0.46 - 0.16 * $t)

    foreach ($side in @(-1, 1)) {
        $mid = $x + $side * $span * 0.5
        $tip = $x + $side * $span * 0.75
        Stroke $g $boneLo $x $y $mid ($y - $span * 0.62) (3.6 * $SS)
        Stroke $g $boneLo $mid ($y - $span * 0.62) $tip ($y - $span * 0.20) (3.0 * $SS)
        Stroke $g (Fade $bone 220) $x $y $mid ($y - $span * 0.62) (1.8 * $SS)
        Stroke $g (Fade $bone 220) $mid ($y - $span * 0.62) $tip ($y - $span * 0.20) (1.5 * $SS)
    }
}

# The skull end, and the silt that has taken the rest.
Shape $g @( (P ($W * 0.04) ($H * 0.62)), (P ($W * 0.14) ($H * 0.50)),
            (P ($W * 0.22) ($H * 0.60)), (P ($W * 0.16) ($H * 0.78)),
            (P ($W * 0.05) ($H * 0.74)) ) $bone $ink (2.2 * $SS) 0.3
Disc $g (C 22 22 24) ($W * 0.115) ($H * 0.625) (5.0 * $SS)
Shape $g @( (P 0 ($H * 0.82)), (P ($W * 0.40) ($H * 0.76)),
            (P ($W * 0.86) ($H * 0.84)), (P $W ($H * 0.94)),
            (P 0 ($H * 0.99)) ) (C 40 42 44 170) $null 0 0.25
Finish $bmp $g $outW $outH "deep_bones.png"

# =====================================================================
#  PIPES  -  the industry. Three crews came down this cable before you
#  and none of them took their gear home.
# =====================================================================
$outW = 112; $outH = 80
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$rand = New-Object System.Random -ArgumentList 9137

$steel   = C 92 100 106
$steelHi = C 148 158 166
$steelLo = C 48 54 60
$rust    = C 128 78 44

# A length of line pipe, half sunk, with its flange still bolted to nothing.
Shape $g @( (P ($W * 0.06) ($H * 0.48)), (P ($W * 0.74) ($H * 0.38)),
            (P ($W * 0.78) ($H * 0.60)), (P ($W * 0.08) ($H * 0.70)) ) $steel $ink (2.4 * $SS) 0.05
Stroke $g (Fade $steelHi 150) ($W * 0.08) ($H * 0.49) ($W * 0.74) ($H * 0.40) (2.0 * $SS)
Stroke $g (Fade $ink 130) ($W * 0.09) ($H * 0.68) ($W * 0.76) ($H * 0.58) (2.2 * $SS)

Disc $g $steelLo ($W * 0.78) ($H * 0.49) (11 * $SS)
Ring $g $steelHi ($W * 0.78) ($H * 0.49) (11 * $SS) (2.0 * $SS)
foreach ($i in 0..5) {
    $a = $i * [math]::PI / 3.0
    Disc $g $steelLo ($W * 0.78 + [math]::Cos($a) * 8 * $SS) ($H * 0.49 + [math]::Sin($a) * 8 * $SS) (1.6 * $SS)
}

# A crate on its side, split, with nothing in it.
Shape $g @( (P ($W * 0.14) ($H * 0.74)), (P ($W * 0.42) ($H * 0.70)),
            (P ($W * 0.44) ($H * 0.94)), (P ($W * 0.16) ($H * 0.97)) ) $steelLo $ink (2.2 * $SS) 0.05
Stroke $g $steelHi ($W * 0.20) ($H * 0.73) ($W * 0.22) ($H * 0.96) (1.6 * $SS)
Stroke $g $steelHi ($W * 0.34) ($H * 0.715) ($W * 0.36) ($H * 0.95) (1.6 * $SS)
Stroke $g (Fade $ink 200) ($W * 0.14) ($H * 0.84) ($W * 0.44) ($H * 0.81) (2.6 * $SS)

# And a cut cable, which is the detail that says somebody left in a hurry.
$cx = $W * 0.56; $cy = $H * 0.24
foreach ($i in 0..9) {
    $t = $i / 9.0
    Stroke $g $steelLo ($cx + $W * 0.30 * $t) ($cy + $H * 0.16 * [math]::Sin($t * 5.2)) `
        ($cx + $W * 0.30 * ($t + 0.11)) ($cy + $H * 0.16 * [math]::Sin(($t + 0.11) * 5.2)) (3.0 * $SS)
}
foreach ($i in 0..3) {
    Stroke $g $rust ($W * 0.86) ($cy + $H * 0.10) ($W * (0.90 + 0.02 * $i)) ($cy + $H * (0.02 + 0.06 * $i)) (1.4 * $SS)
}
for ($i = 0; $i -lt 16; $i++) {
    Disc $g (Fade $rust (50 + $rand.Next(0, 70))) ($rand.Next(0, $W)) ($H * (0.35 + 0.6 * $rand.NextDouble())) ((1 + $rand.NextDouble() * 2.4) * $SS)
}
Finish $bmp $g $outW $outH "deep_pipes.png"

# =====================================================================
#  WALL  -  architecture. Nobody built this and it is built: a course of
#  dressed blocks, a doorway with nothing behind it, and a step. The
#  deeper the player gets, the more of the floor is this.
# =====================================================================
$outW = 120; $outH = 96
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$stone   = C 96 94 88
$stoneHi = C 148 146 138
$stoneLo = C 54 52 50

# Three courses of block, offset the way masonry is, and dead straight -
# which is the whole point. Nothing else on this floor is straight.
foreach ($row in 0..2) {
    $y = $H * (0.34 + 0.20 * $row)
    $shift = ($row % 2) * 0.12

    foreach ($col in 0..3) {
        $x = $W * (-0.04 + $shift + 0.27 * $col)
        $bw = $W * 0.25
        $bh = $H * 0.18

        $g.FillRectangle((Brush $stone), (RectF $x $y $bw $bh))
        $g.FillRectangle((Brush (Fade $stoneHi 120)), (RectF $x $y $bw ($bh * 0.18)))
        $g.FillRectangle((Brush (Fade $stoneLo 160)), (RectF $x ($y + $bh * 0.80) $bw ($bh * 0.20)))

        $pen = New-Object System.Drawing.Pen -ArgumentList $ink, (1.6 * $SS)
        $g.DrawRectangle($pen, $x, $y, $bw, $bh)
        $pen.Dispose()
    }
}

# The doorway. An opening with a lintel over it and nothing behind it, which
# is worse than a wall.
$g.FillRectangle((Brush (C 6 8 10)), (RectF ($W * 0.40) ($H * 0.34) ($W * 0.22) ($H * 0.44)))
$g.FillRectangle((Brush $stoneLo), (RectF ($W * 0.37) ($H * 0.28) ($W * 0.28) ($H * 0.08)))
$g.FillRectangle((Brush (Fade $stoneHi 140)), (RectF ($W * 0.37) ($H * 0.28) ($W * 0.28) ($H * 0.02)))

foreach ($side in @(0.40, 0.60)) {
    $g.FillRectangle((Brush $stone), (RectF ($W * $side - 2 * $SS) ($H * 0.34) (4 * $SS) ($H * 0.44)))
}

# The step at its foot, worn round by something that used it.
Shape $g @( (P ($W * 0.32) ($H * 0.78)), (P ($W * 0.70) ($H * 0.78)),
            (P ($W * 0.74) ($H * 0.90)), (P ($W * 0.28) ($H * 0.90)) ) $stone $ink (2.0 * $SS) 0.08
Stroke $g (Fade $stoneHi 150) ($W * 0.33) ($H * 0.79) ($W * 0.69) ($H * 0.79) (1.8 * $SS)

Mottle $g @( @(($W * 0.30), ($H * 0.50), ($H * 0.24)),
             @(($W * 0.72), ($H * 0.52), ($H * 0.22)) ) $stoneLo 16 617
Finish $bmp $g $outW $outH "deep_wall.png"
