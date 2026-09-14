Add-Type -AssemblyName System.Drawing

# Boss creatures. Each boss is drawn as body parts that belong to one animal:
# a head plus trailing segments, or a bell plus tentacles. Nothing here is a
# machine part bolted to a core.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }
function Disc($g, $col, $cx, $cy, $rad) { $g.FillEllipse((Brush $col), (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2))) }
function Ring($g, $col, $cx, $cy, $rad, $width) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, $width
    $g.DrawEllipse($pen, (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2)))
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

function GradBlob($g, $pts, $top, $bottom, $y0, $y1, $edge, $width) {
    $path = Curve $pts
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList `
        (P 0 $y0), (P 0 $y1), $top, $bottom
    $g.FillPath($brush, $path)
    if ($null -ne $edge) {
        $pen = New-Object System.Drawing.Pen -ArgumentList $edge, $width
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($pen, $path)
        $pen.Dispose()
    }
    $brush.Dispose(); $path.Dispose()
}

"generating boss creatures..."

# =====================================================================
#  LEVIATHAN  -  the great predator. Head plus trailing body segments.
# =====================================================================
$hide    = C 34 58 60
$belly   = C 74 104 100
$ink     = C 8 16 18
$ridge   = C 22 40 42

# The leviathan is drawn rather than stamped now - see BossArt - so its head,
# body, eye, plate and organ have no textures at all. The side fin stays,
# because the trench worm still bolts a pair of them onto itself.

# --- side fin, root at the left edge ------------------------------------
$outW = 104; $outH = 70
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
$fin = @( (P 10 ($cy - 44)), (P 190 ($cy - 96)), (P 380 ($cy - 40)), (P 300 ($cy + 40)), (P 10 ($cy + 48)) )
Blob $g $fin (C 44 72 74) $ink 8
foreach ($t in @(0.25, 0.5, 0.75)) {
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 26 46 48), 6.0
    $g.DrawLine($pen, 30, $cy, (30 + 300 * $t), ($cy - 66 * $t + 20 * $t))
    $pen.Dispose()
}
Finish $bmp $g $outW $outH "lev_fin.png"

# =====================================================================
#  MEDUSA  -  the bloom sovereign. A bell that pulses, with long tentacles.
# =====================================================================
$outW = 212; $outH = 212
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Disc $g (C 120 100 210 50)  $c $c 410
Disc $g (C 96 82 176 130)   $c $c 372
Disc $g (C 128 110 210 150) $c $c 312
Ring  $g (C 176 158 240 170) $c $c 340 12

# scalloped rim, so the edge is organic rather than a clean circle
for ($i = 0; $i -lt 18; $i++) {
    $a = 2 * [math]::PI * $i / 18.0
    Disc $g (C 150 130 226 120) ($c + [math]::Cos($a) * 372) ($c + [math]::Sin($a) * 372) 44
}

# radial canals running in to the middle
for ($i = 0; $i -lt 12; $i++) {
    $a = 2 * [math]::PI * $i / 12.0
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 196 178 250 150), 11.0
    $g.DrawLine($pen, ($c + [math]::Cos($a) * 80), ($c + [math]::Sin($a) * 80),
                      ($c + [math]::Cos($a) * 350), ($c + [math]::Sin($a) * 350))
    $pen.Dispose()
}
Disc $g (C 170 150 240 190) $c $c 96
Disc $g (C 226 210 255 220) $c $c 52
Finish $bmp $g $outW $outH "med_bell.png"

# --- tentacle, origin at the top centre ---------------------------------
$outW = 44; $outH = 200
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS / 2.0
$N = 26
$left = @(); $right = @()
for ($i = 0; $i -le $N; $i++) {
    $f = $i / [double]$N
    $x = $cx + [math]::Sin($f * 5.0) * 22 * $f
    $y = 14 + $f * ($outH * $SS - 28)
    $halfWidth = 26 * (1 - 0.82 * $f)
    $left  += ,(P ($x - $halfWidth) $y)
    $right += ,(P ($x + $halfWidth) $y)
}
[array]::Reverse($right)
Blob $g ($left + $right) (C 132 112 214 200) $null 0 0.25
# stinging nodes
for ($i = 3; $i -lt $N; $i += 4) {
    $f = $i / [double]$N
    $x = $cx + [math]::Sin($f * 5.0) * 22 * $f
    $y = 14 + $f * ($outH * $SS - 28)
    Disc $g (C 226 196 255 220) $x $y (13 * (1 - 0.6 * $f))
}
Finish $bmp $g $outW $outH "med_tentacle.png"

# --- nucleus ------------------------------------------------------------
$outW = 88; $outH = 88
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0
Disc $g (C 140 240 255 70)  $c $c 168
Disc $g (C 60 170 200 180)  $c $c 130
for ($i = 0; $i -lt 9; $i++) {
    $a = 2 * [math]::PI * $i / 9.0
    Disc $g (C 150 240 255 190) ($c + [math]::Cos($a) * 74) ($c + [math]::Sin($a) * 74) 26
}
Disc $g (C 200 250 255 230) $c $c 66
Disc $g (C 255 255 255)     $c $c 30
Finish $bmp $g $outW $outH "med_nucleus.png"

# =====================================================================
#  TRENCH WORM  -  armoured rings and a ring-toothed maw.
# =====================================================================
$chitin = C 62 54 44
$plate2 = C 88 78 62
$ink2   = C 12 10 8

$outW = 142; $outH = 124
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0

$hd = @( (P 552 $cy), (P 500 ($cy - 150)), (P 330 ($cy - 210)), (P 140 ($cy - 180)),
         (P 24 ($cy - 90)), (P 24 ($cy + 90)), (P 140 ($cy + 180)), (P 330 ($cy + 210)),
         (P 500 ($cy + 150)) )
GradBlob $g $hd $plate2 $chitin ($cy - 220) ($cy + 220) $ink2 10

# ring-toothed maw at the front
Disc $g $ink2 470 $cy 132
Disc $g (C 30 24 20) 470 $cy 112
for ($ringIndex = 0; $ringIndex -lt 3; $ringIndex++) {
    $rad = 104 - $ringIndex * 32
    $count = 14 - $ringIndex * 3
    for ($i = 0; $i -lt $count; $i++) {
        $a = 2 * [math]::PI * $i / $count + $ringIndex * 0.3
        Blob $g @(
            (P (470 + [math]::Cos($a - 0.13) * $rad) ($cy + [math]::Sin($a - 0.13) * $rad)),
            (P (470 + [math]::Cos($a) * ($rad - 30)) ($cy + [math]::Sin($a) * ($rad - 30))),
            (P (470 + [math]::Cos($a + 0.13) * $rad) ($cy + [math]::Sin($a + 0.13) * $rad))
        ) (C 216 208 190) $null 0 0.1
    }
}
Disc $g (C 120 30 30) 470 $cy 26
# armour ridges on the back of the skull
foreach ($rx in @(120, 210, 300)) {
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 34 30 24), 12.0
    $g.DrawArc($pen, (RectF ($rx - 60) ($cy - 160) 120 320), -76, 152)
    $pen.Dispose()
}
Finish $bmp $g $outW $outH "worm_head.png"

# --- armoured segment ---------------------------------------------------
$outW = 112; $outH = 112
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0
Disc $g $ink2 $c $c 208
Disc $g $chitin $c $c 194
Ring  $g (C 104 92 74) $c $c 150 16
Ring  $g (C 40 34 28) $c $c 106 14
for ($i = 0; $i -lt 10; $i++) {
    $a = 2 * [math]::PI * $i / 10.0
    Blob $g @(
        (P ($c + [math]::Cos($a - 0.2) * 150) ($c + [math]::Sin($a - 0.2) * 150)),
        (P ($c + [math]::Cos($a) * 206) ($c + [math]::Sin($a) * 206)),
        (P ($c + [math]::Cos($a + 0.2) * 150) ($c + [math]::Sin($a + 0.2) * 150))
    ) $plate2 $ink2 7 0.1
}
Disc $g (C 46 40 32) $c $c 82
Finish $bmp $g $outW $outH "worm_seg.png"

# --- glowing vent (weak point between the plates) -----------------------
$outW = 66; $outH = 66
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0
Disc $g (C 255 140 50 70)  $c $c 128
Disc $g (C 40 22 14)       $c $c 100
Disc $g (C 255 120 40 200) $c $c 76
Disc $g (C 255 190 110)    $c $c 46
Disc $g (C 255 244 210)    $c $c 20
Finish $bmp $g $outW $outH "worm_vent.png"

"done"
