Add-Type -AssemblyName System.Drawing

# The cutscene plates.
#
# The two reels used to be drawn out of triangles, which is fine for a beam of
# light and hopeless for a rig, a station or an animal the size of a station.
# These are painted once and then moved, panned and lit by the reel - which is
# what a shot is: something drawn well, and a camera that does not hold still.
#
# Everything here is in profile. The game is top down, so a side view is the
# cheapest signal there is that what you are watching is not the game.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 3

function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }
function Disc($g, $col, $cx, $cy, $rad) { $g.FillEllipse((Brush $col), (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2))) }
function Box($g, $col, $x, $y, $w, $h) { $g.FillRectangle((Brush $col), (RectF $x $y $w $h)) }
function Ring($g, $col, $cx, $cy, $rad, $width) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, $width
    $g.DrawEllipse($pen, (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2)))
    $pen.Dispose()
}
function Stroke($g, $col, $x0, $y0, $x1, $y1, $width) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, $width
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($pen, [float]$x0, [float]$y0, [float]$x1, [float]$y1)
    $pen.Dispose()
}

function NewCanvas($w, $h) {
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    return @($bmp, $g)
}

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, $outW, $outH))) {
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

function Curve($pts, $tension = 0.5) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, $tension)
    return $path
}

function Blob($g, $pts, $fill, $edge, $width, $tension = 0.5) {
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

"generating the reel plates..."

$steel   = C 78 86 92
$steelLo = C 44 50 56
$steelHi = C 122 132 138
$rust    = C 104 72 50
$ink     = C 12 16 19
$lit     = C 236 196 120
$cold    = C 128 202 214
$rock    = C 38 44 48
$rockLo  = C 22 27 30
$rockHi  = C 68 78 82
$meat    = C 44 40 52
$meatHi  = C 86 78 100

# =====================================================================
#  RIG  -  the thing on the surface, in profile. A deck on four legs
#  with a derrick over the moon pool and one crane hanging off the side.
#  It is the first and the last thing in the game.
# =====================================================================
$outW = 460; $outH = 230
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$deck = $H * 0.52

# Legs, going down out of the frame.
foreach ($x in @(0.14, 0.34, 0.66, 0.86)) {
    Box $g $steelLo ($W * $x - 13) $deck 26 ($H - $deck)
    Stroke $g $steel ($W * $x - 10) ($deck + 20) ($W * $x + 10) ($H - 10) 6
}
# Cross bracing between them.
foreach ($pair in @(@(0.14, 0.34), @(0.34, 0.66), @(0.66, 0.86))) {
    Stroke $g $steelLo ($W * $pair[0]) ($deck + 60) ($W * $pair[1]) ($H - 30) 9
    Stroke $g $steelLo ($W * $pair[1]) ($deck + 60) ($W * $pair[0]) ($H - 30) 9
}

# The deck itself.
Box $g $steel ($W * 0.06) ($deck - 34) ($W * 0.88) 34
Box $g $steelHi ($W * 0.06) ($deck - 38) ($W * 0.88) 7
Box $g $rust ($W * 0.20) ($deck - 2) ($W * 0.16) 8

# Housing: a block of cabins with the lights still on.
Box $g $steelLo ($W * 0.12) ($deck - 96) ($W * 0.24) 62
foreach ($i in 0..4) {
    Box $g $lit ($W * (0.145 + 0.045 * $i)) ($deck - 82) 16 14
}
Box $g $steel ($W * 0.12) ($deck - 102) ($W * 0.24) 8

# The derrick over the middle, which is where the bell goes over.
Stroke $g $steel ($W * 0.44) $deck ($W * 0.50) ($deck - 150) 11
Stroke $g $steel ($W * 0.60) $deck ($W * 0.54) ($deck - 150) 11
foreach ($t in @(0.2, 0.4, 0.6, 0.8)) {
    $y0 = $deck - 150 * $t
    Stroke $g $steelLo ($W * (0.44 + 0.06 * $t)) $y0 ($W * (0.60 - 0.06 * $t)) $y0 7
}
Box $g $steelHi ($W * 0.485) ($deck - 162) ($W * 0.075) 14
Disc $g $lit ($W * 0.52) ($deck - 176) 9

# The crane off the right hand side, and its cable.
Stroke $g $steel ($W * 0.78) ($deck - 12) ($W * 0.78) ($deck - 96) 13
Stroke $g $steel ($W * 0.78) ($deck - 96) ($W * 0.96) ($deck - 62) 11
Stroke $g $steelLo ($W * 0.96) ($deck - 62) ($W * 0.96) ($deck + 30) 4
Disc $g $lit ($W * 0.785) ($deck - 104) 7
Finish $bmp $g $outW $outH "reel_rig.png"

# =====================================================================
#  BELL  -  in profile this time, on the end of a cable. The thing the
#  player is inside for both of the rides that bracket the game.
# =====================================================================
$outW = 180; $outH = 210
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $c = $W / 2.0

# Cable and the yoke it hangs in.
Stroke $g $steelLo $c 0 $c ($H * 0.20) 10
Stroke $g $steel ($c - 70) ($H * 0.30) $c ($H * 0.19) 8
Stroke $g $steel ($c + 70) ($H * 0.30) $c ($H * 0.19) 8

# The sphere.
Disc $g $steelLo $c ($H * 0.56) ($W * 0.42)
Disc $g $steel $c ($H * 0.56) ($W * 0.38)
Ring $g $ink $c ($H * 0.56) ($W * 0.42) 6

# A band round its waist, and the ports in it.
Box $g $steelHi ($c - $W * 0.42) ($H * 0.53) ($W * 0.84) 16
foreach ($x in @(-0.24, 0.0, 0.24)) {
    Disc $g $steelHi ($c + $W * $x) ($H * 0.44) 26
    Disc $g $cold ($c + $W * $x) ($H * 0.44) 17
    Ring $g $ink ($c + $W * $x) ($H * 0.44) 26 5
}

# Skids under it, and the lamp on the front.
Stroke $g $steelLo ($c - $W * 0.30) ($H * 0.84) ($c + $W * 0.30) ($H * 0.84) 12
Stroke $g $steelLo ($c - $W * 0.20) ($H * 0.78) ($c - $W * 0.24) ($H * 0.84) 9
Stroke $g $steelLo ($c + $W * 0.20) ($H * 0.78) ($c + $W * 0.24) ($H * 0.84) 9
Disc $g $lit $c ($H * 0.70) 14
Finish $bmp $g $outW $outH "reel_bell.png"

# =====================================================================
#  WALL  -  rock face, tiling top to bottom, for everything that falls
#  or climbs. Only the vertical seam has to meet, so the marks are drawn
#  three times rather than nine.
# =====================================================================
$outW = 300; $outH = 400
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$rand = New-Object System.Random -ArgumentList 9931

Box $g $rockLo 0 0 $W $H

# Ledges. The whole job of this plate is that something is going past at
# speed, and speed is read off edges: a shelf with a lit top and a black
# underside says "falling" in a way a gradient never does.
# PowerShell variables are case insensitive, so a local called $w in here
# would quietly become the canvas width and every ledge after the first one
# would be drawn inside a box the size of the last ledge. Hence $span.
for ($i = 0; $i -lt 26; $i++) {
    $x = $rand.Next(-100, $W)
    $y = $rand.Next(0, $H)
    $span = 180 + $rand.Next(0, 420)
    $tall = 50 + $rand.Next(0, 120)

    foreach ($dy in @(-$H, 0, $H)) {
        # The mass.
        Blob $g @( (P ($x - $span * 0.5) ($y + $tall * 0.7 + $dy)),
                   (P ($x - $span * 0.3) ($y - $tall * 0.5 + $dy)),
                   (P ($x + $span * 0.1) ($y - $tall * 0.7 + $dy)),
                   (P ($x + $span * 0.5) ($y + $tall * 0.2 + $dy)),
                   (P ($x + $span * 0.2) ($y + $tall * 0.9 + $dy)) ) $rock $null 0 0.25
        # The top of it, catching whatever light there is.
        Stroke $g $rockHi ($x - $span * 0.30) ($y - $tall * 0.46 + $dy) `
            ($x + $span * 0.10) ($y - $tall * 0.64 + $dy) 11
        Stroke $g $rockHi ($x + $span * 0.10) ($y - $tall * 0.64 + $dy) `
            ($x + $span * 0.46) ($y + $tall * 0.16 + $dy) 9
        # And the black under it.
        Stroke $g (C 0 0 0 170) ($x - $span * 0.46) ($y + $tall * 0.72 + $dy) `
            ($x + $span * 0.22) ($y + $tall * 0.92 + $dy) 16
    }
}

# Cracks, which are the only straight lines on the plate.
for ($i = 0; $i -lt 18; $i++) {
    $x = $rand.Next(0, $W)
    $y = $rand.Next(0, $H)
    $len = 60 + $rand.Next(0, 200)
    $lean = $rand.Next(-60, 60)
    foreach ($dy in @(-$H, 0, $H)) {
        Stroke $g (C 0 0 0 150) $x ($y + $dy) ($x + $lean) ($y + $len + $dy) 5
    }
}

# A darker wash down both sides so the plate can be laid next to itself
# across the screen without a hard vertical edge showing.
for ($i = 0; $i -lt 26; $i++) {
    $a = [int](2 + $i * 0.5)
    Box $g (C 0 0 0 $a) ($i * 4) 0 4 $H
    Box $g (C 0 0 0 $a) ($W - ($i + 1) * 4) 0 4 $H
}
Finish $bmp $g $outW $outH "reel_wall.png"

# =====================================================================
#  STATION  -  Nerith itself, in profile: three modules on legs, a mast,
#  and one window still lit. Everything about it has to say "intact, and
#  nobody is in it".
# =====================================================================
$outW = 560; $outH = 260
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$floor = $H * 0.86

# Legs and pads.
foreach ($x in @(0.16, 0.32, 0.62, 0.82)) {
    Stroke $g $steelLo ($W * $x) ($H * 0.58) ($W * ($x + 0.02)) $floor 14
    Box $g $steelLo ($W * $x - 22) ($floor - 8) 60 16
}

# The big module, lying on its side.
Blob $g @( (P ($W * 0.10) ($H * 0.50)), (P ($W * 0.18) ($H * 0.30)),
           (P ($W * 0.46) ($H * 0.28)), (P ($W * 0.54) ($H * 0.50)),
           (P ($W * 0.46) ($H * 0.62)), (P ($W * 0.18) ($H * 0.62)) ) $steel $ink 7 0.2
foreach ($i in 0..5) {
    $x = $W * (0.16 + 0.06 * $i)
    Stroke $g $steelLo $x ($H * 0.31) $x ($H * 0.61) 5
}
# Its windows: all dark but one.
foreach ($i in 0..4) {
    $x = $W * (0.19 + 0.06 * $i)
    Disc $g $steelHi $x ($H * 0.44) 17
    if ($i -eq 3) { Disc $g $lit $x ($H * 0.44) 11 } else { Disc $g (C 16 20 24) $x ($H * 0.44) 11 }
}

# A second, smaller module, and the gantry to it.
Blob $g @( (P ($W * 0.62) ($H * 0.52)), (P ($W * 0.66) ($H * 0.36)),
           (P ($W * 0.84) ($H * 0.34)), (P ($W * 0.88) ($H * 0.52)),
           (P ($W * 0.84) ($H * 0.62)), (P ($W * 0.66) ($H * 0.62)) ) $steelLo $ink 7 0.2
Box $g $steel ($W * 0.54) ($H * 0.46) ($W * 0.08) 20
Disc $g (C 16 20 24) ($W * 0.75) ($H * 0.47) 22
Ring $g $steelHi ($W * 0.75) ($H * 0.47) 22 6

# The mast, with the beacon dead on top of it.
Stroke $g $steel ($W * 0.40) ($H * 0.30) ($W * 0.42) ($H * 0.06) 9
Stroke $g $steelLo ($W * 0.40) ($H * 0.22) ($W * 0.33) ($H * 0.13) 5
Stroke $g $steelLo ($W * 0.41) ($H * 0.16) ($W * 0.50) ($H * 0.09) 5
Disc $g $rust ($W * 0.42) ($H * 0.05) 12

# An open hatch, hanging.
Stroke $g $steelHi ($W * 0.26) ($H * 0.62) ($W * 0.24) ($H * 0.74) 10
Finish $bmp $g $outW $outH "reel_station.png"

# =====================================================================
#  BEAST  -  the thing that goes past. Never lit, never explained: it is
#  a silhouette the length of the screen with one eye that catches the
#  lamp, which is worth more than any amount of detail.
# =====================================================================
$outW = 760; $outH = 280
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H * 0.52

# The body: a long taper, head at the right.
Blob $g @( (P ($W * 0.98) $cy),
           (P ($W * 0.86) ($cy - $H * 0.26)),
           (P ($W * 0.60) ($cy - $H * 0.30)),
           (P ($W * 0.30) ($cy - $H * 0.20)),
           (P ($W * 0.02) ($cy - $H * 0.06)),
           (P ($W * 0.04) ($cy + $H * 0.05)),
           (P ($W * 0.30) ($cy + $H * 0.22)),
           (P ($W * 0.62) ($cy + $H * 0.31)),
           (P ($W * 0.86) ($cy + $H * 0.25)) ) $meat $ink 8 0.4

# A dorsal line of low spines.
for ($i = 0; $i -lt 12; $i++) {
    $t = 0.12 + 0.07 * $i
    $x = $W * $t
    $lift = $H * (0.30 - 0.20 * [math]::Abs($t - 0.6))
    Blob $g @( (P ($x - 26) ($cy - $lift * 0.72)),
               (P $x ($cy - $lift - 30)),
               (P ($x + 26) ($cy - $lift * 0.72)) ) $meatHi $null 0 0.2
}

# Fins, low and long.
Blob $g @( (P ($W * 0.56) ($cy + $H * 0.24)), (P ($W * 0.44) ($cy + $H * 0.46)),
           (P ($W * 0.30) ($cy + $H * 0.44)), (P ($W * 0.36) ($cy + $H * 0.20)) ) $meatHi $ink 6 0.3
Blob $g @( (P ($W * 0.84) ($cy + $H * 0.20)), (P ($W * 0.78) ($cy + $H * 0.42)),
           (P ($W * 0.66) ($cy + $H * 0.38)), (P ($W * 0.70) ($cy + $H * 0.18)) ) $meatHi $ink 6 0.3

# The head, and the one eye.
Blob $g @( (P ($W * 0.99) ($cy - $H * 0.02)), (P ($W * 0.92) ($cy - $H * 0.22)),
           (P ($W * 0.80) ($cy - $H * 0.20)), (P ($W * 0.80) ($cy + $H * 0.20)),
           (P ($W * 0.92) ($cy + $H * 0.22)) ) $meatHi $ink 7 0.3
Disc $g $ink ($W * 0.895) ($cy - $H * 0.06) 30
Disc $g $lit ($W * 0.895) ($cy - $H * 0.06) 19
Disc $g $ink ($W * 0.895) ($cy - $H * 0.06) 8

# And the mouth line, shut.
Stroke $g $ink ($W * 0.80) ($cy + $H * 0.08) ($W * 0.985) ($cy + $H * 0.02) 7
Finish $bmp $g $outW $outH "reel_beast.png"

# =====================================================================
#  DIVER  -  a man in a hard suit, in profile, drifting. Used for what
#  is left of the ones who came before.
# =====================================================================
$outW = 150; $outH = 220
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$suit   = C 128 112 54
$suitLo = C 82 70 32
$rubber = C 40 38 42

# Legs, hanging.
Stroke $g $suitLo ($W * 0.44) ($H * 0.62) ($W * 0.36) ($H * 0.90) 40
Stroke $g $suitLo ($W * 0.58) ($H * 0.62) ($W * 0.62) ($H * 0.92) 40
Stroke $g $rubber ($W * 0.34) ($H * 0.90) ($W * 0.24) ($H * 0.95) 30
Stroke $g $rubber ($W * 0.62) ($H * 0.92) ($W * 0.72) ($H * 0.96) 30

# Body and tank. Broad: a hard suit is a barrel with a man in it, and the
# shoulders have to be wider than the helmet or he reads as a doll.
Blob $g @( (P ($W * 0.76) ($H * 0.40)), (P ($W * 0.70) ($H * 0.66)),
           (P ($W * 0.30) ($H * 0.66)), (P ($W * 0.24) ($H * 0.40)),
           (P ($W * 0.50) ($H * 0.33)) ) $suit $ink 7 0.25
Box $g $rubber ($W * 0.16) ($H * 0.40) ($W * 0.13) ($H * 0.24)
Stroke $g $suitLo ($W * 0.30) ($H * 0.52) ($W * 0.70) ($H * 0.52) 9

# Arms, one out.
Stroke $g $suit ($W * 0.36) ($H * 0.44) ($W * 0.14) ($H * 0.54) 24
Stroke $g $rubber ($W * 0.14) ($H * 0.54) ($W * 0.07) ($H * 0.58) 19
Stroke $g $suit ($W * 0.66) ($H * 0.44) ($W * 0.84) ($H * 0.60) 24
Stroke $g $rubber ($W * 0.84) ($H * 0.60) ($W * 0.90) ($H * 0.66) 19

# The collar, then the helmet in it: small, and sunk into the shoulders.
Box $g $suitLo ($W * 0.36) ($H * 0.31) ($W * 0.28) ($H * 0.05)
Blob $g @( (P ($W * 0.66) ($H * 0.24)), (P ($W * 0.62) ($H * 0.12)),
           (P ($W * 0.42) ($H * 0.12)), (P ($W * 0.36) ($H * 0.24)),
           (P ($W * 0.42) ($H * 0.32)), (P ($W * 0.62) ($H * 0.32)) ) $suit $ink 6 0.3
Stroke $g $suitLo ($W * 0.44) ($H * 0.16) ($W * 0.60) ($H * 0.16) 7

# One port, in the front of it, and a hose over the shoulder.
Blob $g @( (P ($W * 0.665) ($H * 0.22)), (P ($W * 0.62) ($H * 0.155)),
           (P ($W * 0.55) ($H * 0.155)), (P ($W * 0.55) ($H * 0.275)),
           (P ($W * 0.62) ($H * 0.275)) ) (C 20 26 30) $ink 5 0.2
Disc $g $cold ($W * 0.615) ($H * 0.20) ($W * 0.028)
Stroke $g $rubber ($W * 0.26) ($H * 0.40) ($W * 0.36) ($H * 0.30) 9
Finish $bmp $g $outW $outH "reel_diver.png"
