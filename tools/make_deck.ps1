Add-Type -AssemblyName System.Drawing

# The surface. Everything you see between dives: the plate you walk on, the
# man walking on it, and the three things on the deck worth walking to.
#
# It is drawn top down like the rest of the game, but it is the only place in
# Nerith with a floor rather than a seabed, so it is all straight edges, rivets
# and rust. The water round it is drawn live by the deck itself - it moves, and
# a moving sea under a still rig is most of what says "surface" without a
# caption having to.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

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
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 2), ($outH * 2)))) {
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

"generating the deck..."

$steel   = C 86 92 96
$steelLo = C 62 68 72
$steelHi = C 116 122 124
$rust    = C 122 84 58
$grime   = C 46 50 54
$rubber  = C 44 42 46
$glass   = C 138 208 216
$ink     = C 18 18 20

# =====================================================================
#  PLATE  -  the floor. Has to meet itself on all four sides, so every
#  mark goes down nine times: once where it belongs, and once for each
#  way the plate can wrap. Folding a finished bitmap onto itself paints
#  each copy over the last and gives mush.
# =====================================================================
$tile = 96
$W = $tile * $SS; $H = $tile * $SS
$res = NewCanvas $W $H; $bmp = $res[0]; $g = $res[1]
Box $g $steel 0 0 $W $H

$rand = New-Object System.Random -ArgumentList 4471
$offsets = @(@(-$W, -$H), @(-$W, 0), @(-$W, $H), @(0, -$H), @(0, 0), @(0, $H), @($W, -$H), @($W, 0), @($W, $H))

# Treadplate: a diamond pattern, raised.
for ($row = 0; $row -lt 6; $row++) {
    for ($col = 0; $col -lt 6; $col++) {
        $cx = ($col + 0.5) * $W / 6.0
        $cy = ($row + 0.5) * $H / 6.0
        if ((($row + $col) % 2) -eq 0) { $dx = 22 } else { $dx = -22 }
        foreach ($o in $offsets) {
            Stroke $g $steelHi ($cx - $dx + $o[0]) ($cy - 20 + $o[1]) ($cx + $dx + $o[0]) ($cy + 20 + $o[1]) 9
            Stroke $g $steelLo ($cx - $dx + $o[0]) ($cy - 15 + $o[1]) ($cx + $dx + $o[0]) ($cy + 25 + $o[1]) 4
        }
    }
}

# Seams between plates.
foreach ($s in @(0.0, 0.5)) {
    $sx = $W * $s
    $sy = $H * $s
    foreach ($o in $offsets) {
        Stroke $g $grime ($sx + $o[0]) (-10 + $o[1]) ($sx + $o[0]) ($H + 10 + $o[1]) 6
        Stroke $g $grime (-10 + $o[0]) ($sy + $o[1]) ($W + 10 + $o[0]) ($sy + $o[1]) 6
    }
}

# Rivets along them.
for ($i = 0; $i -lt 30; $i++) {
    $rx = $rand.Next(0, $W); $ry = $rand.Next(0, $H)
    $rr = 5 + $rand.Next(0, 4)
    foreach ($o in $offsets) {
        Disc $g $steelHi ($rx + $o[0]) ($ry + $o[1]) $rr
        Disc $g $steelLo ($rx + $o[0]) ($ry + $o[1] - 1) ($rr * 0.55)
    }
}

# Rust, where the sea has been getting in. Sparingly: the tile is laid dozens
# of times across a deck, so a patch that reads as weathering on its own reads
# as carpet by the time the floor is finished.
for ($i = 0; $i -lt 7; $i++) {
    $rx = $rand.Next(0, $W); $ry = $rand.Next(0, $H)
    $rr = 12 + $rand.Next(0, 30)
    $a = 14 + $rand.Next(0, 22)
    foreach ($o in $offsets) {
        Disc $g (C $rust.R $rust.G $rust.B $a) ($rx + $o[0]) ($ry + $o[1]) $rr
    }
}
Finish $bmp $g $tile $tile "deck_plate.png"

# =====================================================================
#  DIVER  -  you, out of the sub. Drawn facing right like everything
#  else in the game, seen from above: helmet, shoulders, tank.
# =====================================================================
$suit   = C 168 146 62
$suitLo = C 110 92 38
$suitHi = C 214 196 104

$outW = 34; $outH = 30
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

# Seen from directly above, a man is mostly shoulders: the silhouette has to
# be wider across than it is long, or he reads as a barrel with a lid.

# The tank and its hose, behind everything and so drawn first.
Blob $g @( (P ($W * 0.30) ($cy - 44)), (P ($W * 0.12) ($cy - 38)),
           (P ($W * 0.10) ($cy + 38)), (P ($W * 0.30) ($cy + 44)) ) $rubber $ink 5 0.3
Stroke $g $steelHi ($W * 0.16) ($cy - 30) ($W * 0.26) ($cy - 30) 5
Stroke $g $steelHi ($W * 0.16) ($cy + 30) ($W * 0.26) ($cy + 30) 5
Stroke $g $rubber ($W * 0.20) ($cy - 42) ($W * 0.40) ($cy - 52) 8
Stroke $g $rubber ($W * 0.40) ($cy - 52) ($W * 0.62) ($cy - 30) 8

# Arms, held out from the body the way a hard suit forces them, ending in
# gloves that are a different colour so the ends of him are findable.
foreach ($side in @(-1, 1)) {
    Stroke $g $suit ($W * 0.42) ($cy + $side * 40) ($W * 0.62) ($cy + $side * 50) 17
    Stroke $g $suitLo ($W * 0.42) ($cy + $side * 44) ($W * 0.60) ($cy + $side * 54) 6
    Disc $g $rubber ($W * 0.66) ($cy + $side * 51) 11
    Ring $g $ink ($W * 0.66) ($cy + $side * 51) 11 4
}

# Shoulders: wide across, short along.
Blob $g @( (P ($W * 0.56) $cy), (P ($W * 0.48) ($cy - 46)),
           (P ($W * 0.26) ($cy - 42)), (P ($W * 0.22) $cy),
           (P ($W * 0.26) ($cy + 42)), (P ($W * 0.48) ($cy + 46)) ) $suit $ink 6 0.3
Stroke $g $suitHi ($W * 0.28) ($cy - 24) ($W * 0.50) ($cy - 26) 7
Stroke $g $suitLo ($W * 0.28) ($cy + 22) ($W * 0.50) ($cy + 24) 6

# The collar, and then the helmet sitting in it: narrower than the shoulders,
# which is the whole reason the shape reads as a man rather than a drum.
Disc $g $suitLo ($W * 0.60) $cy 30
Disc $g $suitHi ($W * 0.64) $cy 26
Ring $g $ink ($W * 0.64) $cy 26 5
Stroke $g $suitLo ($W * 0.64) ($cy - 26) ($W * 0.64) ($cy + 26) 4

# One window, in the front of it.
Blob $g @( (P ($W * 0.79) $cy), (P ($W * 0.70) ($cy - 15)),
           (P ($W * 0.63) ($cy - 12)), (P ($W * 0.63) ($cy + 12)),
           (P ($W * 0.70) ($cy + 15)) ) $glass $ink 4 0.2
Disc $g (C 255 255 255 130) ($W * 0.72) ($cy - 5) 5
Finish $bmp $g $outW $outH "deck_diver.png"

# --- a boot, drawn twice and swung, which is what makes him walk ------
$outW = 12; $outH = 9
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
Blob $g @( (P ($W * 0.92) ($H * 0.5)), (P ($W * 0.40) ($H * 0.10)),
           (P ($W * 0.06) ($H * 0.36)), (P ($W * 0.06) ($H * 0.64)),
           (P ($W * 0.40) ($H * 0.90)) ) $rubber $ink 4 0.3
Stroke $g $suitLo ($W * 0.18) ($H * 0.5) ($W * 0.62) ($H * 0.5) 5
Finish $bmp $g $outW $outH "deck_boot.png"

# =====================================================================
#  CRANE  -  where the hold gets sold. A winch drum, a jib out over the
#  side, a hook on the end of it.
# =====================================================================
$outW = 150; $outH = 112
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

Box $g $steelLo ($W * 0.04) ($cy - 74) ($W * 0.40) 148
foreach ($bx in @(0.08, 0.38)) {
    foreach ($by in @(-60, 60)) { Disc $g $steelHi ($W * $bx) ($cy + $by) 9 }
}

# The drum, with cable wound onto it.
Box $g $steel ($W * 0.10) ($cy - 52) ($W * 0.26) 104
for ($i = 0; $i -lt 9; $i++) {
    $x = $W * (0.115 + 0.028 * $i)
    Stroke $g $steelLo $x ($cy - 48) $x ($cy + 48) 5
}
Box $g $steelHi ($W * 0.09) ($cy - 58) ($W * 0.03) 116
Box $g $steelHi ($W * 0.34) ($cy - 58) ($W * 0.03) 116

# The jib, out over the water, and the cross bracing down it.
Blob $g @( (P ($W * 0.36) ($cy - 26)), (P ($W * 0.92) ($cy - 14)),
           (P ($W * 0.92) ($cy + 14)), (P ($W * 0.36) ($cy + 26)) ) $steel $ink 6 0.05
for ($i = 0; $i -lt 6; $i++) {
    $x0 = $W * (0.40 + 0.085 * $i)
    Stroke $g $steelLo $x0 ($cy - 20) ($x0 + $W * 0.08) ($cy + 20) 5
    Stroke $g $steelLo $x0 ($cy + 20) ($x0 + $W * 0.08) ($cy - 20) 5
}
Stroke $g $rust ($W * 0.90) $cy ($W * 0.97) $cy 7
Ring $g $ink ($W * 0.975) $cy 13 6
Finish $bmp $g $outW $outH "deck_crane.png"

# =====================================================================
#  BENCH  -  the workshop. Where the tree gets bought.
# =====================================================================
$outW = 146; $outH = 84
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Box $g $steelLo 0 ($H * 0.10) $W ($H * 0.80)
Box $g $steel ($W * 0.02) ($H * 0.14) ($W * 0.96) ($H * 0.68)
Stroke $g $steelHi ($W * 0.02) ($H * 0.20) ($W * 0.98) ($H * 0.20) 6

# A vice at one end.
Box $g $steelHi ($W * 0.06) ($H * 0.30) ($W * 0.14) ($H * 0.36)
Box $g $steelLo ($W * 0.14) ($H * 0.34) ($W * 0.05) ($H * 0.28)
Stroke $g $steelHi ($W * 0.19) ($H * 0.48) ($W * 0.27) ($H * 0.48) 9

# Tools laid out, and a part waiting to go on.
Stroke $g $rubber ($W * 0.34) ($H * 0.34) ($W * 0.50) ($H * 0.30) 11
Stroke $g $steelHi ($W * 0.50) ($H * 0.30) ($W * 0.57) ($H * 0.29) 7
Stroke $g $rubber ($W * 0.36) ($H * 0.62) ($W * 0.54) ($H * 0.66) 9
Disc $g $steelHi ($W * 0.56) ($H * 0.67) 12
Disc $g $glass ($W * 0.76) ($H * 0.46) 24
Ring $g $ink ($W * 0.76) ($H * 0.46) 24 5
Stroke $g $rust ($W * 0.86) ($H * 0.34) ($W * 0.92) ($H * 0.66) 10
Finish $bmp $g $outW $outH "deck_bench.png"

# =====================================================================
#  BELL  -  the way down. It sits in the moon pool; the hole in the
#  deck is drawn live underneath it, so this is only the bell.
# =====================================================================
$outW = 132; $outH = 132
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $c = $W / 2.0

Disc $g $steelLo $c $c ($c * 0.86)
Disc $g $steel $c $c ($c * 0.78)
Ring $g $ink $c $c ($c * 0.86) 7

for ($i = 0; $i -lt 8; $i++) {
    $a = [math]::PI * 2 * $i / 8.0
    Stroke $g $steelLo ($c + [math]::Cos($a) * $c * 0.24) ($c + [math]::Sin($a) * $c * 0.24) `
        ($c + [math]::Cos($a) * $c * 0.82) ($c + [math]::Sin($a) * $c * 0.82) 9
}

# The hatch on top, open.
Disc $g $steelHi $c $c ($c * 0.40)
Ring $g $ink $c $c ($c * 0.40) 6
Disc $g (C 12 20 26) $c $c ($c * 0.30)
Disc $g (C 40 92 104 150) $c ($c * 0.94) ($c * 0.14)

# Four ports round the waist, lit from inside.
for ($i = 0; $i -lt 4; $i++) {
    $a = [math]::PI * 2 * $i / 4.0 + 0.4
    $px = $c + [math]::Cos($a) * $c * 0.58
    $py = $c + [math]::Sin($a) * $c * 0.58
    Disc $g $steelHi $px $py 20
    Disc $g $glass $px $py 13
    Ring $g $ink $px $py 20 5
}
Finish $bmp $g $outW $outH "deck_bell.png"

# =====================================================================
#  CRATE  -  dressing, and what the hold gets stacked into.
# =====================================================================
$outW = 46; $outH = 46
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS
Box $g $steelLo 0 0 $W $W
Box $g $steel ($W * 0.06) ($W * 0.06) ($W * 0.88) ($W * 0.88)
Stroke $g $rust ($W * 0.22) 0 ($W * 0.22) $W 12
Stroke $g $rust ($W * 0.78) 0 ($W * 0.78) $W 12
Stroke $g $steelHi ($W * 0.06) ($W * 0.30) ($W * 0.94) ($W * 0.30) 5
Disc $g $steelHi ($W * 0.50) ($W * 0.62) 16
Ring $g $ink ($W * 0.50) ($W * 0.62) 16 4
Finish $bmp $g $outW $outH "deck_crate.png"

# =====================================================================
#  RAIL  -  the edge of the deck. Tiles left to right.
# =====================================================================
$outW = 64; $outH = 20
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
Box $g $steelHi 0 ($H * 0.10) $W ($H * 0.22)
Box $g $steel 0 ($H * 0.52) $W ($H * 0.14)
foreach ($x in @(0.0, 0.5)) {
    $px = $W * $x
    Box $g $steelLo ($px - 7) ($H * 0.10) 14 ($H * 0.88)
}
Finish $bmp $g $outW $outH "deck_rail.png"
