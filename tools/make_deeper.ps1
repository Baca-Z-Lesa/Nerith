Add-Type -AssemblyName System.Drawing

# The two creatures below the abyss, and the two things that guard the water
# they live in. Kept in its own script so the deep roster can be retuned
# without touching the twenty levels above it.
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

"generating the deep roster..."

# =====================================================================
#  MOTH  -  all wing and no face. It is drawn facing right like every
#  other creature, but the wings are the whole silhouette: at speed the
#  player should read a shape coming at them, not an animal.
# =====================================================================
$wing   = C 128 116 150
$wingLo = C 72 62 96
$wingHi = C 178 168 198
$mink   = C 18 14 24

$outW = 76; $outH = 72
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

foreach ($side in @(-1, 1)) {
    # Forewing, swept back off the shoulder.
    Blob $g @( (P ($W * 0.62) ($cy + $side * 22)),
               (P ($W * 0.50) ($cy + $side * 128)),
               (P ($W * 0.22) ($cy + $side * 136)),
               (P ($W * 0.16) ($cy + $side * 52)) ) $wing $mink 7 0.4
    # Hindwing, smaller and lower.
    Blob $g @( (P ($W * 0.34) ($cy + $side * 26)),
               (P ($W * 0.22) ($cy + $side * 106)),
               (P ($W * 0.04) ($cy + $side * 92)),
               (P ($W * 0.06) ($cy + $side * 30)) ) $wingLo $mink 6 0.4

    # Ribs, so a wing is a wing and not a petal.
    foreach ($t in @(0.3, 0.6, 0.9)) {
        Stroke $g $wingLo ($W * 0.56) ($cy + $side * 30) `
            ($W * (0.52 - 0.28 * $t)) ($cy + $side * (52 + 72 * $t)) 5
    }
}

# The body: a narrow thorax with nothing on the front of it at all.
$body = @( (P ($W * 0.90) $cy), (P ($W * 0.64) ($cy - 32)),
           (P ($W * 0.20) ($cy - 26)), (P ($W * 0.04) $cy),
           (P ($W * 0.20) ($cy + 26)), (P ($W * 0.64) ($cy + 32)) )
GradBlob $g $body $wingHi $wingLo ($cy - 40) ($cy + 40) $mink 7

# Feathered antennae where the eyes would be on anything else. This is the
# whole character of it: it is not looking at you, it is smelling the lamp.
foreach ($side in @(-1, 1)) {
    Stroke $g $wingHi ($W * 0.84) ($cy + $side * 10) ($W * 0.99) ($cy + $side * 60) 7
    foreach ($t in @(0.35, 0.6, 0.85)) {
        $bx = $W * (0.84 + 0.15 * $t)
        $by = $cy + $side * (10 + 50 * $t)
        Stroke $g $wingHi $bx $by ($bx - 20) ($by + $side * 26) 4
        Stroke $g $wingHi $bx $by ($bx + 16) ($by + $side * 24) 4
    }
}
Finish $bmp $g $outW $outH "enemy_moth.png"

# =====================================================================
#  EFFIGY  -  a standing thing. Deliberately symmetric and deliberately
#  still: it has no direction of travel to draw, because most of the time
#  it is not travelling.
# =====================================================================
$stone   = C 118 122 108
$stoneLo = C 64 68 60
$stoneHi = C 166 170 152
$eink    = C 16 18 14

$outW = 68; $outH = 68
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $c = $W / 2.0

# A rough column of stacked plates, widest at the base.
foreach ($plate in @(@(0.86, 118), @(0.62, 104), @(0.40, 86), @(0.20, 62))) {
    $t = $plate[0]
    $half = $plate[1]
    $y = $H * (0.20 + 0.62 * $t)
    Blob $g @( (P ($c - $half) ($y - 46)), (P ($c + $half) ($y - 40)),
               (P ($c + $half - 10) ($y + 46)), (P ($c - $half + 14) ($y + 42)) ) `
        $stone $eink 7 0.15
}

# The head: one flat slab with a notch cut out of it.
Blob $g @( (P ($c - 74) ($H * 0.20)), (P ($c + 70) ($H * 0.18)),
           (P ($c + 62) ($H * 0.40)), (P ($c - 66) ($H * 0.42)) ) $stoneHi $eink 8 0.1
Blob $g @( (P ($c - 26) ($H * 0.19)), (P ($c + 24) ($H * 0.19)),
           (P ($c + 18) ($H * 0.33)), (P ($c - 20) ($H * 0.33)) ) $eink $null 0 0.05

# Arms folded in, because it is not reaching for anything yet.
foreach ($side in @(-1, 1)) {
    Stroke $g $stoneLo ($c + $side * 100) ($H * 0.46) ($c + $side * 52) ($H * 0.70) 26
}
Disc $g $stoneLo $c ($H * 0.86) 78
Finish $bmp $g $outW $outH "enemy_effigy.png"
# =====================================================================
#  What the two deep bosses throw.
#  The creatures themselves are drawn from a pose by BossArt and have no
#  textures at all any more; a shot is still a sprite, because a shot is
#  twenty pixels across and there are forty of them in the water.
# =====================================================================
$ember   = C 236 128 54
$emberHi = C 255 210 130
$vein    = C 126 84 96


# --- its shot: a cinder ----------------------------------------------
$outW = 24; $outH = 24
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0
Disc $g (C 255 120 40 90) $c $c ($c * 0.92)
Disc $g $ember $c $c ($c * 0.56)
Disc $g $emberHi $c $c ($c * 0.30)
Finish $bmp $g $outW $outH "shot_furnace.png"


# --- its shot: a seed ------------------------------------------------
$outW = 22; $outH = 22
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0
Blob $g @( (P ($c + $c * 0.80) $c), (P $c ($c - $c * 0.46)),
           (P ($c - $c * 0.70) $c), (P $c ($c + $c * 0.46)) ) (C 206 170 172) $null 0 0.3
Disc $g $vein $c $c ($c * 0.30)
Finish $bmp $g $outW $outH "shot_mother.png"
