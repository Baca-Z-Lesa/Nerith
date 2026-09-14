Add-Type -AssemblyName System.Drawing

# Tails. Everything in this game that is built out of a head plus a chain of
# links used to stop rather than end: the last ring was the same ring as all
# the others, so a leviathan looked like something that had been cut in half
# and a coilworm looked like a string of beads that ran out.
#
# Each of these is drawn facing +X, because that is the way a body link is
# turned: a link points at the one ahead of it, so the root of a tail is on
# its right and the fluke trails off to its left.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }
function Disc($g, $col, $cx, $cy, $rad) { $g.FillEllipse((Brush $col), (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2))) }
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

"generating tails..."

# =====================================================================
#  LEVIATHAN  -  the same hide as its body, narrowing to a peduncle and
#  opening again into a two lobed fluke.
# =====================================================================
$hide  = C 34 58 60
$belly = C 74 104 100
$ink   = C 8 16 18
$ridge = C 22 40 42

$outW = 124; $outH = 104
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $cy = $outH * $SS / 2.0

# The body carrying on out of the last link and thinning as it goes.
$stalk = @( (P ($W * 0.98) ($cy - 128)), (P ($W * 0.62) ($cy - 94)),
            (P ($W * 0.34) ($cy - 38)), (P ($W * 0.34) ($cy + 38)),
            (P ($W * 0.62) ($cy + 94)), (P ($W * 0.98) ($cy + 128)) )
GradBlob $g $stalk $hide $belly ($cy - 150) ($cy + 150) $ink 9

# The fluke: two lobes off the end, swept back the way a tail trails.
foreach ($side in @(-1, 1)) {
    Blob $g @( (P ($W * 0.36) ($cy + $side * 24)),
               (P ($W * 0.20) ($cy + $side * 118)),
               (P ($W * 0.04) ($cy + $side * 164)),
               (P ($W * 0.03) ($cy + $side * 94)),
               (P ($W * 0.22) ($cy + $side * 14)) ) $hide $ink 8 0.45
    Stroke $g $ridge ($W * 0.32) ($cy + $side * 20) ($W * 0.08) ($cy + $side * 128) 7
}

# The grain of the hide running back into the notch, so the stalk does not
# read as a flat wedge. Lines rather than plates: the ridge plates on a body
# link sit on a back that is still wide, and there is no back left out here.
foreach ($t in @(0.30, 0.55, 0.80)) {
    Stroke $g $ridge ($W * 0.92) ($cy - 96 * $t + 48 * $t) ($W * 0.36) ($cy - 26 * $t) 7
    Stroke $g $ridge ($W * 0.92) ($cy + 96 * $t - 48 * $t) ($W * 0.36) ($cy + 26 * $t) 7
}
Finish $bmp $g $outW $outH "lev_tail.png"

# =====================================================================
#  TRENCH WORM  -  no fluke on a worm. It narrows through a few more
#  rings and finishes on a point, with the last pair of parapodia small
#  enough to read as the end rather than as another link.
# =====================================================================
$grub   = C 118 74 46
$grubLo = C 64 38 24

$outW = 116; $outH = 116
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $cy = $outH * $SS / 2.0

foreach ($side in @(-1, 1)) {
    Blob $g @( (P ($W * 0.74) ($cy + $side * 28)),
               (P ($W * 0.66) ($cy + $side * 126)),
               (P ($W * 0.50) ($cy + $side * 148)),
               (P ($W * 0.54) ($cy + $side * 58)) ) $grubLo (C 40 24 16) 6 0.3
}

$cone = @( (P ($W * 0.97) ($cy - 154)), (P ($W * 0.60) ($cy - 118)),
           (P ($W * 0.22) ($cy - 42)), (P ($W * 0.03) $cy),
           (P ($W * 0.22) ($cy + 42)), (P ($W * 0.60) ($cy + 118)),
           (P ($W * 0.97) ($cy + 154)) )
Blob $g $cone $grubLo $null 0 0.3

$inner = @( (P ($W * 0.95) ($cy - 130)), (P ($W * 0.58) ($cy - 98)),
            (P ($W * 0.24) ($cy - 32)), (P ($W * 0.09) $cy),
            (P ($W * 0.24) ($cy + 32)), (P ($W * 0.58) ($cy + 98)),
            (P ($W * 0.95) ($cy + 130)) )
Blob $g $inner $grub $null 0 0.3

# The rings that make it a worm rather than a tube, tightening to the tip.
foreach ($t in @(0.86, 0.70, 0.54, 0.38, 0.24)) {
    $half = 150 * $t * $t + 12
    Stroke $g (C 74 44 28 180) ($W * $t) ($cy - $half) ($W * $t) ($cy + $half) 8
}
Finish $bmp $g $outW $outH "worm_tail.png"

# =====================================================================
#  THE HOLLOW  -  it is a column of bone, so the end of it is the last
#  vertebra drawn out into a spine.
# =====================================================================
$bone   = C 156 166 152
$boneLo = C 96 108 104
$boneInk = C 10 14 16

$outW = 96; $outH = 96
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $cy = $outH * $SS / 2.0

$spine = @( (P ($W * 0.92) ($cy - 114)), (P ($W * 0.52) ($cy - 90)),
            (P ($W * 0.06) $cy),
            (P ($W * 0.52) ($cy + 90)), (P ($W * 0.92) ($cy + 114)) )
Blob $g $spine $boneLo $boneInk 7 0.35

$spineIn = @( (P ($W * 0.88) ($cy - 84)), (P ($W * 0.52) ($cy - 62)),
              (P ($W * 0.15) $cy),
              (P ($W * 0.52) ($cy + 62)), (P ($W * 0.88) ($cy + 84)) )
Blob $g $spineIn $bone $null 0 0.35

# Spines off the edge rather than plates cut into it, the same way a live
# joint carries them - shorter here, because this is the end of the column.
foreach ($t in @(0.74, 0.54, 0.34)) {
    foreach ($side in @(-1, 1)) {
        $edgeY = $cy + $side * (84 * $t + 6)
        Stroke $g $boneLo ($W * $t) $edgeY ($W * $t - 16) ($edgeY + $side * (46 * $t + 10)) 11
    }
}
Disc $g (C 8 12 14 200) ($W * 0.80) $cy 32
Finish $bmp $g $outW $outH "hol_tail.png"

# =====================================================================
#  COILWORM  -  the body it tows is rings, and rings have to stop
#  somewhere. This is where.
# =====================================================================
$chitin   = C 118 86 58
$chitinLo = C 62 42 28
$chitinHi = C 170 134 96

$outW = 40; $outH = 40
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $cy = $outH * $SS / 2.0

$wedge = @( (P ($W * 0.92) ($cy - 70)), (P ($W * 0.52) ($cy - 50)),
            (P ($W * 0.10) $cy),
            (P ($W * 0.52) ($cy + 50)), (P ($W * 0.92) ($cy + 70)) )
Blob $g $wedge $chitinLo $null 0 0.3

$wedgeIn = @( (P ($W * 0.88) ($cy - 54)), (P ($W * 0.52) ($cy - 36)),
              (P ($W * 0.21) $cy),
              (P ($W * 0.52) ($cy + 36)), (P ($W * 0.88) ($cy + 54)) )
Blob $g $wedgeIn $chitin $null 0 0.3
Disc $g $chitinHi ($W * 0.74) ($cy - 13) 21

# Two small flukes, so it ends on an animal and not on a spike.
foreach ($side in @(-1, 1)) {
    Blob $g @( (P ($W * 0.34) ($cy + $side * 9)),
               (P ($W * 0.14) ($cy + $side * 60)),
               (P ($W * 0.04) ($cy + $side * 28)),
               (P ($W * 0.22) ($cy + $side * 4)) ) $chitinLo $null 0 0.4
}
foreach ($t in @(0.76, 0.56)) {
    $half = 70 * $t * $t + 7
    Stroke $g $chitinLo ($W * $t) ($cy - $half) ($W * $t) ($cy + $half) 6
}
Finish $bmp $g $outW $outH "enemy_coil_tail.png"
