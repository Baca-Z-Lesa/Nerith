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
# The leviathan's tail went with the rest of its textures: that animal is
# drawn from a pose now, fluke and all. What follows still needs one.

# The worm and the hollow are drawn from a pose now, tail and all. The
# coilworm is a creature rather than a boss, so it still wears one.


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
