Add-Type -AssemblyName System.Drawing

# The five things standing on the floor of a basin that had no picture.
#
# A wreck, a brood, a dead diver's case, the hole down to the next floor and a
# vent. Every one of them was a fan of triangles in the kind's colour, built
# in Game::drawOffers and Game::drawLifts - which meant the most important
# objects in a level, the ones a player crosses a dark basin FOR, were the
# only things down there that were not drawn.
#
# Same rule as the salvage: each is an object with a lit top, a shaded
# underside and an ink edge, in the palette its config row already gives it,
# so the colour still reads from the edge of the lamp and the shape says what
# it is once you are close.
. "$PSScriptRoot\organic.ps1"

$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function Fade($col, $alpha) { C $col.R $col.G $col.B $alpha }

function Curve($pts, $tension = 0.5) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, $tension)
    return $path
}

function Shape($g, $pts, $fill, $edge, $width, $tension = 0.5) {
    $path = Curve $pts $tension
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
        $gd.InterpolationMode    = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $gd.PixelOffsetMode      = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $gd.CompositingQuality   = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $gd.SmoothingMode        = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $gd.DrawImage($bmp, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $target[1], $target[2]))
        $gd.Dispose()
        $dst.Save("$dir\$name", [System.Drawing.Imaging.ImageFormat]::Png)
        $dst.Dispose()
    }
    $bmp.Dispose()
    "  $name  ($outW x $outH)"
}

"generating the floor gear..."

$ink = C 10 12 16

# =====================================================================
#  WRECK  -  a hull that never opened. Half buried on its side, ribs
#  showing where the plate has gone, and one seam somebody else already
#  started cutting and did not finish.
# =====================================================================
$outW = 96; $outH = 96
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$steel   = C 92 100 108
$steelHi = C 150 162 172
$steelLo = C 46 52 58
$cut     = C 214 148 92

# The buried half first, so the hull sits IN the floor rather than on it.
Shape $g @( (P ($W * 0.06) ($H * 0.72)), (P ($W * 0.50) ($H * 0.64)),
            (P ($W * 0.94) ($H * 0.74)), (P ($W * 0.88) ($H * 0.92)),
            (P ($W * 0.12) ($H * 0.92)) ) $steelLo $null 0 0.2

# The barrel of it, tipped over: an oval lying on its long side.
Shape $g @( (P ($W * 0.10) ($H * 0.54)), (P ($W * 0.34) ($H * 0.22)),
            (P ($W * 0.72) ($H * 0.20)), (P ($W * 0.92) ($H * 0.48)),
            (P ($W * 0.80) ($H * 0.80)), (P ($W * 0.26) ($H * 0.82)) ) $steel $ink (2.6 * $SS) 0.35

# Frames showing through the plate, which is what makes it a wreck and not a
# rock: a thing that was built, coming apart along the lines it was built on.
foreach ($t in @(0.24, 0.40, 0.56, 0.72)) {
    Stroke $g $steelLo ($W * $t) ($H * 0.24) ($W * ($t + 0.03)) ($H * 0.80) (2.4 * $SS)
    Stroke $g (Fade $steelHi 120) ($W * $t - 1.6 * $SS) ($H * 0.26) ($W * ($t + 0.03) - 1.6 * $SS) ($H * 0.78) (1.0 * $SS)
}

# The lit top edge and the dark underside, the same two bands everything in
# this game gets.
Stroke $g (Fade $steelHi 170) ($W * 0.22) ($H * 0.26) ($W * 0.78) ($H * 0.23) (2.2 * $SS)
Stroke $g (Fade $ink 150) ($W * 0.24) ($H * 0.79) ($W * 0.80) ($H * 0.77) (2.6 * $SS)

# And the cut. Started at the top, run a third of the way round, abandoned -
# so the level's verb is already written on the object.
foreach ($i in 0..5) {
    $a = 3.9 + $i * 0.30
    $x = $W * 0.50 + [math]::Cos($a) * $W * 0.30
    $y = $H * 0.50 + [math]::Sin($a) * $H * 0.27
    Disc $g (Fade $cut (200 - $i * 26)) $x $y ((2.6 - $i * 0.22) * $SS)
}

Rivets $g ($W * 0.20) ($H * 0.34) ($W * 0.82) ($H * 0.32) 9 (1.1 * $SS) $steelLo $steelHi
Finish $bmp $g $outW $outH "gear_wreck.png"

# =====================================================================
#  HIVE  -  an egg mass, awake. A cluster of sacs with something folded
#  inside each one, over a root that has taken hold of the floor.
# =====================================================================
$outW = 96; $outH = 96
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$sacLo = C 96 44 62
$sac   = C 168 84 104
$sacHi = C 226 150 162
$vein  = C 70 26 40

# The root: it is attached to the floor, which is why burning it out is a job
# rather than a kill.
foreach ($a in @(2.4, 2.9, 3.4, 3.9, 4.4)) {
    Stroke $g $vein ($W * 0.50) ($H * 0.62) `
        ($W * 0.50 + [math]::Cos($a) * $W * 0.42) ($H * 0.62 + [math]::Sin($a) * $H * 0.30) (3.0 * $SS)
}

# Six sacs, biggest at the back, so it reads as a mass rather than a ring.
$eggs = @(@(0.50, 0.50, 0.26), @(0.30, 0.40, 0.17), @(0.70, 0.40, 0.18),
          @(0.36, 0.66, 0.15), @(0.66, 0.68, 0.16), @(0.50, 0.28, 0.14))

foreach ($e in $eggs) {
    $ex = $W * $e[0]; $ey = $H * $e[1]; $er = $W * $e[2]

    Disc $g $sacLo $ex ($ey + $er * 0.10) $er
    Disc $g $sac $ex $ey ($er * 0.94)
    Disc $g (Fade $sacHi 150) ($ex - $er * 0.26) ($ey - $er * 0.32) ($er * 0.42)

    # Something folded up inside, dark and off centre. An egg you can see into
    # is worse than one you cannot.
    Disc $g (Fade $vein 210) ($ex + $er * 0.10) ($ey + $er * 0.06) ($er * 0.34)
    Stroke $g (Fade $ink 120) ($ex - $er * 0.5) ($ey + $er * 0.2) ($ex + $er * 0.5) ($ey - $er * 0.1) (1.2 * $SS)
}

Mottle $g @( @(($W * 0.30), ($H * 0.50), ($H * 0.24)),
             @(($W * 0.50), ($H * 0.50), ($H * 0.30)),
             @(($W * 0.70), ($H * 0.50), ($H * 0.24)) ) $vein 14 991
Finish $bmp $g $outW $outH "gear_hive.png"

# =====================================================================
#  CACHE  -  what is left of a diver. The rig, the bottle, and the
#  recorder that is the only thing in the game anybody says anything on.
# =====================================================================
$outW = 96; $outH = 96
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$rig    = C 128 122 104
$rigHi  = C 188 182 160
$rigLo  = C 66 62 52
$brass  = C 198 158 88
$glass  = C 118 190 196

# The bottle, lying down, with its straps still done up round nothing.
Shape $g @( (P ($W * 0.16) ($H * 0.46)), (P ($W * 0.50) ($H * 0.38)),
            (P ($W * 0.82) ($H * 0.48)), (P ($W * 0.80) ($H * 0.70)),
            (P ($W * 0.18) ($H * 0.68)) ) $rig $ink (2.4 * $SS) 0.3

Stroke $g (Fade $rigHi 170) ($W * 0.22) ($H * 0.46) ($W * 0.78) ($H * 0.49) (2.0 * $SS)
Stroke $g (Fade $ink 140) ($W * 0.22) ($H * 0.67) ($W * 0.78) ($H * 0.68) (2.2 * $SS)

foreach ($t in @(0.34, 0.62)) {
    Stroke $g $rigLo ($W * $t) ($H * 0.38) ($W * $t) ($H * 0.70) (3.2 * $SS)
    Stroke $g (Fade $rigHi 110) ($W * $t + 1.4 * $SS) ($H * 0.40) ($W * $t + 1.4 * $SS) ($H * 0.68) (1.0 * $SS)
}

# The valve and the hose, cut.
Disc $g $brass ($W * 0.84) ($H * 0.44) (5.0 * $SS)
Ring $g $rigLo ($W * 0.84) ($H * 0.44) (5.0 * $SS) (1.6 * $SS)
foreach ($i in 0..4) {
    Disc $g (Fade $rigLo 200) ($W * (0.86 + 0.026 * $i)) ($H * (0.40 - 0.03 * $i)) (2.2 * $SS)
}

# The recorder, propped against it, still on: one small light that is the
# whole reason to swim over here.
Shape $g @( (P ($W * 0.30) ($H * 0.70)), (P ($W * 0.52) ($H * 0.68)),
            (P ($W * 0.54) ($H * 0.86)), (P ($W * 0.32) ($H * 0.88)) ) $rigLo $ink (2.0 * $SS) 0.1
Stroke $g (Fade $rigHi 130) ($W * 0.32) ($H * 0.71) ($W * 0.52) ($H * 0.70) (1.2 * $SS)
Lamp $g ($W * 0.48) ($H * 0.81) (3.4 * $SS) $glass
Finish $bmp $g $outW $outH "gear_cache.png"

# =====================================================================
#  SHAFT  -  the way to the next floor. A rimmed hole with nothing in
#  it, and the frame somebody bolted round it before they went down.
# =====================================================================
$outW = 128; $outH = 128
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$frame   = C 116 124 132
$frameHi = C 176 188 196
$frameLo = C 58 64 70

# The rim, stepped, so the hole reads as a thing that was cut and dressed
# rather than a hole that was always there.
Disc $g $frameLo ($W * 0.5) ($H * 0.5) ($W * 0.46)
Ring $g $frame ($W * 0.5) ($H * 0.5) ($W * 0.42) (5.0 * $SS)
Ring $g (Fade $frameHi 160) ($W * 0.5) ($H * 0.5) ($W * 0.39) (1.8 * $SS)

# And the hole itself: nothing. Everything else in the game is lit from the
# top left; this is the one thing with no top at all.
Disc $g (C 2 4 6) ($W * 0.5) ($H * 0.5) ($W * 0.35)
foreach ($i in 0..3) {
    Ring $g (C 6 10 14 (90 - $i * 20)) ($W * 0.5) ($H * 0.5) ($W * (0.30 - 0.06 * $i)) (2.0 * $SS)
}

# Eight bolts round the frame, and four brackets at the quarters.
foreach ($i in 0..7) {
    $a = $i * [math]::PI / 4.0 + 0.2
    Disc $g $frameLo ($W * 0.5 + [math]::Cos($a) * $W * 0.435) ($H * 0.5 + [math]::Sin($a) * $H * 0.435) (2.6 * $SS)
    Disc $g (Fade $frameHi 180) ($W * 0.5 + [math]::Cos($a) * $W * 0.435 - 0.8 * $SS) ($H * 0.5 + [math]::Sin($a) * $H * 0.435 - 0.8 * $SS) (1.2 * $SS)
}

foreach ($i in 0..3) {
    $a = $i * [math]::PI / 2.0 + 0.78
    $bx = $W * 0.5 + [math]::Cos($a) * $W * 0.40
    $by = $H * 0.5 + [math]::Sin($a) * $H * 0.40
    Stroke $g $frame $bx $by ($W * 0.5 + [math]::Cos($a) * $W * 0.50) ($H * 0.5 + [math]::Sin($a) * $H * 0.50) (5.0 * $SS)
    Stroke $g (Fade $frameHi 130) $bx $by ($W * 0.5 + [math]::Cos($a) * $W * 0.49) ($H * 0.5 + [math]::Sin($a) * $H * 0.49) (1.6 * $SS)
}
Finish $bmp $g $outW $outH "gear_shaft.png"

# =====================================================================
#  VENT  -  a chimney the floor grew. Black rock furred with what only
#  lives on it, and a mouth that is dark until the moment it is not.
# =====================================================================
$outW = 96; $outH = 96
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$rock   = C 52 44 44
$rockHi = C 104 88 84
$rockLo = C 24 20 20
$fur    = C 190 176 158

# The stack, lopsided: a chimney builds up on whichever side the water is not
# taking it away from.
Shape $g @( (P ($W * 0.22) ($H * 0.92)), (P ($W * 0.32) ($H * 0.44)),
            (P ($W * 0.46) ($H * 0.24)), (P ($W * 0.62) ($H * 0.26)),
            (P ($W * 0.70) ($H * 0.50)), (P ($W * 0.82) ($H * 0.92)) ) $rock $ink (2.6 * $SS) 0.25

Shape $g @( (P ($W * 0.26) ($H * 0.90)), (P ($W * 0.34) ($H * 0.50)),
            (P ($W * 0.44) ($H * 0.30)), (P ($W * 0.48) ($H * 0.32)),
            (P ($W * 0.42) ($H * 0.56)), (P ($W * 0.38) ($H * 0.90)) ) (Fade $rockHi 120) $null 0 0.3

# The mouth, looking up and slightly towards the camera.
Shape $g @( (P ($W * 0.44) ($H * 0.26)), (P ($W * 0.56) ($H * 0.23)),
            (P ($W * 0.64) ($H * 0.30)), (P ($W * 0.52) ($H * 0.34)),
            (P ($W * 0.44) ($H * 0.31)) ) (C 14 8 8) $rockLo (2.0 * $SS) 0.3

# Tube worms, which only grow on these, so the thing is identifiable from
# across a basin by its fur rather than by its shape.
foreach ($i in 0..11) {
    $bx = $W * (0.20 + 0.052 * $i)
    $by = $H * (0.94 - 0.05 * [math]::Abs(5.5 - $i))
    $len = $H * (0.10 + 0.045 * (($i * 7) % 3))
    Stroke $g $fur $bx $by ($bx + $W * 0.02) ($by - $len) (2.2 * $SS)
    Disc $g (C 208 96 88) ($bx + $W * 0.02) ($by - $len) (1.8 * $SS)
}

Mottle $g @( @(($W * 0.30), ($H * 0.70), ($H * 0.22)),
             @(($W * 0.52), ($H * 0.45), ($H * 0.18)) ) $rockLo 18 313
Finish $bmp $g $outW $outH "gear_vent.png"
